#pragma once

// Qt includes
#include <QThread>

// util includes
#include <utils/VideoMode.h>
#include <utils/PixelFormat.h>
#include <utils/Image.h>
#include <utils/ColorRgb.h>
#include <utils/EncoderThread.h>

class ImageResampler : public QObject
{
    Q_OBJECT
public:
	explicit ImageResampler(QObject *parent = nullptr);
	~ImageResampler();

	void setPixelDecimation(int decimator) { _pixelDecimation = decimator; }
	void setCropping(int cropLeft, int cropRight, int cropTop, int cropBottom);
	void setVideoMode(VideoMode mode) { _videoMode = mode; }
	void setFlipMode(FlipMode mode) { _flipMode = mode; }
	void processImage(const uint8_t *data, int size, int width, int height, int lineLength, PixelFormat pixelFormat) const;

signals:
	void newFrame(const Image<ColorRgb>& data);

private:
	int						_pixelDecimation,
							_cropLeft,
							_cropRight,
							_cropTop,
							_cropBottom,
							_threadCount;
	Thread<EncoderThread>**	_threads;
	VideoMode				_videoMode;
	FlipMode				_flipMode;

};
