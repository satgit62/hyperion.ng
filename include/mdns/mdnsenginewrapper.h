#ifndef MDNSENGINEWRAPPER_H
#define MDNSENGINEWRAPPER_H

#include <qmdnsengine/server.h>
#include <qmdnsengine/service.h>
#include <qmdnsengine/browser.h>
#include <qmdnsengine/resolver.h>
#include <qmdnsengine/cache.h>
#include <qmdnsengine/record.h>

// qt incl
#include <QObject>
#include <QByteArray>
#include <QHostAddress>

class MdnsEngineWrapper : public QObject
{
	Q_OBJECT
private:
	friend class HyperionDaemon;

	MdnsEngineWrapper(QObject* parent = nullptr);

public:

	static MdnsEngineWrapper* instance;
	static MdnsEngineWrapper* getInstance() { return instance; }

public slots:

	///
	/// @brief Browse for a service of type
	///
	bool browseForServiceType(const QByteArray& serviceType);


	void loadService(const QMdnsEngine::Service& service);
	//QVariantList getServicesDiscoveredJson(const QString &serviceType, const QString &filter = ".*") const;

private slots:

		void onServiceAdded(const QMdnsEngine::Service &service);
		void onServiceUpdated(const QMdnsEngine::Service &service);
		void onServiceRemoved(const QMdnsEngine::Service &service);

		void onServiceResolved(const QHostAddress& address);

private:

	QMdnsEngine::Server* _server;
	QMdnsEngine::Cache* _cache;

	/// map of service names and browsers
	QMap<QByteArray, QMdnsEngine::Browser*> _browsedServiceTypes;

};

#endif // MDNSENGINEWRAPPER_H
