#pragma once

// Build configuration
#include <HyperionConfig.h>

// #ifdef ENABLE_DISPMANX
// 	#include <grabber/DispmanxWrapper.h>
// #else
// 	typedef QObject DispmanxWrapper;
// #endif

// #if defined(ENABLE_V4L2) || defined(ENABLE_MF)
// 	#include <grabber/VideoWrapper.h>
// #else
// 	typedef QObject VideoWrapper;
// #endif

// #ifdef ENABLE_FB
// 	#include <grabber/FramebufferWrapper.h>
// #else
// 	typedef QObject FramebufferWrapper;
// #endif

// #ifdef ENABLE_AMLOGIC
// 	#include <grabber/AmlogicWrapper.h>
// #else
// 	typedef QObject AmlogicWrapper;
// #endif

// #ifdef ENABLE_OSX
// 	#include <grabber/OsxWrapper.h>
// #else
// 	typedef QObject OsxWrapper;
// #endif

// #ifdef ENABLE_X11
// 	#include <grabber/X11Wrapper.h>
// #else
// 	typedef QObject X11Wrapper;
// #endif

// #ifdef ENABLE_XCB
// 	#include <grabber/XcbWrapper.h>
// #else
// 	typedef QObject XcbWrapper;
// #endif

// #ifdef ENABLE_DX
// 	#include <grabber/DirectXWrapper.h>
// #else
// 	typedef QObject DirectXWrapper;
// #endif

#ifdef ENABLE_QT
	#include <QtGrabber.h>
#else
	typedef QObject QtGrabber;
#endif

// utils
#include <utils/settings.h>

class Grabber;
class QJsonObject;

///
/// The GrabberFactory is responsible for constructing 'Grabber'
///
class GrabberFactory
{
public:
	///
	/// Constructs a Grabber based on the given configuration
	///
	/// @param grabberConfig The configuration of the grabber
	///
	/// @return The constructed Grabber or nullptr if configuration is invalid. The ownership of
	/// the constructed Grabber is transferred to the caller
	///
	static Grabber* construct(const QJsonObject & grabberConfig, settings::type settingsType);
};
