#ifndef LEDDEVICEBONJOURREGISTER_H
#define LEDDEVICEBONJOURREGISTER_H

#include <QMap>

struct BonjourConfig
{
	QString serviceType;
	QString serviceNameFilter;
};

typedef QMap<QString, BonjourConfig> BonjourConfigMap;

const BonjourConfigMap ledDeviceBonjourConfigtMap = {

	{"cololight", {"_hap._tcp", "ColoLight.*"}},
	{"nanoleaf"	, {"_nanoleafapi._tcp", ".*"}},
	{"wled"		, {"_wled._tcp", ".*"}},
	{"yeelight"	, {"_hap._tcp", "Yeelight.*|YLBulb.*"}},
};

class LedDeviceBonjourRegister {
public:
	static QString getServiceType(const QString &deviceType) { return ledDeviceBonjourConfigtMap[deviceType].serviceType; }
	static QString getServiceNameFilter(const QString &deviceType) { return ledDeviceBonjourConfigtMap[deviceType].serviceNameFilter; }
	static BonjourConfigMap getAllConfigs () { return ledDeviceBonjourConfigtMap; }
};

#endif // LEDDEVICEBONJOURREGISTER_H
