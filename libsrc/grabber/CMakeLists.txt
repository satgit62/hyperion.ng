project(grabber)

# Define the current source locations
set(CURRENT_HEADER_DIR ${CMAKE_SOURCE_DIR}/include/grabber)
set(CURRENT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libsrc/grabber)

file(GLOB Grabber_SOURCES
	"${CURRENT_HEADER_DIR}/*.h"
	"${CURRENT_SOURCE_DIR}/*.h"
	"${CURRENT_SOURCE_DIR}/*.cpp"
)

#########################
##### Video Grabber #####
#########################

if(ENABLE_V4L2)
	include_directories("${CURRENT_SOURCE_DIR}/video/v4l2")
	file (GLOB Grabber_V4L2_SOURCES "${CURRENT_SOURCE_DIR}/video/v4l2/*.h" "${CURRENT_SOURCE_DIR}/video/v4l2/*.cpp")
endif()

if(ENABLE_MF)
	include_directories("${CURRENT_SOURCE_DIR}/video/mediafoundation")
	file (GLOB Grabber_MF_SOURCES "${CURRENT_SOURCE_DIR}/video/mediafoundation/*.h" "${CURRENT_SOURCE_DIR}/video/mediafoundation/*.cpp")
endif()

# Add Turbo JPEG library
if(ENABLE_V4L2 OR ENABLE_MF)
	find_package(TurboJPEG)
	if(TURBOJPEG_FOUND)
		add_definitions(-DHAVE_TURBO_JPEG)
		message( STATUS "Using Turbo JPEG library: ${TurboJPEG_LIBRARY}")
		include_directories(${TurboJPEG_INCLUDE_DIRS})
	else()
		message( STATUS "Turbo JPEG library not found, MJPEG camera format won't work.")
	endif()
endif()

##########################
##### Screen Grabber #####
##########################

if(ENABLE_QT)
	include_directories("${CURRENT_SOURCE_DIR}/screen/qt")
	file(GLOB Grabber_QT_SOURCES "${CURRENT_SOURCE_DIR}/screen/qt/*.h" "${CURRENT_SOURCE_DIR}/screen/qt/*.cpp")
endif()

#########################
######## General ########
#########################

set(Grabber_SOURCES
	${Grabber_SOURCES}
	${Grabber_V4L2_SOURCES}
	${Grabber_MF_SOURCES}
	${Grabber_QT_SOURCES}
)

add_library(${PROJECT_NAME} ${Grabber_SOURCES})
target_link_libraries(${PROJECT_NAME}
	hyperion
	${QT_LIBRARIES}
)

if(ENABLE_V4L2 OR ENABLE_MF AND TURBOJPEG_FOUND)
	target_link_libraries(${PROJECT_NAME} ${TurboJPEG_LIBRARY})
endif()
