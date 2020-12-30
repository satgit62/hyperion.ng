#include <mdns/mdnsenginewrapper.h>
#include <utils/Logger.h>

namespace {

} //End of constants

MdnsEngineWrapper* MdnsEngineWrapper::instance = nullptr;

MdnsEngineWrapper::MdnsEngineWrapper(QObject * parent)
	: QObject(parent)
{
	MdnsEngineWrapper::instance = this;
}

bool MdnsEngineWrapper::browseForServiceType(const QString &serviceType)
{
	return false;
}
