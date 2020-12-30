#pragma once

#include <qmdnsengine/server.h>
#include <qmdnsengine/resolver.h>

// qt incl
#include <QObject>

class MdnsEngineWrapper : public QObject
{
	Q_OBJECT
private:
	friend class HyperionDaemon;

	MdnsEngineWrapper(QObject * parent = nullptr);

public:

	static MdnsEngineWrapper* instance;
	static MdnsEngineWrapper *getInstance()	{ return instance; }

public slots:

	///
	/// @brief Browse for a service
	///
	bool browseForServiceType(const QString &serviceType);
	//QVariantList getServicesDiscoveredJson(const QString &serviceType, const QString &filter = ".*") const;

private:

	QMdnsEngine::Server mServer;
	QMdnsEngine::Resolver *mResolver;
};
