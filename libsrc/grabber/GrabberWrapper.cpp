// Hyperion includes
#include <grabber/GrabberWrapper.h>

#include <grabber/Grabber.h>
#include <grabber/GrabberFactory.h>

#include <HyperionConfig.h>

// hyperion includes
#include <hyperion/Hyperion.h>

// qt
#include <QTimer>
#include <QThread>

GrabberWrapper::GrabberWrapper(Hyperion* hyperion)
	: QObject(hyperion)
	, _hyperion(hyperion)
	, _screenGrabber(nullptr)
	, _screenGrabberEnabled(false)
	, _screenGrabberPriority(0)
	, _screenGrabberName()
	, _screenGrabberInactiveTimer(new QTimer(this))
	, _videoGrabber(nullptr)
	, _videoGrabberEnabled(false)
	, _videoGrabberPriority(0)
	, _videoGrabberName()
	, _videoGrabberInactiveTimer(new QTimer(this))
{
	// inactive timer screen grabber
	connect(_screenGrabberInactiveTimer, &QTimer::timeout, this, &GrabberWrapper::setScreenGrabberInactive);
	_screenGrabberInactiveTimer->setSingleShot(true);
	_screenGrabberInactiveTimer->setInterval(5000);

	// inactive timer video grabber
	connect(_videoGrabberInactiveTimer, &QTimer::timeout, this, &GrabberWrapper::setVideoGrabberInactive);
	_videoGrabberInactiveTimer->setSingleShot(true);
	_videoGrabberInactiveTimer->setInterval(1000);

	// init
	settingsChanged(settings::INSTCAPTURE, _hyperion->getSetting(settings::INSTCAPTURE));
}

// GrabberWrapper::GrabberWrapper(const QString& grabberName, Grabber * ggrabber, int updateRate_Hz)
// 	: _grabberName(grabberName)
// 	, _log(Logger::getInstance(grabberName.toUpper()))
// 	, _timer(new QTimer(this))
// 	, _updateInterval_ms(1000/updateRate_Hz)
// 	, _ggrabber(ggrabber)
// {
// 	GrabberWrapper::instance = this;

// 	// Configure the timer to generate events every n milliseconds
// 	_timer->setTimerType(Qt::PreciseTimer);
// 	_timer->setInterval(_updateInterval_ms);

// 	connect(_timer, &QTimer::timeout, this, &GrabberWrapper::action);

// }

GrabberWrapper::~GrabberWrapper()
{
	stopGrabberThread(settings::SCREENGRABBER);
	stopGrabberThread(settings::VIDEOGRABBER);
}

void GrabberWrapper::createGrabber(const QJsonObject& config, settings::type type)
{
	if((type == settings::SCREENGRABBER && _screenGrabber != nullptr) || (type == settings::VIDEOGRABBER && _videoGrabber != nullptr))
	{
		stopGrabberThread(type);
	}

	// create thread and grabber
	QThread* thread = new QThread(this);
	Grabber* ggrabber;

	(type == settings::SCREENGRABBER) ? ggrabber = _screenGrabber : ggrabber = _videoGrabber;

	ggrabber = GrabberFactory::construct(config, type);
	thread->setObjectName(ggrabber->getGrabberName() + "_Thread");
	ggrabber->moveToThread(thread);
	// setup thread management
	connect(thread, &QThread::started, ggrabber, &Grabber::start);

	// further signals
	connect(this, &GrabberWrapper::newVideoMode, ggrabber, &Grabber::setVideoMode, Qt::QueuedConnection);

	if (type == settings::SCREENGRABBER)
	{
		connect(ggrabber, &Grabber::newImage, this, &GrabberWrapper::setScreenImage);
		connect(this, &GrabberWrapper::stopScreenGrabber, ggrabber, &Grabber::stop, Qt::BlockingQueuedConnection);
	}

	if (type == settings::VIDEOGRABBER)
	{
		connect(ggrabber, &Grabber::newImage, this, &GrabberWrapper::setVideoImage);
		connect(this, &GrabberWrapper::stopVideoGrabber, ggrabber, &Grabber::stop, Qt::BlockingQueuedConnection);
	}

	// connect(ggrabber, &LedDevice::enableStateChanged, this, &LedDeviceWrapper::handleInternalEnableState, Qt::QueuedConnection);

	// start the thread
	thread->start();
}

// bool GrabberWrapper::isActive() const
// {
// 	return _timer->isActive();
// }

// QStringList GrabberWrapper::getActive(int inst, GrabberTypeFilter type) const
// {
// 	QStringList result = QStringList();

// 	if (type == GrabberTypeFilter::SCREEN || type == GrabberTypeFilter::ALL)
// 	{
// 		if (GRABBER_SYS_CLIENTS.contains(inst))
// 			result << GRABBER_SYS_CLIENTS.value(inst);
// 	}

// 	if (type == GrabberTypeFilter::VIDEO || type == GrabberTypeFilter::ALL)
// 	{
// 		if (GRABBER_V4L_CLIENTS.contains(inst))
// 			result << GRABBER_V4L_CLIENTS.value(inst);
// 	}

// 	return result;
// }

QStringList GrabberWrapper::availableGrabbers(GrabberTypeFilter type)
{
	QStringList grabbers;

	if (type == GrabberTypeFilter::SCREEN || type == GrabberTypeFilter::ALL)
	{
		#ifdef ENABLE_DISPMANX
				grabbers << "dispmanx";
		#endif

		#ifdef ENABLE_FB
				grabbers << "framebuffer";
		#endif

		#ifdef ENABLE_AMLOGIC
				grabbers << "amlogic";
		#endif

		#ifdef ENABLE_OSX
				grabbers << "osx";
		#endif

		#ifdef ENABLE_X11
				grabbers << "x11";
		#endif

		#ifdef ENABLE_XCB
				grabbers << "xcb";
		#endif

		#ifdef ENABLE_QT
				grabbers << "qt";
		#endif

		#ifdef ENABLE_DX
				grabbers << "dx";
		#endif
	}

	if (type == GrabberTypeFilter::VIDEO || type == GrabberTypeFilter::ALL)
	{
		#if defined(ENABLE_V4L2) || defined(ENABLE_MF)
			grabbers << "v4l2";
		#endif
	}

	return grabbers;
}

// void GrabberWrapper::updateTimer(int interval)
// {
// 	if(_updateInterval_ms != interval)
// 	{
// 		_updateInterval_ms = interval;

// 		const bool& timerWasActive = _timer->isActive();
// 		_timer->stop();
// 		_timer->setInterval(_updateInterval_ms);

// 		if(timerWasActive)
// 			_timer->start();
// 	}
// }

// void GrabberWrapper::tryStart()
// {
// 	// verify start condition
// 	if(!_grabberName.startsWith("V4L") && !GRABBER_SYS_CLIENTS.empty() && getSysGrabberState())
// 		start();
// }

void GrabberWrapper::handleScreenImage(const QString& name, const Image<ColorRgb>& image)
{
	if(_screenGrabberName != name)
	{
		_hyperion->registerInput(_screenGrabberPriority, hyperion::COMP_SCREEN_GRABBER, "System", name);
		_screenGrabberName = name;
	}

	_screenGrabberInactiveTimer->start();
	_hyperion->setInputImage(_screenGrabberPriority, image);
}

void GrabberWrapper::handleVideoImage(const QString& name, const Image<ColorRgb> & image)
{
	if(_videoGrabberName != name)
	{
		_hyperion->registerInput(_videoGrabberPriority, hyperion::COMP_VIDEO_GRABBER, "System", name);
		_videoGrabberName = name;
	}

	_videoGrabberInactiveTimer->start();
	_hyperion->setInputImage(_videoGrabberPriority, image);
}

void GrabberWrapper::setScreenGrabberEnable(bool enable)
{
	if(_screenGrabberEnabled != enable)
	{
		if(enable)
		{
			_hyperion->registerInput(_screenGrabberPriority, hyperion::COMP_SCREEN_GRABBER);
			connect(this, &GrabberWrapper::setScreenImage, this, &GrabberWrapper::handleScreenImage);
			// connect the image forwarding
			connect(this, &GrabberWrapper::setScreenImage, _hyperion, &Hyperion::forwardSystemProtoMessage);
		}
		else
		{
			disconnect(this, &GrabberWrapper::setScreenImage, this, nullptr);
			_hyperion->clear(_screenGrabberPriority);
			_screenGrabberInactiveTimer->stop();
			_screenGrabberName = "";
		}

		_screenGrabberEnabled = enable;
		_hyperion->setNewComponentState(hyperion::COMP_SCREEN_GRABBER, enable);
	}
}

void GrabberWrapper::setVideoGrabberEnable(bool enable)
{
	if(_videoGrabberEnabled != enable)
	{
		if(enable)
		{
			_hyperion->registerInput(_videoGrabberPriority, hyperion::COMP_VIDEO_GRABBER);
			connect(this, &GrabberWrapper::setVideoImage, this, &GrabberWrapper::handleVideoImage);
			// connect the image forwarding
			connect(this, &GrabberWrapper::setVideoImage, _hyperion, &Hyperion::forwardV4lProtoMessage);
		}
		else
		{
			disconnect(this, &GrabberWrapper::setVideoImage, this, nullptr);
			_hyperion->clear(_videoGrabberPriority);
			_videoGrabberInactiveTimer->stop();
			_videoGrabberName = "";
		}

		_videoGrabberEnabled = enable;
		_hyperion->setNewComponentState(hyperion::COMP_VIDEO_GRABBER, enable);
	}
}

void GrabberWrapper::componentStateChanged(hyperion::Components component, bool state)
{
	if(component == hyperion::COMP_SCREEN_GRABBER)
	{
		setScreenGrabberEnable(state);
	}
	else if(component == hyperion::COMP_VIDEO_GRABBER)
	{
		setVideoGrabberEnable(state);
	}
}

void GrabberWrapper::settingsChanged(settings::type type, const QJsonDocument& config)
{
	if(type == settings::INSTCAPTURE)
	{
		const QJsonObject& obj = config.object();

		if(_screenGrabberPriority != obj["systemPriority"].toInt(250))
		{
			setScreenGrabberEnable(false); // clear prio
			_screenGrabberPriority = obj["systemPriority"].toInt(250);
		}

		if(_videoGrabberPriority != obj["v4lPriority"].toInt(240))
		{
			setVideoGrabberEnable(false); // clear prio
			_videoGrabberPriority = obj["v4lPriority"].toInt(240);
		}

		setScreenGrabberEnable(obj["systemEnable"].toBool(false));
		setVideoGrabberEnable(obj["v4lEnable"].toBool(false));
	}
}

void GrabberWrapper::setScreenGrabberInactive()
{
	_hyperion->setInputInactive(_screenGrabberPriority);
}

void GrabberWrapper::setVideoGrabberInactive()
{
	_hyperion->setInputInactive(_videoGrabberPriority);
}

void GrabberWrapper::stopGrabberThread(settings::type type)
{
	if(type == settings::SCREENGRABBER)
	{
		// stop screen grabber & update timers
		emit stopScreenGrabber();

		// get current thread
		QThread* oldThread = _screenGrabber->thread();
		disconnect(oldThread, nullptr, nullptr, nullptr);
		oldThread->quit();
		oldThread->wait();
		delete oldThread;

		disconnect(_screenGrabber, nullptr, nullptr, nullptr);
		delete _screenGrabber;
		_screenGrabber = nullptr;
	}
	else if(type == settings::VIDEOGRABBER)
	{
		// stop video grabber
		emit stopVideoGrabber();

		// get current thread
		QThread* oldThread = _videoGrabber->thread();
		disconnect(oldThread, nullptr, nullptr, nullptr);
		oldThread->quit();
		oldThread->wait();
		delete oldThread;

		disconnect(_videoGrabber, nullptr, nullptr, nullptr);
		delete _videoGrabber;
		_videoGrabber = nullptr;
	}
}
