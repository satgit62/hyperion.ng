#pragma once
// qt incl
#include <QObject>
#include <QMultiMap>
#include <QHostInfo>
#include <QRegularExpression>

#include <bonjour/bonjourrecord.h>

class BonjourServiceBrowser;
class BonjourServiceResolver;
class QTimer;

class BonjourBrowserWrapper : public QObject
{
	Q_OBJECT
private:
	friend class HyperionDaemon;
	///
	/// @brief Browse for hyperion services in bonjour, constructed from HyperionDaemon
	///        Searching for hyperion http service by default
	///
	BonjourBrowserWrapper(QObject * parent = nullptr);

public:

	///
	/// @brief Get all available sessions
	///
	QMap<QString, QMap<QString, BonjourRecord>> getAllServices() { return _servicesResolved; }
	QMap<QString, BonjourRecord> getAllServices(const QString &serviceType, const QString &filter) { return _servicesResolved[serviceType]; }


	static BonjourBrowserWrapper* instance;
	static BonjourBrowserWrapper *getInstance()	{ return instance; }

public slots:

	///
	/// @brief Browse for a service
	///
	bool browseForServiceType(const QString &serviceType);
	QVariantList getServicesDiscoveredJson(const QString &serviceType, const QString &filter = ".*") const;

signals:
	///
	/// @brief Emits whenever a change happened
	///
	void browserChange( const QMap<QString, BonjourRecord> &bRegisters );

private:

	/// map of service names and browsers
	QMap<QString, BonjourServiceBrowser *> _browsedServiceTypes;
	/// Resolver
	BonjourServiceResolver *_bonjourResolver;

	typedef QMap<QString, BonjourRecord> ServiceMap;

	// contains all current active services registered for
	ServiceMap _services;
	QMap<QString, ServiceMap > _servicesResolved;

	QString _bonjourCurrentServiceTypeToResolve;
	QString _bonjourCurrentServiceToResolve;
	/// timer to resolve changes
	QTimer *_timerBonjourResolver;

private slots:
	///
	/// @brief is called whenever a BonjourServiceBrowser emits change
	void currentBonjourRecordsChanged( const QList<BonjourRecord> &list );
	/// @brief new record resolved
	void bonjourRecordResolved(const QString &fullname, const QHostInfo &hostInfo, int port, const QMap<QString,QByteArray> &txt);

	///
	/// @brief timer slot which updates regularly entries
	///
	void bonjourResolve();

};
