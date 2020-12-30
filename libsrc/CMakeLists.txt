
# Define the current source locations
SET(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include)
SET(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libsrc)

add_subdirectory(hyperion)
add_subdirectory(commandline)
add_subdirectory(blackborder)
add_subdirectory(jsonserver)
add_subdirectory(flatbufserver)
add_subdirectory(protoserver)
add_subdirectory(ssdp)
add_subdirectory(boblightserver)
add_subdirectory(leddevice)
add_subdirectory(utils)
add_subdirectory(effectengine)
add_subdirectory(grabber)
add_subdirectory(webserver)
add_subdirectory(db)
add_subdirectory(api)
add_subdirectory(python)

# QMdnsEngine is currently not supported under MacOS because no Devs
if(NOT APPLE)
	add_subdirectory(mdns)
endif()

if(ENABLE_CEC)
	add_subdirectory(cec)
endif()

if(ENABLE_AVAHI)
	add_subdirectory(bonjour)
endif()

if(ENABLE_EXPERIMENTAL)
	add_subdirectory(experimental)
endif()

