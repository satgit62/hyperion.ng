
#include "QtWrapper.h"

QtWrapper::QtWrapper(int updateRate_Hz, int display, int pixelDecimation, int cropLeft, int cropRight, int cropTop, int cropBottom)
	: _timer(this)
	, _grabber(display, cropLeft, cropRight, cropTop, cropBottom)
	, _screenshot(new Image<ColorRgb>(0,0))
{
	_grabber.setFramerate(updateRate_Hz);
	_grabber.setPixelDecimation(pixelDecimation);

	_timer.setTimerType(Qt::PreciseTimer);
	_timer.setSingleShot(false);
	_timer.setInterval(_grabber.getUpdateInterval());

	// Connect capturing to the timeout signal of the timer
	connect(&_timer, SIGNAL(timeout()), this, SLOT(capture()));
}

const Image<ColorRgb> & QtWrapper::getScreenshot()
{
	_grabber.grabFrame(_screenshot.data());
	return *_screenshot.data();
}

void QtWrapper::start()
{
	_timer.start();
}

void QtWrapper::stop()
{
	_timer.stop();
}

bool QtWrapper::displayInit()
{
	return _grabber.setupDisplay();
}

void QtWrapper::capture()
{
	if(unsigned(_grabber.getImageWidth()) != unsigned(_screenshot->width()) || unsigned(_grabber.getImageHeight()) != unsigned(_screenshot->height()))
		_screenshot->resize(_grabber.getImageWidth(),_grabber.getImageHeight());

	_grabber.grabFrame(_screenshot.data());
	emit sig_screenshot(*_screenshot.data());
}

void QtWrapper::setVideoMode(VideoMode mode)
{
	_grabber.setVideoMode(mode);
}
