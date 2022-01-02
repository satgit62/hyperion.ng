#include <cassert>
#include <stdlib.h>

#include "hyperiond.h"

#include <QCoreApplication>
#include <QResource>
#include <QLocale>
#include <QFile>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <cstdint>
#include <limits>
#include <QThread>

#include <utils/Components.h>
#include <utils/JsonUtils.h>
#include <utils/Image.h>

#include <HyperionConfig.h> // Required to determine the cmake options

// bonjour browser
#ifdef ENABLE_AVAHI
#include <bonjour/bonjourbrowserwrapper.h>
#endif
#include <jsonserver/JsonServer.h>
#include <webserver/WebServer.h>

// Flatbuffer Server
#ifdef ENABLE_FLATBUF_SERVER
#include <flatbufserver/FlatBufferServer.h>
#endif

// Protobuffer Server
#ifdef ENABLE_PROTOBUF_SERVER
#include <protoserver/ProtoServer.h>
#endif

// ssdp
#include <ssdp/SSDPHandler.h>

// settings
#include <hyperion/SettingsManager.h>

// AuthManager
#include <hyperion/AuthManager.h>

// InstanceManager Hyperion
#include <hyperion/HyperionIManager.h>

// NetOrigin checks
#include <utils/NetOrigin.h>

// Init Python
#include <python/PythonInit.h>

// EffectFileHandler
#include <effectengine/EffectFileHandler.h>

#ifdef ENABLE_CEC
#include <cec/CECHandler.h>
#endif

HyperionDaemon *HyperionDaemon::daemon = nullptr;

HyperionDaemon::HyperionDaemon(const QString& rootPath, QObject* parent, bool logLvlOverwrite, bool readonlyMode)
	: QObject(parent), _log(Logger::getInstance("DAEMON"))
	  , _instanceManager(new HyperionIManager(rootPath, this, readonlyMode))
	  , _authManager(new AuthManager(this, readonlyMode))
#ifdef ENABLE_AVAHI
	  , _bonjourBrowserWrapper(new BonjourBrowserWrapper())
#endif
	  , _netOrigin(new NetOrigin(this))
	  , _pyInit(new PythonInit())
	  , _webserver(nullptr)
	  , _sslWebserver(nullptr)
	  , _jsonServer(nullptr)
	  , _ssdp(nullptr)
#ifdef ENABLE_CEC
	  , _cecHandler(nullptr)
#endif
	  , _currVideoMode(VideoMode::VIDEO_2D)
{
	HyperionDaemon::daemon = this;

	// Register metas for thread queued connection
	qRegisterMetaType<Image<ColorRgb>>("Image<ColorRgb>");
	qRegisterMetaType<hyperion::Components>("hyperion::Components");
	qRegisterMetaType<settings::type>("settings::type");
	qRegisterMetaType<VideoMode>("VideoMode");
	qRegisterMetaType<QMap<quint8, QJsonObject>>("QMap<quint8,QJsonObject>");
	qRegisterMetaType<std::vector<ColorRgb>>("std::vector<ColorRgb>");

	// init settings, this settingsManager accesses global settings which are independent from instances
	_settingsManager = new SettingsManager(GLOABL_INSTANCE_ID, this, readonlyMode);

	// set inital log lvl if the loglvl wasn't overwritten by arg
	if (!logLvlOverwrite)
	{
		handleSettingsUpdate(settings::LOGGER, getSetting(settings::LOGGER));
	}

	createCecHandler();

	// init EffectFileHandler
	EffectFileHandler* efh = new EffectFileHandler(rootPath, getSetting(settings::EFFECTS), this);
	connect(this, &HyperionDaemon::settingsChanged, efh, &EffectFileHandler::handleSettingsUpdate);

	// connect and apply settings for AuthManager
	connect(this, &HyperionDaemon::settingsChanged, _authManager, &AuthManager::handleSettingsUpdate);
	_authManager->handleSettingsUpdate(settings::NETWORK, _settingsManager->getSetting(settings::NETWORK));

	// connect and apply settings for NetOrigin
	connect(this, &HyperionDaemon::settingsChanged, _netOrigin, &NetOrigin::handleSettingsUpdate);
	_netOrigin->handleSettingsUpdate(settings::NETWORK, _settingsManager->getSetting(settings::NETWORK));

	// spawn all Hyperion instances (non blocking)
	_instanceManager->startAll();

	//Cleaning up Hyperion before quit
	connect(parent, SIGNAL(aboutToQuit()), this, SLOT(freeObjects()));

	// pipe settings changes from HyperionIManager to Daemon
	connect(_instanceManager, &HyperionIManager::settingsChanged, this, &HyperionDaemon::settingsChanged);

	// forward videoModes from HyperionIManager to Daemon evaluation
	connect(_instanceManager, &HyperionIManager::requestVideoMode, this, &HyperionDaemon::setVideoMode);
	// return videoMode changes from Daemon to HyperionIManager
	connect(this, &HyperionDaemon::videoMode, _instanceManager, &HyperionIManager::newVideoMode);

	// ---- grabber -----
#if !defined(ENABLE_DISPMANX) && !defined(ENABLE_OSX) && !defined(ENABLE_FB) && !defined(ENABLE_X11) && !defined(ENABLE_XCB) && !defined(ENABLE_AMLOGIC) && !defined(ENABLE_QT) && !defined(ENABLE_DX)
	Info(_log, "No platform capture supported on this platform");
#endif

	// listen for settings changes of cec and init once
	connect(this, &HyperionDaemon::settingsChanged, this, &HyperionDaemon::handleSettingsUpdate);
	handleSettingsUpdate(settings::VIDEOGRABBER, getSetting(settings::VIDEOGRABBER));

	// ---- network services -----
	startNetworkServices();
}

HyperionDaemon::~HyperionDaemon()
{
	delete _settingsManager;
	delete _pyInit;
}

void HyperionDaemon::setVideoMode(VideoMode mode)
{
	if (_currVideoMode != mode)
	{
		_currVideoMode = mode;
		emit videoMode(mode);
	}
}

QJsonDocument HyperionDaemon::getSetting(settings::type type) const
{
	return _settingsManager->getSetting(type);
}

void HyperionDaemon::freeObjects()
{
	Debug(_log, "Cleaning up Hyperion before quit.");

	// destroy network first as a client might want to access hyperion
	delete _jsonServer;
	_jsonServer = nullptr;

#if defined(ENABLE_FLATBUF_SERVER)
	if (_flatBufferServer != nullptr)
	{
		auto flatBufferServerThread = _flatBufferServer->thread();
		flatBufferServerThread->quit();
		flatBufferServerThread->wait();
		delete flatBufferServerThread;
		_flatBufferServer = nullptr;
	}
#endif

#if defined(ENABLE_PROTOBUF_SERVER)
	if (_protoServer != nullptr)
	{
		auto protoServerThread = _protoServer->thread();
		protoServerThread->quit();
		protoServerThread->wait();
		delete protoServerThread;
		_protoServer = nullptr;
	}
#endif

	//ssdp before webserver
	if (_ssdp != nullptr)
	{
		auto ssdpThread = _ssdp->thread();
		ssdpThread->quit();
		ssdpThread->wait();
		delete ssdpThread;
		_ssdp = nullptr;
	}

	if (_webserver != nullptr)
	{
		auto webserverThread = _webserver->thread();
		webserverThread->quit();
		webserverThread->wait();
		delete webserverThread;
		_webserver = nullptr;
	}

	if (_sslWebserver != nullptr)
	{
		auto sslWebserverThread = _sslWebserver->thread();
		sslWebserverThread->quit();
		sslWebserverThread->wait();
		delete sslWebserverThread;
		_sslWebserver = nullptr;
	}

#ifdef ENABLE_CEC
	if (_cecHandler != nullptr)
	{
		auto cecHandlerThread = _cecHandler->thread();
		cecHandlerThread->quit();
		cecHandlerThread->wait();
		delete cecHandlerThread;
		delete _cecHandler;
		_cecHandler = nullptr;
	}
#endif

	// stop Hyperions (non blocking)
	_instanceManager->stopAll();

#ifdef ENABLE_AVAHI
	delete _bonjourBrowserWrapper;
	_bonjourBrowserWrapper = nullptr;
#endif
}

void HyperionDaemon::startNetworkServices()
{
	// Create Json server
	_jsonServer = new JsonServer(getSetting(settings::JSONSERVER));
	connect(this, &HyperionDaemon::settingsChanged, _jsonServer, &JsonServer::handleSettingsUpdate);

#if defined(ENABLE_FLATBUF_SERVER)
	// Create FlatBuffer server in thread
	_flatBufferServer = new FlatBufferServer(getSetting(settings::FLATBUFSERVER));
	QThread* fbThread = new QThread(this);
	fbThread->setObjectName("FlatBufferServerThread");
	_flatBufferServer->moveToThread(fbThread);
	connect(fbThread, &QThread::started, _flatBufferServer, &FlatBufferServer::initServer);
	connect(fbThread, &QThread::finished, _flatBufferServer, &FlatBufferServer::deleteLater);
	connect(this, &HyperionDaemon::settingsChanged, _flatBufferServer, &FlatBufferServer::handleSettingsUpdate);
	fbThread->start();
#endif

#if defined(ENABLE_PROTOBUF_SERVER)
	// Create Proto server in thread
	_protoServer = new ProtoServer(getSetting(settings::PROTOSERVER));
	QThread* pThread = new QThread(this);
	pThread->setObjectName("ProtoServerThread");
	_protoServer->moveToThread(pThread);
	connect(pThread, &QThread::started, _protoServer, &ProtoServer::initServer);
	connect(pThread, &QThread::finished, _protoServer, &ProtoServer::deleteLater);
	connect(this, &HyperionDaemon::settingsChanged, _protoServer, &ProtoServer::handleSettingsUpdate);
	pThread->start();
#endif

	// Create Webserver in thread
	_webserver = new WebServer(getSetting(settings::WEBSERVER), false);
	QThread* wsThread = new QThread(this);
	wsThread->setObjectName("WebServerThread");
	_webserver->moveToThread(wsThread);
	connect(wsThread, &QThread::started, _webserver, &WebServer::initServer);
	connect(wsThread, &QThread::finished, _webserver, &WebServer::deleteLater);
	connect(this, &HyperionDaemon::settingsChanged, _webserver, &WebServer::handleSettingsUpdate);
	wsThread->start();

	// Create SSL Webserver in thread
	_sslWebserver = new WebServer(getSetting(settings::WEBSERVER), true);
	QThread* sslWsThread = new QThread(this);
	sslWsThread->setObjectName("SSLWebServerThread");
	_sslWebserver->moveToThread(sslWsThread);
	connect(sslWsThread, &QThread::started, _sslWebserver, &WebServer::initServer);
	connect(sslWsThread, &QThread::finished, _sslWebserver, &WebServer::deleteLater);
	connect(this, &HyperionDaemon::settingsChanged, _sslWebserver, &WebServer::handleSettingsUpdate);
	sslWsThread->start();

	// Create SSDP server in thread
	_ssdp = new SSDPHandler(_webserver,
							   getSetting(settings::FLATBUFSERVER).object()["port"].toInt(),
							   getSetting(settings::PROTOSERVER).object()["port"].toInt(),
							   getSetting(settings::JSONSERVER).object()["port"].toInt(),
							   getSetting(settings::WEBSERVER).object()["sslPort"].toInt(),
							   getSetting(settings::GENERAL).object()["name"].toString());
	QThread* ssdpThread = new QThread(this);
	ssdpThread->setObjectName("SSDPThread");
	_ssdp->moveToThread(ssdpThread);
	connect(ssdpThread, &QThread::started, _ssdp, &SSDPHandler::initServer);
	connect(ssdpThread, &QThread::finished, _ssdp, &SSDPHandler::deleteLater);
	connect(_webserver, &WebServer::stateChange, _ssdp, &SSDPHandler::handleWebServerStateChange);
	connect(this, &HyperionDaemon::settingsChanged, _ssdp, &SSDPHandler::handleSettingsUpdate);
	ssdpThread->start();
}

void HyperionDaemon::handleSettingsUpdate(settings::type settingsType, const QJsonDocument& config)
{
	if (settingsType == settings::LOGGER)
	{
		const QJsonObject& logConfig = config.object();

		std::string level = logConfig["level"].toString("warn").toStdString(); // silent warn verbose debug
		if (level == "silent")
		{
			Logger::setLogLevel(Logger::OFF);
		}
		else if (level == "warn")
		{
			Logger::setLogLevel(Logger::LogLevel::WARNING);
		}
		else if (level == "verbose")
		{
			Logger::setLogLevel(Logger::INFO);
		}
		else if (level == "debug")
		{
			Logger::setLogLevel(Logger::DEBUG);
		}
	}

	if (settingsType == settings::VIDEOGRABBER)
	{
#ifdef ENABLE_CEC
		const QJsonObject& grabberConfig = config.object();
		if (_cecHandler != nullptr && grabberConfig["cecDetection"].toBool(false))
		{
			QMetaObject::invokeMethod(_cecHandler, "start", Qt::QueuedConnection);
		}
		else
		{
			QMetaObject::invokeMethod(_cecHandler, "stop", Qt::QueuedConnection);
		}
#endif
	}
}

void HyperionDaemon::createCecHandler()
{
#if defined(ENABLE_V4L2) && defined(ENABLE_CEC)
	_cecHandler = new CECHandler;

	QThread* thread = new QThread(this);
	thread->setObjectName("CECThread");
	_cecHandler->moveToThread(thread);
	thread->start();

	connect(_cecHandler, &CECHandler::cecEvent, [&](CECEvent event) {
		if (_videoGrabber != nullptr)
		{
			_videoGrabber->handleCecEvent(event);
		}
	});

	Info(_log, "CEC handler created");
#else
	Debug(_log, "The CEC handler is not supported on this platform");
#endif
}
