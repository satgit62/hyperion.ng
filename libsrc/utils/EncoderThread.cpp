#include "utils/EncoderThread.h"

EncoderThread::EncoderThread()
	: _localData(nullptr)
	, _scalingFactorsCount(0)
#ifdef HAVE_TURBO_JPEG
	, _transform(nullptr)
	, _decompress(nullptr)
	, _scalingFactors(nullptr)
	, _xform(nullptr)
#endif
{}

EncoderThread::~EncoderThread()
{
#ifdef HAVE_TURBO_JPEG
	if (_transform)
		tjDestroy(_transform);

	if (_decompress)
		tjDestroy(_decompress);
#endif

	if (_localData != nullptr)
#ifdef HAVE_TURBO_JPEG
		tjFree(_localData);
#else
		free(_localData);
		_localData = nullptr;
#endif
}

void EncoderThread::setup(
	uint8_t* sharedData, int size, int width, int height, int lineLength,
	unsigned cropLeft, unsigned cropTop, unsigned cropBottom, unsigned cropRight,
	VideoMode videoMode, FlipMode flipMode, int pixelDecimation, PixelFormat pixelFormat)
{
	_lineLength = lineLength;
	_pixelFormat = pixelFormat;
	_size = (unsigned long) size;
	_width = width;
	_height = height;
	_cropLeft = cropLeft;
	_cropTop = cropTop;
	_cropBottom = cropBottom;
	_cropRight = cropRight;
	_videoMode = videoMode;
	_flipMode = flipMode;
	_horizontalDecimation = _verticalDecimation = pixelDecimation;

#ifdef HAVE_TURBO_JPEG
	if (_localData)
		tjFree(_localData);

	_localData = (uint8_t*)tjAlloc(size + 1);
#else
	if (_localData != nullptr)
	{
		free(_localData);
		_localData = nullptr;
	}
	_localData = (uint8_t*)malloc((size_t)_size + 1);
#endif

	memcpy(_localData, sharedData, size);
}

void EncoderThread::process()
{
	_busy = true;
	if (_width > 0 && _height > 0)
	{
#ifdef HAVE_TURBO_JPEG
		if (_pixelFormat == PixelFormat::MJPEG)
		{
			processImageMjpeg();
		}
		else
#endif
		{
			int xDestFlip = 0, yDestFlip = 0;
			int uOffset = 0, vOffset = 0;

			// handle 3D mode
			switch (_videoMode)
			{
				case VideoMode::VIDEO_3DSBS:
				{
					_cropRight = _width >> 1;
				}
				break;

				case VideoMode::VIDEO_3DTAB:
				{
					_cropBottom = _height >> 1;
				}
				break;

				default:
				break;
			}

			// calculate the output size
			int outputWidth = (_width - _cropLeft - _cropRight - (_horizontalDecimation >> 1) + _horizontalDecimation - 1) / _horizontalDecimation;
			int outputHeight = (_height - _cropTop - _cropBottom - (_verticalDecimation >> 1) + _verticalDecimation - 1) / _verticalDecimation;

			Image<ColorRgb> _outputImage(outputWidth, outputHeight);

			for (int yDest = 0, ySource = _cropTop + (_verticalDecimation >> 1); yDest < outputHeight; ySource += _verticalDecimation, ++yDest)
			{
				int yOffset = _lineLength * ySource;
				if (_pixelFormat == PixelFormat::NV12)
				{
					uOffset = (_height + ySource / 2) * _lineLength;
				}
				else if (_pixelFormat == PixelFormat::I420)
				{
					uOffset = _width * _height + (ySource/2) * _width/2;
					vOffset = _width * _height * 1.25 + (ySource/2) * _width/2;
				}

				for (int xDest = 0, xSource = _cropLeft + (_horizontalDecimation >> 1); xDest < outputWidth; xSource += _horizontalDecimation, ++xDest)
				{
					switch (_flipMode)
					{
						case FlipMode::HORIZONTAL:

							xDestFlip = xDest;
							yDestFlip = outputHeight-yDest-1;
							break;
						case FlipMode::VERTICAL:
							xDestFlip = outputWidth-xDest-1;
							yDestFlip = yDest;
							break;
						case FlipMode::BOTH:
							xDestFlip = outputWidth-xDest-1;
							yDestFlip = outputHeight-yDest-1;
							break;
						case FlipMode::NO_CHANGE:
							xDestFlip = xDest;
							yDestFlip = yDest;
							break;
					}

					ColorRgb &rgb = _outputImage(xDestFlip, yDestFlip);
					switch (_pixelFormat)
					{
						case PixelFormat::UYVY:
						{
							int index = yOffset + (xSource << 1);
							uint8_t y = _localData[index+1];
							uint8_t u = ((xSource&1) == 0) ? _localData[index  ] : _localData[index-2];
							uint8_t v = ((xSource&1) == 0) ? _localData[index+2] : _localData[index  ];
							ColorSys::yuv2rgb(y, u, v, rgb.red, rgb.green, rgb.blue);
						}
						break;
						case PixelFormat::YUYV:
						{
							int index = yOffset + (xSource << 1);
							uint8_t y = _localData[index];
							uint8_t u = ((xSource&1) == 0) ? _localData[index+1] : _localData[index-1];
							uint8_t v = ((xSource&1) == 0) ? _localData[index+3] : _localData[index+1];
							ColorSys::yuv2rgb(y, u, v, rgb.red, rgb.green, rgb.blue);
						}
						break;
						case PixelFormat::BGR16:
						{
							int index = yOffset + (xSource << 1);
							rgb.blue  = (_localData[index] & 0x1f) << 3;
							rgb.green = (((_localData[index+1] & 0x7) << 3) | (_localData[index] & 0xE0) >> 5) << 2;
							rgb.red   = (_localData[index+1] & 0xF8);
						}
						break;
						case PixelFormat::BGR24:
						{
							int index = yOffset + (xSource << 1) + xSource;
							rgb.blue  = _localData[index  ];
							rgb.green = _localData[index+1];
							rgb.red   = _localData[index+2];
						}
						break;
						case PixelFormat::RGB32:
						{
							int index = yOffset + (xSource << 2);
							rgb.red   = _localData[index  ];
							rgb.green = _localData[index+1];
							rgb.blue  = _localData[index+2];
						}
						break;
						case PixelFormat::BGR32:
						{
							int index = yOffset + (xSource << 2);
							rgb.blue  = _localData[index  ];
							rgb.green = _localData[index+1];
							rgb.red   = _localData[index+2];
						}
						break;
						case PixelFormat::NV12:
						{
							uint8_t y = _localData[yOffset + xSource];
							uint8_t u = _localData[uOffset + ((xSource >> 1) << 1)];
							uint8_t v = _localData[uOffset + ((xSource >> 1) << 1) + 1];
							ColorSys::yuv2rgb(y, u, v, rgb.red, rgb.green, rgb.blue);
						}
						break;
						case PixelFormat::I420:
						{
							int y = _localData[yOffset + xSource];
							int u = _localData[uOffset + (xSource >> 1)];
							int v = _localData[vOffset + (xSource >> 1)];
							ColorSys::yuv2rgb(y, u, v, rgb.red, rgb.green, rgb.blue);
							break;
						}
						break;
#ifdef HAVE_TURBO_JPEG
						case PixelFormat::MJPEG:
#endif
						case PixelFormat::NO_CHANGE:
						break;
					}
				}
			}

			emit newFrame(_outputImage);
		}
	}
	_busy = false;
}

#ifdef HAVE_TURBO_JPEG
void EncoderThread::processImageMjpeg()
{
	if (!_transform && _flipMode != FlipMode::NO_CHANGE)
	{
		_transform = tjInitTransform();
		_xform = new tjtransform();
	}

	if (_flipMode == FlipMode::BOTH || _flipMode == FlipMode::HORIZONTAL)
	{
		_xform->op = TJXOP_HFLIP;
		tjTransform(_transform, _localData, _size, 1, &_localData, &_size, _xform, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE);
	}

	if (_flipMode == FlipMode::BOTH || _flipMode == FlipMode::VERTICAL)
	{
		_xform->op = TJXOP_VFLIP;
		tjTransform(_transform, _localData, _size, 1, &_localData, &_size, _xform, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE);
	}

	if (!_decompress)
	{
		_decompress = tjInitDecompress();
		_scalingFactors = tjGetScalingFactors(&_scalingFactorsCount);
	}

	int subsamp = 0;
	if (tjDecompressHeader2(_decompress, _localData, _size, &_width, &_height, &subsamp) != 0)
		return;

	int scaledWidth = _width, scaledHeight = _height;
	if(_scalingFactors != nullptr && _pixelDecimation > 1)
	{
		for (int i = 0; i < _scalingFactorsCount ; i++)
		{
			const int tempWidth = TJSCALED(_width, _scalingFactors[i]);
			const int tempHeight = TJSCALED(_height, _scalingFactors[i]);
			if (tempWidth <= _width/_pixelDecimation && tempHeight <= _height/_pixelDecimation)
			{
				scaledWidth = tempWidth;
				scaledHeight = tempHeight;
				break;
			}
		}

		if (scaledWidth == _width && scaledHeight == _height)
		{
			scaledWidth = TJSCALED(_width, _scalingFactors[_scalingFactorsCount-1]);
			scaledHeight = TJSCALED(_height, _scalingFactors[_scalingFactorsCount-1]);
		}
	}

	Image<ColorRgb> srcImage(scaledWidth, scaledHeight);

	if (tjDecompress2(_decompress, _localData , _size, (unsigned char*)srcImage.memptr(), scaledWidth, 0, scaledHeight, TJPF_RGB, TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE) != 0)
			return;

	// got image, process it
	if (!(_cropLeft > 0 || _cropTop > 0 || _cropBottom > 0 || _cropRight > 0))
		emit newFrame(srcImage);
	else
    {
		// calculate the output size
		int outputWidth = (_width - _cropLeft - _cropRight);
		int outputHeight = (_height - _cropTop - _cropBottom);

		if (outputWidth <= 0 || outputHeight <= 0)
			return;

		Image<ColorRgb> destImage(outputWidth, outputHeight);

		for (unsigned int y = 0; y < destImage.height(); y++)
		{
			unsigned char* source = (unsigned char*)srcImage.memptr() + (y + _cropTop)*srcImage.width()*3 + _cropLeft*3;
			unsigned char* dest = (unsigned char*)destImage.memptr() + y*destImage.width()*3;
			memcpy(dest, source, destImage.width()*3);
			free(source);
			source = nullptr;
			free(dest);
			dest = nullptr;
		}

    	// emit
		emit newFrame(destImage);
	}
}
#endif


