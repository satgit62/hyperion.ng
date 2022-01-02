// grabber includes
#include <grabber/GrabberFactory.h>
#include <grabber/GrabberWrapper.h>
#include <grabber/Grabber.h>

// utils
#include <utils/Logger.h>

Grabber* GrabberFactory::construct(const QJsonObject& grabberConfig, settings::type settingsType)
{
	Logger * log = Logger::getInstance("GRABBER");
	Grabber* grabber = nullptr;

	if (settingsType == settings::SCREENGRABBER)
	{
		int _grabber_cropLeft = grabberConfig["cropLeft"].toInt(0);
		int _grabber_cropRight = grabberConfig["cropRight"].toInt(0);
		int _grabber_cropTop = grabberConfig["cropTop"].toInt(0);
		int _grabber_cropBottom = grabberConfig["cropBottom"].toInt(0);

#ifdef ENABLE_OSX
		QString type = grabberConfig["device"].toString("osx");
#else
		QString type = grabberConfig["device"].toString("auto");
#endif

		// if (type == "framebuffer")
		// {
		// 	createGrabberFramebuffer(grabberConfig);
		// }
		// else if (type == "dispmanx")
		// {
		// 	createGrabberDispmanx(grabberConfig);
		// }
		// else if (type == "amlogic")
		// {
		// 	createGrabberAmlogic(grabberConfig);
		// }
		// else if (type == "osx")
		// {
		// 	createGrabberOsx(grabberConfig);
		// }
		// else if (type == "x11")
		// {
		// 	createGrabberX11(grabberConfig);
		// }
		// else if (type == "xcb")
		// {
		// 	createGrabberXcb(grabberConfig);
		// }
		// else if (type == "dx")
		// {
		// 	createGrabberDx(grabberConfig);
		// }
		if (type == "qt")
		{
			grabber = new QtGrabber(grabberConfig["input"].toInt(0), _grabber_cropLeft, _grabber_cropRight, _grabber_cropTop, _grabber_cropBottom);
		}
		else
		{
			Error(log, "Unknown platform capture type: %s", QSTRING_CSTR(type));
		}
	}
	else if (settingsType == settings::VIDEOGRABBER)
	{
		// TODO
	}

	return grabber;
}
