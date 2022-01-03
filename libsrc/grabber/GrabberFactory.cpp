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

		QString type = grabberConfig["device"].toString("auto");

		if (type == "qt" || type == "auto")
		{
			grabber = new QtGrabber(grabberConfig["input"].toInt(0), _grabber_cropLeft, _grabber_cropRight, _grabber_cropTop, _grabber_cropBottom);
		}
		else
		{
			Error(log, "Unknown platform capture type: %s", QSTRING_CSTR(type));
			// // TODO dummy objekt zurück geben oder nullptr ?
		}
	}
	else if (settingsType == settings::VIDEOGRABBER)
	{
		// TODO
	}

	return grabber;
}
