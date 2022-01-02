#pragma once
#include <QString>

#include "HyperionConfig.h"

namespace hyperion
{

/**
 * Enumeration of components in Hyperion.
 */
enum Components
{
	COMP_INVALID,
	COMP_ALL,
	COMP_SMOOTHING,
	COMP_BLACKBORDER,
#if defined(ENABLE_FORWARDER)
	COMP_FORWARDER,
#endif
#if defined(ENABLE_BOBLIGHT_SERVER)
	COMP_BOBLIGHTSERVER,
#endif
	COMP_SCREEN_GRABBER,
	COMP_VIDEO_GRABBER,
	COMP_COLOR,
	COMP_IMAGE,
	COMP_EFFECT,
	COMP_LEDDEVICE,
#if defined(ENABLE_FLATBUF_SERVER)
	COMP_FLATBUFSERVER,
#endif
#if defined(ENABLE_PROTOBUF_SERVER)
	COMP_PROTOSERVER
#endif
};

inline const char* componentToString(Components c)
{
	switch (c)
	{
		case COMP_ALL:            return "Hyperion";
		case COMP_SMOOTHING:      return "Smoothing";
		case COMP_BLACKBORDER:    return "Blackborder detector";
#if defined(ENABLE_FORWARDER)
		case COMP_FORWARDER:      return "Json/Proto forwarder";
#endif
#if defined(ENABLE_BOBLIGHT_SERVER)
		case COMP_BOBLIGHTSERVER: return "Boblight server";
#endif
		case COMP_SCREEN_GRABBER: return "Screen Grabber";
		case COMP_VIDEO_GRABBER:  return "Video Grabber";
		case COMP_COLOR:          return "Solid color";
		case COMP_EFFECT:         return "Effect";
		case COMP_IMAGE:          return "Image";
		case COMP_LEDDEVICE:      return "LED device";
#if defined(ENABLE_FLATBUF_SERVER)
		case COMP_FLATBUFSERVER:  return "Image Receiver";
#endif
#if defined(ENABLE_PROTOBUF_SERVER)
		case COMP_PROTOSERVER:    return "Proto Server";
#endif
		default:                  return "";
	}
}

inline const char* componentToIdString(Components c)
{
	switch (c)
	{
		case COMP_ALL:            return "ALL";
		case COMP_SMOOTHING:      return "SMOOTHING";
		case COMP_BLACKBORDER:    return "BLACKBORDER";
#if defined(ENABLE_FORWARDER)
		case COMP_FORWARDER:      return "FORWARDER";
#endif
#if defined(ENABLE_BOBLIGHT_SERVER)
		case COMP_BOBLIGHTSERVER: return "BOBLIGHTSERVER";
#endif
		case COMP_SCREEN_GRABBER: return "SCREENGRABBER";
		case COMP_VIDEO_GRABBER:  return "VIDEOGRABBER";
		case COMP_COLOR:          return "COLOR";
		case COMP_EFFECT:         return "EFFECT";
		case COMP_IMAGE:          return "IMAGE";
		case COMP_LEDDEVICE:      return "LEDDEVICE";
#if defined(ENABLE_FLATBUF_SERVER)
		case COMP_FLATBUFSERVER:  return "FLATBUFSERVER";
#endif
#if defined(ENABLE_PROTOBUF_SERVER)
		case COMP_PROTOSERVER:    return "PROTOSERVER";
#endif
		default:                  return "";
	}
}

inline Components stringToComponent(const QString& component)
{
	const QString cmp = component.toUpper();
	if (cmp == "ALL")           return COMP_ALL;
	if (cmp == "SMOOTHING")     return COMP_SMOOTHING;
	if (cmp == "BLACKBORDER")   return COMP_BLACKBORDER;
#if defined(ENABLE_FORWARDER)
	if (cmp == "FORWARDER")     return COMP_FORWARDER;
#endif
#if defined(ENABLE_BOBLIGHT_SERVER)
	if (cmp == "BOBLIGHTSERVER")return COMP_BOBLIGHTSERVER;
#endif
	if (cmp == "SCREENGRABBER") return COMP_SCREEN_GRABBER;
	if (cmp == "VIDEOGRABBER")  return COMP_VIDEO_GRABBER;
	if (cmp == "COLOR")         return COMP_COLOR;
	if (cmp == "EFFECT")        return COMP_EFFECT;
	if (cmp == "IMAGE")         return COMP_IMAGE;
	if (cmp == "LEDDEVICE")     return COMP_LEDDEVICE;
#if defined(ENABLE_FLATBUF_SERVER)
	if (cmp == "FLATBUFSERVER") return COMP_FLATBUFSERVER;
#endif
#if defined(ENABLE_PROTOBUF_SERVER)
	if (cmp == "PROTOSERVER")   return COMP_PROTOSERVER;
#endif
	return COMP_INVALID;
}

} // end of namespace
