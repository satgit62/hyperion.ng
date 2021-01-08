#include <mdns/mdnsenginewrapper.h>
#include <qmdnsengine/resolver.h>
#include <qmdnsengine/record.h>
#include <qmdnsengine/message.h>

//Qt includes
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QHostInfo>
#include <QThread>

// Utility includes
#include <utils/Logger.h>
#include <HyperionConfig.h>
#include <hyperion/AuthManager.h>


namespace {
	const bool verbose = false;
	const bool verboseProvider = false;
} //End of constants

MdnsEngineWrapper* MdnsEngineWrapper::instance = nullptr;

MdnsEngineWrapper::MdnsEngineWrapper(QObject* parent)
	: QObject(parent)
	, _log(Logger::getInstance("MDNS"))
	, _server(nullptr)
	, _hostname(nullptr)
	, _provider(nullptr)
	, _cache(nullptr)
{
	MdnsEngineWrapper::instance = this;

	qRegisterMetaType<QMdnsEngine::Message>("Message");

	_server = new QMdnsEngine::Server(this);
	_cache = new QMdnsEngine::Cache(this);
	_hostname = new QMdnsEngine::Hostname(_server, this);

	connect(_hostname, &QMdnsEngine::Hostname::hostnameChanged, this, &MdnsEngineWrapper::onHostnameChanged);
	DebugIf(verboseProvider, _log, "Hostname [%s], isRegistered [%d]", QSTRING_CSTR(QString(_hostname->hostname())), _hostname->isRegistered());

	//For Testing
	provideServiceType("_hiperiond-flatbuf._tcp.local.", 19400, "flatbuffer");
	provideServiceType("_hiperiond-api._tcp.local.", 19444, "API");
	//provideServiceType("_http._tcp.local.", 8090);
	//provideServiceType("_https._tcp.local.", 8092);

	browseForServiceType("_hiperiond-flatbuf._tcp.local.");
	browseForServiceType("_hiperiond-json._tcp.local.");
	browseForServiceType("_http._tcp.local.");
	browseForServiceType("_https._tcp.local.");
}

MdnsEngineWrapper::~MdnsEngineWrapper()
{
	QMapIterator<QByteArray, QMdnsEngine::Browser*> b(_browsedServiceTypes);
	while (b.hasNext()) {
		b.next();
		QMdnsEngine::Browser* browserPtr = b.value();
		browserPtr->disconnect();
		delete browserPtr;
	}

	QMapIterator<QByteArray, QMdnsEngine::Provider*> p(_providedServiceTypes);
	while (p.hasNext()) {
		p.next();
		QMdnsEngine::Provider* providerPtr = p.value();
		delete providerPtr;
	}

	_hostname->disconnect();

	delete _cache;
	delete _provider;
	delete _hostname;
	delete _server;
}

bool MdnsEngineWrapper::provideServiceType(const QByteArray& serviceType, quint16 servicePort, const QByteArray& serviceName)
{
	DebugIf(verbose, _log, "Start new Provider for serviceType [%s]", QSTRING_CSTR(QString(serviceType)));
	qDebug() << "\nMdnsEngineWrapper::provideServiceType" << QThread::currentThread();
	if (!_providedServiceTypes.contains(serviceType))
	{


		QMdnsEngine::Provider* newProvider = new QMdnsEngine::Provider(_server, _hostname);

		QMdnsEngine::Service service;
		service.setType(serviceType);
		service.setPort(servicePort);

		QByteArray name("Hyperion");
		if (!serviceName.isEmpty())
		{
			name += "-" + serviceName;
		}
		name += "@" + QHostInfo::localHostName().toUtf8();;
		service.setName(name);

		QByteArray id = AuthManager::getInstance()->getID().toUtf8();
		const QMap<QByteArray, QByteArray> attributes = { {"id", id}, {"version", HYPERION_VERSION} };
		service.setAttributes(attributes);

		DebugIf(verboseProvider, _log, "[%s] Name: [%s], Hostname[%s], Port: [%u] ",
			QSTRING_CSTR(QString(service.type())),
			QSTRING_CSTR(QString(service.name())),
			QSTRING_CSTR(QString(service.hostname())), service.port());
		
		newProvider->update(service);

		_providedServiceTypes.insert(serviceType, newProvider);
		return true;
	}
	return false;
}

bool MdnsEngineWrapper::browseForServiceType(const QByteArray& serviceType)
{
	if (!_browsedServiceTypes.contains(serviceType))
	{
		DebugIf(verbose, _log, "Start new Browser for serviceType [%s]", QSTRING_CSTR(QString(serviceType)));
		QMdnsEngine::Browser* newBrowser = new QMdnsEngine::Browser(_server, serviceType, _cache, this);

		QObject::connect(newBrowser, &QMdnsEngine::Browser::serviceAdded, this, &MdnsEngineWrapper::onServiceAdded);
		QObject::connect(newBrowser, &QMdnsEngine::Browser::serviceUpdated, this, &MdnsEngineWrapper::onServiceUpdated);
		QObject::connect(newBrowser, &QMdnsEngine::Browser::serviceRemoved, this, &MdnsEngineWrapper::onServiceRemoved);

		_browsedServiceTypes.insert(serviceType, newBrowser);
		return true;
	}
	return false;
}

void MdnsEngineWrapper::onHostnameChanged(const QByteArray& hostname)
{
	DebugIf(verboseProvider, _log, "Hostname changed to Hostname [%s]", QSTRING_CSTR(QString(hostname)));
}

void MdnsEngineWrapper::onServiceAdded(const QMdnsEngine::Service& service)
{
	DebugIf(verbose, _log, "[%s] Name: [%s], Hostname[%s], Port: [%u] ",
		QSTRING_CSTR(QString(service.type())),
		QSTRING_CSTR(QString(service.name())),
		QSTRING_CSTR(QString(service.hostname())), service.port());
	resolveService(service);
}

void MdnsEngineWrapper::onServiceUpdated(const QMdnsEngine::Service& service)
{
	DebugIf(verbose, _log, "[%s] Name: [%s], Hostname[%s], Port: [%u] ",
		QSTRING_CSTR(QString(service.type())),
		QSTRING_CSTR(QString(service.name())),
		QSTRING_CSTR(QString(service.hostname())), service.port());
	resolveService(service);
}

void MdnsEngineWrapper::resolveService(const QMdnsEngine::Service& service)
{
	DebugIf(verbose, _log, "[%s] Name: [%s], Hostname[%s], Port: [%u] ",
		QSTRING_CSTR(QString(service.type())),
		QSTRING_CSTR(QString(service.name())),
		QSTRING_CSTR(QString(service.hostname())), service.port());

	emit resolveHostName(service.hostname());
}

void MdnsEngineWrapper::resolveHostName(const QByteArray& hostName)
{
	DebugIf(verbose, _log, "Hostname[%s]", QSTRING_CSTR(QString(hostName)));

	qRegisterMetaType<QMdnsEngine::Message>("Message");
	auto* resolver = new QMdnsEngine::Resolver(_server, hostName, _cache);
	connect(resolver, &QMdnsEngine::Resolver::resolved, this, &MdnsEngineWrapper::onHostNameResolved);

	//qRegisterMetaType<QMdnsEngine::Message>("Message");
	//QMdnsEngine::Resolver resolver(_server, hostName, _cache);
	//connect(&resolver, &QMdnsEngine::Resolver::resolved, [](const QHostAddress& hostAddress) {
	//	qDebug() << "\nMdnsEngineWrapper::getHostAddress" << QThread::currentThread();
	//	qDebug() << "\nMdnsEngineWrapper::hostAddress" << hostAddress;
	//});
}

void MdnsEngineWrapper::onHostNameResolved(const QHostAddress& address)
{
	switch (address.protocol()) {
	case QAbstractSocket::IPv4Protocol:
		DebugIf(verbose, _log, "resolved to IP4 [%s]", QSTRING_CSTR(address.toString()));
		break;
	case QAbstractSocket::IPv6Protocol:
		DebugIf(verbose, _log, "resolved to IP6 [%s]", QSTRING_CSTR(address.toString()));
		break;
	default:
		break;
	}
}

void MdnsEngineWrapper::onServiceRemoved(const QMdnsEngine::Service& service)
{
	DebugIf(verbose, _log, "[%s] Name: [%s], Hostname[%s], Port: [%u] ",
		QSTRING_CSTR(QString(service.type())),
		QSTRING_CSTR(QString(service.name())),
		QSTRING_CSTR(QString(service.hostname())), service.port());
}

QHostAddress MdnsEngineWrapper::getHostAddress(const QString& hostName)
{
	return getHostAddress(hostName.toUtf8());
}

QHostAddress MdnsEngineWrapper::getHostAddress(const QByteArray& hostName)
{
	Debug(_log, "Resolve IP-address for hostname [%s].", QSTRING_CSTR(QString(hostName)));

	qDebug() << "\nMdnsEngineWrapper::getHostAddress" << QThread::currentThread();

	QHostAddress hostAddress;

	QMdnsEngine::Record aRecord;
	if (!_cache->lookupRecord(hostName, QMdnsEngine::A, aRecord))
	{
		DebugIf(verbose, _log, "IP-address for hostname [%s] not yet in cache, start resolver.", QSTRING_CSTR(QString(hostName)));
		//emit resolveHostName(hostName);
	}
	else
	{
		hostAddress = aRecord.address();
		Debug(_log, "Hostname [%s] translates to IP-address [%s]", QSTRING_CSTR(QString(hostName)), QSTRING_CSTR(hostAddress.toString()));
	}
	return hostAddress;
}

QVariantList MdnsEngineWrapper::getServicesDiscoveredJson(const QByteArray& serviceType, const QString& filter) const
{
	Debug(_log, "Get services of type [%s], matching name: [%s]", QSTRING_CSTR(QString(serviceType)), QSTRING_CSTR(filter));

	QJsonArray result;

	QRegularExpression regEx(filter);
	if (!regEx.isValid()) {
		QString errorString = regEx.errorString();
		int errorOffset = regEx.patternErrorOffset();

		Error(_log, "Filtering regular expression [%s] error [%d]:[%s]", QSTRING_CSTR(filter), errorOffset, QSTRING_CSTR(errorString));
	}
	else
	{
		QList<QMdnsEngine::Record> ptrRecords;

		if (_cache->lookupRecords(serviceType, QMdnsEngine::PTR, ptrRecords))
		{
			for (int ptrCounter = 0; ptrCounter < ptrRecords.size(); ++ptrCounter)
			{
				QByteArray serviceNameFull = ptrRecords.at(ptrCounter).target();

				QRegularExpressionMatch match = regEx.match(serviceNameFull);
				if (match.hasMatch())
				{
					QMdnsEngine::Record srvRecord;
					if (!_cache->lookupRecord(serviceNameFull, QMdnsEngine::SRV, srvRecord))
					{
						Debug(_log, "No SRV record for [%s] found, skip entry", QSTRING_CSTR(QString(serviceNameFull)));
					}
					else
					{
						QJsonObject obj;

						obj.insert("id", QString(serviceNameFull));
						obj.insert("nameFull", QString(serviceNameFull));
						obj.insert("type", QString(serviceType));

						if (serviceNameFull.endsWith("." + serviceType))
						{
							QString serviceName = serviceNameFull.left(serviceNameFull.length() - serviceType.length() - 1);
							obj.insert("name", QString(serviceName));
						}

						QByteArray hostName = srvRecord.target();
						obj.insert("hostname", QString(hostName));

						quint16 port = srvRecord.port();
						obj.insert("port", port);

						QMdnsEngine::Record txtRecord;
						if (_cache->lookupRecord(serviceNameFull, QMdnsEngine::TXT, txtRecord))
						{
							QMap<QByteArray, QByteArray> txtAttributes = txtRecord.attributes();

							QVariantMap txtMap;
							QMapIterator<QByteArray, QByteArray> i(txtAttributes);
							while (i.hasNext()) {
								i.next();
								txtMap.insert(i.key(), i.value());
							}
							obj.insert("txt", QJsonObject::fromVariantMap(txtMap));
						}

						QMdnsEngine::Record aRecord;
						if (_cache->lookupRecord(hostName, QMdnsEngine::A, aRecord))
						{
							QHostAddress hostAddress = aRecord.address();
							obj.insert("address", hostAddress.toString());
						}
						result << obj;
					}
				}
			}
			Debug(_log, "result: [%s]", QString(QJsonDocument(result).toJson(QJsonDocument::Compact)).toUtf8().constData());
		}
		else
		{
			Debug(_log, "No service of type [%s] found", QSTRING_CSTR(QString(serviceType)));
		}
	}

	return result.toVariantList();
}

void MdnsEngineWrapper::printCache(const QByteArray& name, quint16 type) const
{
	QList<QMdnsEngine::Record> records;
	if (_cache->lookupRecords(name, type, records))
	{
		foreach(QMdnsEngine::Record record, records)
		{
			qDebug() << QMdnsEngine::typeName(record.type()) << "," << record.name() << "], ttl       : " << record.ttl();

			switch (record.type()) {
			case QMdnsEngine::PTR:
				qDebug() << QMdnsEngine::typeName(record.type()) << "," << record.name() << ", target    : " << record.target();
				break;

			case QMdnsEngine::SRV:
				qDebug() << QMdnsEngine::typeName(record.type()) << "," << record.name() << ", target    : " << record.target();
				qDebug() << QMdnsEngine::typeName(record.type()) << "," << record.name() << ", port      : " << record.port();
				qDebug() << QMdnsEngine::typeName(record.type()) << "," << record.name() << ", priority  : " << record.priority();
				qDebug() << QMdnsEngine::typeName(record.type()) << "," << record.name() << ", weight    : " << record.weight();
				break;
			case QMdnsEngine::TXT:
				qDebug() << QMdnsEngine::typeName(record.type()) << "," << record.name() << ", attributes: " << record.attributes();
				break;

			case QMdnsEngine::NSEC:
				qDebug() << QMdnsEngine::typeName(record.type()) << "," << record.name() << ", nextDomNam: " << record.nextDomainName();
				break;

			case QMdnsEngine::A:
			case QMdnsEngine::AAAA:
				qDebug() << QMdnsEngine::typeName(record.type()) << "," << record.name() << ", address   : " << record.address();
				break;
			}
		}
	}
}
