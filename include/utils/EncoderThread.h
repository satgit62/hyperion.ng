#pragma once

// Qt includes
#include <QThread>

// util includes
#include <utils/VideoMode.h>
#include <utils/PixelFormat.h>
#include <utils/Image.h>
#include <utils/ColorRgb.h>
#include <utils/ColorSys.h>

// Determine the cmake options
#include <HyperionConfig.h>

// Turbo JPEG decoder
#ifdef HAVE_TURBO_JPEG
	#include <turbojpeg.h>
#endif

/// Encoder thread for image data
class EncoderThread : public QObject
{
	Q_OBJECT
public:
	explicit EncoderThread();
	~EncoderThread();

	void setup(
		uint8_t* sharedData, int size, int width, int height, int lineLength,
		unsigned cropLeft, unsigned cropTop, unsigned cropBottom, unsigned cropRight,
		VideoMode videoMode, FlipMode flipMode, int pixelDecimation, PixelFormat pixelFormat);

	void process();

	bool isBusy() { return _busy; }
	QAtomicInt _busy = false;

signals:
	void newFrame(const Image<ColorRgb>& data);

private:
	PixelFormat			_pixelFormat;
	uint8_t*			_localData,
						*_flipBuffer;
	int					_scalingFactorsCount,
						_width,
						_height,
						_lineLength,
						_currentFrame,
						_horizontalDecimation,
						_verticalDecimation;
	unsigned long		_size;
	unsigned			_cropLeft,
						_cropTop,
						_cropBottom,
						_cropRight;
	VideoMode			_videoMode;
	FlipMode			_flipMode;

#ifdef HAVE_TURBO_JPEG
	tjhandle			_transform, _decompress;
	tjscalingfactor*	_scalingFactors;
	tjtransform*		_xform;

	void processImageMjpeg();
#endif
};

template <typename TThread> class Thread : public QThread
{
public:
	TThread *_thread;
	explicit Thread(TThread *thread, QObject *parent = nullptr)
		: QThread(parent)
		, _thread(thread)
	{
		_thread->moveToThread(this);
		start();
	}

	~Thread()
	{
		quit();
		wait();
	}

	EncoderThread* thread() const { return qobject_cast<EncoderThread*>(_thread); }

	void setup(
		uint8_t* sharedData, int size, int width, int height, int lineLength,
		unsigned cropLeft, unsigned cropTop, unsigned cropBottom, unsigned cropRight,
		VideoMode videoMode, FlipMode flipMode, int pixelDecimation, PixelFormat pixelFormat)
	{
		auto encThread = qobject_cast<EncoderThread*>(_thread);
		if (encThread != nullptr)
			encThread->setup(sharedData,
				size, width, height, lineLength,
				cropLeft, cropTop, cropBottom, cropRight,
				videoMode, flipMode, pixelDecimation, pixelFormat);
	}

	bool isBusy()
	{
		auto encThread = qobject_cast<EncoderThread*>(_thread);
		if (encThread != nullptr)
			return encThread->isBusy();

		return true;
	}

	void process()
	{
		auto encThread = qobject_cast<EncoderThread*>(_thread);
		if (encThread != nullptr)
			encThread->process();
	}

protected:
	void run() override
	{
		QThread::run();
		delete _thread;
	}
};
