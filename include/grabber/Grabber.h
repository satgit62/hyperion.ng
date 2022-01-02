#pragma once

// qt includes
#include <QObject>
#include <QJsonObject>

// stl includes
#include <cstdint>

// utils includes
#include <utils/ColorRgb.h>
#include <utils/Image.h>
#include <utils/VideoMode.h>
#include <utils/VideoStandard.h>
#include <utils/ImageResampler.h>
#include <utils/Logger.h>
#include <utils/Components.h>

///
/// @brief Interface (pure virtual base class) for Screen/Video Grabber.

class Grabber : public QObject
{
	Q_OBJECT
public:
	///
	/// @brief Constructor of the Screen/Video Grabber
	///
	Grabber(const QString& grabberName = "", int cropLeft=0, int cropRight=0, int cropTop=0, int cropBottom=0);

	static const int DEFAULT_RATE_HZ;
	static const int DEFAULT_MIN_GRAB_RATE_HZ;
	static const int DEFAULT_MAX_GRAB_RATE_HZ;
	static const int DEFAULT_PIXELDECIMATION;

	///
	/// @brief Set the video mode (2D/3D)
	/// @param[in] mode The new video mode
	///
	virtual void setVideoMode(VideoMode mode);

	///
	/// @brief Apply new flip mode (vertical/horizontal/both)
	/// @param[in] mode The new flip mode
	///
	virtual void setFlipMode(FlipMode mode);

	///
	/// @brief Apply new crop values, on errors reject the values
	///
	virtual void setCropping(int cropLeft, int cropRight, int cropTop, int cropBottom);

	///
	/// @brief Apply new video input (used from v4l2/MediaFoundation)
	/// @param[in] input device input
	///
	virtual bool setInput(int input);

	///
	/// @brief Apply new width/height values, on errors (collide with cropping) reject the values
	/// @return True on success else false
	///
	virtual bool setWidthHeight(int width, int height);

	///
	/// @brief Apply new capture framerate in Hz
	/// @param fps framesPerSecond
	///
	virtual bool setFramerate(int fps);

	///
	/// @brief Apply new framerate software decimation (used from v4l2/MediaFoundation)
	/// @param decimation how many frames per second to omit
	///
	virtual void setFpsSoftwareDecimation(int decimation);

	///
	/// @brief Apply videoStandard (used from v4l2)
	///
	virtual void setVideoStandard(VideoStandard videoStandard);

	///
	/// @brief  Apply new pixelDecimation
	///
	virtual bool setPixelDecimation(int pixelDecimation);

	///
	/// @brief Apply display index (used from qt)
	///
	virtual bool setDisplayIndex(int /*index*/) { return true; }

	///
	/// @brief Prevent the real capture implementation from capturing if disabled
	///
	virtual void setEnabled(bool enable);

	///
	/// @brief get current resulting height of image (after crop)
	///
	int getImageWidth() const { return _width; }

	///
	/// @brief get current resulting width of image (after crop)
	///
	int getImageHeight() const { return _height; }

	///
	/// @brief Get current capture framerate in Hz
	/// @param fps framesPerSecond
	///
	int getFramerate() const { return _fps; }

	///
	/// @brief Get capture interval in ms
	///
	int getUpdateInterval() const { return 1000/_fps; }

	///
	/// @brief  Get pixelDecimation
	///
	int getPixelDecimation() const { return _pixelDecimation; }

	QString getGrabberName() const { return _grabberName; }

public slots:
	///
	/// @brief Is called on thread start, all construction tasks and init should run here.
	///
	virtual void start();

	///
	/// @brief Stops the grabber.
	///
	virtual void stop();

signals:
	void newImage(const QString& name, const Image<ColorRgb>& image);

protected slots:
	void receiveImage(const Image<ColorRgb>& image) { emit newImage(_grabberName, image); }

	///
	/// @brief Set device in error state
	/// @param[in] errorMsg The error message to be logged
	///
	virtual void setInError( const QString& errorMsg);

protected:
	///
	/// @brief Opens the output device.
	/// @return Zero, on success (i.e. device is ready), else negative
	///
	virtual bool open() { return true; }

	///
	/// @brief Closes the output device.
	/// @return Zero on success (i.e. device is closed), else negative
	///
	virtual bool close() { return true; }

	QString _grabberName;

	/// logger instance
	Logger* _log;

	ImageResampler _imageResampler;
	bool _useImageResampler;

	/// the selected VideoMode
	VideoMode _videoMode;

	/// the used video standard
	VideoStandard _videoStandard;

	/// Image size decimation
	int _pixelDecimation;

	/// the used Flip Mode
	FlipMode _flipMode;

	/// Width of the captured snapshot [pixels]
	int _width;

	/// Height of the captured snapshot [pixels]
	int _height;

	/// frame per second
	int _fps;

	/// fps software decimation
	int _fpsSoftwareDecimation;

	/// device input
	int _input;

	/// number of pixels to crop after capturing
	int _cropLeft, _cropRight, _cropTop, _cropBottom;

	////////// Device states //////////

	/// Is the device enabled?
	bool _isEnabled;

	/// Is the device in error state and stopped?
	bool _isDeviceInError;

};
