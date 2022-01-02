#include "utils/ImageResampler.h"
#include <utils/ColorSys.h>
#include <utils/Logger.h>

ImageResampler::ImageResampler(QObject *parent)
	: QObject(parent)
	, _threadCount(qMax(QThread::idealThreadCount(), 1))
	, _threads(new Thread<EncoderThread>*[_threadCount])
	, _pixelDecimation(8)
	, _cropLeft(0)
	, _cropRight(0)
	, _cropTop(0)
	, _cropBottom(0)
	, _videoMode(VideoMode::VIDEO_2D)
	, _flipMode(FlipMode::NO_CHANGE)
{
	for (int i = 0; i < _threadCount; i++)
	{
		_threads[i] = new Thread<EncoderThread>(new EncoderThread, this);
		_threads[i]->setObjectName("Encoder " + QString::number(i));
		connect(_threads[i]->thread(), &EncoderThread::newFrame, this, &ImageResampler::newFrame);
	}
}

ImageResampler::~ImageResampler()
{
	if (_threads != nullptr)
	{
		for(int i = 0; i < _threadCount; i++)
		{
			disconnect(_threads[i]->thread(), nullptr, nullptr, nullptr);
			_threads[i]->deleteLater();
			_threads[i] = nullptr;
		}

		delete[] _threads;
		_threads = nullptr;
	}
}

void ImageResampler::setCropping(int cropLeft, int cropRight, int cropTop, int cropBottom)
{
	_cropLeft   = cropLeft;
	_cropRight  = cropRight;
	_cropTop    = cropTop;
	_cropBottom = cropBottom;
}

void ImageResampler::processImage(const uint8_t* data, int size, int width, int height, int lineLength, PixelFormat pixelFormat) const
{
	for (int i = 0; i < _threadCount; i++)
	{
		if (!_threads[i]->isBusy())
		{
			_threads[i]->setup((uint8_t*)data, size, width, height, lineLength, _cropLeft, _cropTop, _cropBottom, _cropRight, _videoMode, _flipMode, _pixelDecimation, pixelFormat);
			_threads[i]->process();
		}
	}
}
