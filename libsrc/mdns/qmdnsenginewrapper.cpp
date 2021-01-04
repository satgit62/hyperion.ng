#include <mdns/mdnsenginewrapper.h>
#include <qmdnsengine/resolver.h>
#include <qmdnsengine/record.h>

//Qt includes
#include <QTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRegularExpression>

// Utility includes
#include <utils/Logger.h>
#include <HyperionConfig.h>
#include <hyperion/AuthManager.h>

namespace {
	const bool verbose = false;
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

	Debug(_log, "");

	_server = new QMdnsEngine::Server(this);
	_cache = new QMdnsEngine::Cache(this);

	// mDNS Provider Test - to be refined
	_hostname = new QMdnsEngine::Hostname(_server, this);
	connect(_hostname, &QMdnsEngine::Hostname::hostnameChanged, this, &MdnsEngineWrapper::onHostnameChanged);

	qDebug() << "hostname: " << _hostname->hostname() << " 	isRegistered " << _hostname->isRegistered();
	_provider = new QMdnsEngine::Provider(_server, _hostname, this);

	Debug(_log, "Hostname[%s] ", QSTRING_CSTR(QString(_hostname->hostname())));

	QMdnsEngine::Service service;
	service.setType("_hiperion-flatbuf._tcp.local.");
	service.setName("My hiperion flatbuffer Service");
	service.setPort(19400);

	QByteArray id = AuthManager::getInstance()->getID().toUtf8();
	const QMap<QByteArray, QByteArray> attributes = { {"id", id},{"version", HYPERION_VERSION} };

	service.setAttributes(attributes);

	qDebug() << "provide: " << service.name() << " hostname: " << service.hostname() << " port: " << service.port() << " type: " << service.type();

	_provider->update(service);

	browseForServiceType("_hiperion-flatbuf._tcp.local.");
}

MdnsEngineWrapper::~MdnsEngineWrapper()
{
	QMapIterator<QByteArray, QMdnsEngine::Browser*> i(_browsedServiceTypes);
	while (i.hasNext()) {
		i.next();
		QMdnsEngine::Browser* browserPtr = i.value();
		browserPtr->disconnect();
		browserPtr->deleteLater();
	}

	delete _cache;
	delete _provider;
	delete _hostname;
	delete _server;
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
	DebugIf(verbose, _log, "Hostname changed to Hostname [%s]", QSTRING_CSTR(QString(hostname)));
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

	auto* resolver = new QMdnsEngine::Resolver(_server, service.hostname(), _cache, this);
	connect(resolver, &QMdnsEngine::Resolver::resolved, this, &MdnsEngineWrapper::onServiceResolved);
}

void MdnsEngineWrapper::onServiceResolved(const QHostAddress& address)
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

QVariantList MdnsEngineWrapper::getServicesDiscoveredJson(const QByteArray& serviceType, const QString& filter) const
{
	Debug(_log, "Get services of type [%s], matching name: [%s]", QSTRING_CSTR(QString(serviceType)), QSTRING_CSTR(filter));
	//	printCache(nullptr, QMdnsEngine::PTR);
	//	printCache(nullptr, QMdnsEngine::SRV);
	//	printCache(nullptr, QMdnsEngine::A);
	//	printCache(nullptr, QMdnsEngine::TXT);

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
	_cache->lookupRecords(name, type, records);
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
