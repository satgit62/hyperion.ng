#include <mdns/mdnsenginewrapper.h>
#include <utils/Logger.h>

#include <qmdnsengine/dns.h>
#include <qmdnsengine/mdns.h>
#include <qmdnsengine/server.h>
#include <qmdnsengine/browser.h>
#include <qmdnsengine/resolver.h>

//qt includes
#include <QTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRegularExpression>

namespace {
} //End of constants

MdnsEngineWrapper* MdnsEngineWrapper::instance = nullptr;

MdnsEngineWrapper::MdnsEngineWrapper(QObject* parent)
	: QObject(parent)
	, _server(nullptr)
	, _cache(nullptr)

{
	MdnsEngineWrapper::instance = this;

	Debug(Logger::getInstance("mDNS"), "");

	_server = new QMdnsEngine::Server(this);
	_cache = new QMdnsEngine::Cache(this);

	//browseForServiceType(QMdnsEngine::MdnsBrowseType);
	//browseForServiceType("_hap._tcp.local.");
	//browseForServiceType("_wled._tcp.local.");

	//_browser = new QMdnsEngine::Browser(&_server, QMdnsEngine::MdnsBrowseType,_cache, this);

	//QObject::connect(_browser, &QMdnsEngine::Browser::serviceAdded,
	//	[](const QMdnsEngine::Service& service) {
	//		qDebug() << "discovered: " << service.name() << " hostname: " << service.hostname() << " port: " << service.port() << " type: " << service.type();
	//		qDebug() << "discovered: " << service.name() << " attributes " << service.attributes();
	//	}
	//);

	/*
	QObject::connect(_resolver, &QMdnsEngine::Resolver::resolved,
		[](const QHostAddress& address) {
			qDebug() << "resolved to" << address;
		}
	);
	*/

	//QObject::connect(_browser, &QMdnsEngine::Browser::serviceAdded, this, &MdnsEngineWrapper::onServiceAdded);
	//QObject::connect(_browser, &QMdnsEngine::Browser::serviceUpdated, this, &MdnsEngineWrapper::onServiceUpdated);
	//QObject::connect(_browser, &QMdnsEngine::Browser::serviceRemoved, this, &MdnsEngineWrapper::onServiceRemoved);

	//connect(_browser, &QMdnsEngine::Browser::serviceAdded, this, &MdnsEngineWrapper::loadService);
	//connect(_browser, &QMdnsEngine::Browser::serviceUpdated, this, &MdnsEngineWrapper::loadService);
}

bool MdnsEngineWrapper::browseForServiceType(const QByteArray& serviceType)
{
	Debug(Logger::getInstance("mDNS"), "serviceType [%s]", QSTRING_CSTR(QString(serviceType)));
	if (!_browsedServiceTypes.contains(serviceType))
	{
		Debug(Logger::getInstance("mDNS"), "Start new Browser for serviceType [%s]", QSTRING_CSTR(QString(serviceType)));
		QMdnsEngine::Browser* newBrowser = new QMdnsEngine::Browser(_server, serviceType, _cache, this);

		QObject::connect(newBrowser, &QMdnsEngine::Browser::serviceAdded, this, &MdnsEngineWrapper::onServiceAdded);
		QObject::connect(newBrowser, &QMdnsEngine::Browser::serviceUpdated, this, &MdnsEngineWrapper::onServiceUpdated);
		QObject::connect(newBrowser, &QMdnsEngine::Browser::serviceRemoved, this, &MdnsEngineWrapper::onServiceRemoved);

		_browsedServiceTypes.insert(serviceType, newBrowser);
		return true;
	}
	return false;
}

void MdnsEngineWrapper::onServiceAdded(const QMdnsEngine::Service& service)
{
	Debug(Logger::getInstance("mDNS"), "Name: [%s], Hostname[%s] ", QSTRING_CSTR(QString(service.name())), QSTRING_CSTR(QString(service.hostname())));

	//QMdnsEngine::Resolver resolver(&_server, service.name());
}

void MdnsEngineWrapper::onServiceUpdated(const QMdnsEngine::Service& service)
{
	Debug(Logger::getInstance("mDNS"), "");
}

void MdnsEngineWrapper::onServiceRemoved(const QMdnsEngine::Service& service)
{
	Debug(Logger::getInstance("mDNS"), "");
}

void MdnsEngineWrapper::loadService(const QMdnsEngine::Service& service)
{
	Debug(Logger::getInstance("mDNS"), "");
	auto* resolver = new QMdnsEngine::Resolver(_server, service.hostname(), _cache, this);

	connect(resolver, &QMdnsEngine::Resolver::resolved, this, &MdnsEngineWrapper::onServiceResolved);
}

void MdnsEngineWrapper::onServiceResolved(const QHostAddress& address)
{
	Debug(Logger::getInstance("mDNS"), "");
	if (address.protocol() == QAbstractSocket::IPv4Protocol)
	{
		qDebug() << "resolved to IP4" << address;
	}

	if (address.protocol() == QAbstractSocket::IPv6Protocol)
	{
		qDebug() << "resolved to IP6" << address;
	}

	//QList<QMdnsEngine::Record> records;
	//_cache->lookupRecords(0, QMdnsEngine::ANY, records);
	////_cache->lookupRecords(0, QMdnsEngine::TXT, records);
	//foreach(QMdnsEngine::Record record, records) {
	//	qDebug() << "Record:" << record.name() << " type: " << record.type() << " address: " << record.address() << " port: " << record.port() << " prio: " << record.priority();;
	//	qDebug() << "Record:" << record.name() << "Attributes: " << record.attributes();
	//}
}

QVariantList MdnsEngineWrapper::getServicesDiscoveredJson(const QByteArray& serviceType, const QString& filter) const
{
	Debug(Logger::getInstance("mDNS"), "getServicesDiscoveredJson");

	QJsonArray result;


	if (_browsedServiceTypes.contains(serviceType))
	{
		//ServiceMap services = _servicesResolved[serviceType];


		//Debug(Logger::getInstance("BonJour"), "Get services of type [%s], matching name: [%s]", QSTRING_CSTR(serviceType), QSTRING_CSTR(filter));

		//QRegularExpression regEx(filter);
		//if (!regEx.isValid()) {
		//	QString errorString = regEx.errorString();
		//	int errorOffset = regEx.patternErrorOffset();

		//	Error(Logger::getInstance("BonJour"), "Filtering regular expression [%s] error [%d]:[%s]", QSTRING_CSTR(filter), errorOffset, QSTRING_CSTR(errorString));
		//}
		//else
		//{
		//	ServiceMap::const_iterator i;
		//	for (i = services.begin(); i != services.end(); ++i)
		//	{
		//		QRegularExpressionMatch match = regEx.match(i.key());
		//		if (match.hasMatch())
		//		{
		//			Debug(Logger::getInstance("BonJour"), "Found service [%s], type [%s]", QSTRING_CSTR(i.key()), QSTRING_CSTR(i.value().registeredType));

		//			QJsonObject obj;

		//			obj.insert("id", i.key());

		//			obj.insert("name", i.value().serviceName);
		//			obj.insert("type", i.value().registeredType);
		//			obj.insert("domain", i.value().replyDomain);
		//			obj.insert("address", i.value().address);
		//			obj.insert("hostname", i.value().hostName);
		//			obj.insert("port", i.value().port);

		//			qDebug() << "i.value().txt [" << i.value().txt << "]";

		//			QJsonObject objOther;
		//			QMap <QString, QByteArray>::const_iterator o;
		//			for (o = i.value().txt.begin(); o != i.value().txt.end(); ++o)
		//			{
		//				objOther.insert(o.key(), o.value().data());
		//			}
		//			obj.insert("bonjourTxt", objOther);

		//			result << obj;
		//		}
		//	}
		//}
		Debug(Logger::getInstance("mDNS"), "result: [%s]", QString(QJsonDocument(result).toJson(QJsonDocument::Compact)).toUtf8().constData());
	}
	else
	{
		Debug(Logger::getInstance("mDNS"), "No servicetype [%s] resolved", QSTRING_CSTR(QString(serviceType)));
	}

	return result.toVariantList();
}
