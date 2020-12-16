#include <bonjour/bonjourbrowserwrapper.h>
#include <utils/Logger.h>

//qt includes
#include <QTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRegularExpression>

// bonjour
#include <bonjour/bonjourservicebrowser.h>
#include <bonjour/bonjourserviceresolver.h>

#include <chrono>

namespace {

const char BONJOUR_HYPERION_SERVICES_TYPE[] = "_hyperiond-http._tcp";
//const char BONJOUR_HYPERION_SERVICES_TYPE[] = "_wled._tcp";

constexpr std::chrono::milliseconds DEFAULT_BROWSE_INTERVAL{1000}; // in ms

} //End of constants

BonjourBrowserWrapper* BonjourBrowserWrapper::instance = nullptr;

BonjourBrowserWrapper::BonjourBrowserWrapper(QObject * parent)
	: QObject(parent)
	, _bonjourResolver(new BonjourServiceResolver(this))
	, _timerBonjourResolver(new QTimer(this))
{
	// register meta
	qRegisterMetaType<QMap<QString,BonjourRecord>>("QMap<QString,BonjourRecord>");

	BonjourBrowserWrapper::instance = this;

	connect(_bonjourResolver, &BonjourServiceResolver::bonjourRecordResolved, this, &BonjourBrowserWrapper::bonjourRecordResolved);

	connect(_timerBonjourResolver, &QTimer::timeout, this, &BonjourBrowserWrapper::bonjourResolve);
	_timerBonjourResolver->setInterval(DEFAULT_BROWSE_INTERVAL.count());
	_timerBonjourResolver->start();

	// browse for _hyperiond-http._tcp
	browseForServiceType(QLatin1String(BONJOUR_HYPERION_SERVICES_TYPE));
}

bool BonjourBrowserWrapper::browseForServiceType(const QString &serviceType)
{
	if(!_browsedServiceTypes.contains(serviceType))
	{
		BonjourServiceBrowser* newBrowser = new BonjourServiceBrowser(this);
		connect(newBrowser, &BonjourServiceBrowser::currentBonjourRecordsChanged, this, &BonjourBrowserWrapper::currentBonjourRecordsChanged);
		newBrowser->browseForServiceType(serviceType);
		_browsedServiceTypes.insert(serviceType, newBrowser);
		return true;
	}
	return false;
}

void BonjourBrowserWrapper::currentBonjourRecordsChanged(const QList<BonjourRecord> &list)
{
	for ( auto rec : list )
	{
		char fullName [kDNSServiceMaxDomainName];
		DNSServiceConstructFullName	(fullName,rec.serviceName.toLocal8Bit().data(),rec.registeredType.toLocal8Bit().data(), rec.replyDomain.toLocal8Bit().data());

		rec.fullName = fullName;
		if ( !_services.contains(fullName) )
		{
			Debug(Logger::getInstance("BonJour"), "Service added [%s:%s], fullname [%s]", QSTRING_CSTR(rec.registeredType), QSTRING_CSTR(rec.serviceName), fullName);
			_services.insert(fullName, rec);
		}
		else
		{
			Debug(Logger::getInstance("BonJour"), "Service already exists [%s:%s], fullname [%s]", QSTRING_CSTR(rec.registeredType), QSTRING_CSTR(rec.serviceName), fullName);
		}
	}
}

void BonjourBrowserWrapper::bonjourRecordResolved(const QString &fullName, const QHostInfo &hostInfo, int port, const QMap<QString,QByteArray> &txt)
{
	Debug(Logger::getInstance("BonJour"), "bonjourRecordResolved - [%s], hostInfo (%s), port (%d)", QSTRING_CSTR(fullName), QSTRING_CSTR(hostInfo.hostName()), port);

//	for(auto service : _services.keys())
//	{
//		Debug(Logger::getInstance("BonJour"), "bonjourRecordResolved - services found [%s]",  QSTRING_CSTR(service));
//	}

	if (hostInfo.error() != QHostInfo::NoError) {
		qDebug() << "Resolve failed:" << hostInfo.errorString();
	}

	if ( _services.contains(fullName))
	{
		Debug(Logger::getInstance("BonJour"), "bonjourRecordResolved - contains(fullname)");
		QString host   = hostInfo.hostName();
		QString domain = _services[fullName].replyDomain;
		if (host.endsWith("."+domain))
		{
			host.remove(host.length()-domain.length()-1,domain.length()+1);
		}
		Debug(Logger::getInstance("BonJour"), "bonjourRecordResolved - service (%s), type (%s)",
			   QSTRING_CSTR(_services[fullName].serviceName),
			   QSTRING_CSTR(_services[fullName].registeredType));

		_services[fullName].hostName = host;
		_services[fullName].port     = port;

		if ( port < 0 )
		{
			Error(Logger::getInstance("BonJour"), "bonjourRecordResolved - port < 0 for service (%s), type (%s)",
				   QSTRING_CSTR(_services[fullName].serviceName),
				   QSTRING_CSTR(_services[fullName].registeredType));
			_services[fullName].port = 0;
		}
		_services[fullName].address  = hostInfo.addresses().isEmpty() ? "" : hostInfo.addresses().constFirst().toString();
		_services[fullName].txt      = txt;

		ServiceMap service;
		service = _servicesResolved[_services[fullName].registeredType];
		service.insert (_services[fullName].serviceName,_services[fullName]);

		_servicesResolved.insert(_services[fullName].registeredType, service);


		//emit change
		emit browserChange(_servicesResolved[BONJOUR_HYPERION_SERVICES_TYPE]);
	}
	else
	{
		Debug(Logger::getInstance("BonJour"), "bonjourRecordResolved - NOT contains(fullname)");
	}
}

void BonjourBrowserWrapper::bonjourResolve()
{
	Debug(Logger::getInstance("BonJour"), "bonjourResolve");

	ServiceMap::const_iterator i;
	for (i = _services.begin(); i != _services.end(); ++i)
	{
		if (i.value().port  < 0 )
		{
			Debug(Logger::getInstance("BonJour"), "bonjourResolve - service [%s:%s], port (%d)",
					QSTRING_CSTR(i.value().registeredType), QSTRING_CSTR(i.value().serviceName), i.value().port);

			_bonjourCurrentServiceToResolve = i.value().fullName;
			if ( !_bonjourResolver->resolveBonjourRecord(i.value()) )
			{
				Debug(Logger::getInstance("BonJour"), "bonjourResolve failed for - service [%s:%s], port (%d)",
					   QSTRING_CSTR(i.value().registeredType), QSTRING_CSTR(i.value().serviceName), i.value().port);
			}
			break;
		}
	}
}

QVariantList BonjourBrowserWrapper::getServicesDiscoveredJson(const QString &serviceType, const QString& filter) const
{
	Debug(Logger::getInstance("BonJour"), "getServicesDiscoveredJson");

	QJsonArray result;

	if ( _servicesResolved.contains(serviceType) )
	{
		ServiceMap services = _servicesResolved[serviceType];


		Debug(Logger::getInstance("BonJour"), "Get services of type [%s], matching name: [%s]",  QSTRING_CSTR(serviceType), QSTRING_CSTR(filter));

		QRegularExpression regEx( filter );
		if (!regEx.isValid()) {
			QString errorString = regEx.errorString();
			int errorOffset = regEx.patternErrorOffset();

			Error(Logger::getInstance("BonJour"), "Filtering regular expression [%s] error [%d]:[%s]",  QSTRING_CSTR(filter), errorOffset, QSTRING_CSTR(errorString) );
		}
		else
		{
			ServiceMap::const_iterator i;
			for (i = services.begin(); i != services.end(); ++i)
			{
				QRegularExpressionMatch match = regEx.match(i.key());
				if ( match.hasMatch() )
				{
					Debug(Logger::getInstance("BonJour"),"Found service [%s], type [%s]", QSTRING_CSTR(i.key()), QSTRING_CSTR(i.value().registeredType));

					QJsonObject obj;

					obj.insert("id", i.key());

					obj.insert("name", i.value().serviceName);
					obj.insert("type", i.value().registeredType);
					obj.insert("domain", i.value().replyDomain);
					obj.insert("address", i.value().address);
					obj.insert("hostname", i.value().hostName);
					obj.insert("port", i.value().port);

					qDebug() << "i.value().txt [" << i.value().txt << "]";

					QJsonObject objOther;
					QMap <QString,QByteArray>::const_iterator o;
					for (o = i.value().txt.begin(); o != i.value().txt.end(); ++o)
					{
						objOther.insert(o.key(), o.value().data());
					}
					obj.insert("bonjourTxt", objOther);

					result  << obj;
				}
			}
		}
		Debug(Logger::getInstance("BonJour"), "result: [%s]", QString(QJsonDocument(result).toJson(QJsonDocument::Compact)).toUtf8().constData() );
	}
	else
	{
		Debug(Logger::getInstance("BonJour"), "No servicetype [%s] resolved",  QSTRING_CSTR(serviceType));
	}

	return result.toVariantList();
}
