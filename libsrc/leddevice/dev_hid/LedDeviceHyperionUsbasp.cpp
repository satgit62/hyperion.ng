// stl includes
#include <exception>
#include <cstring>

// Local Hyperion includes
#include "LedDeviceHyperionUsbasp.h"

// Constants which define the Hyperion USBASP device
namespace {
const uint16_t USB_VENDOR_ID = 0x16c0;
const uint16_t USB_PRODUCT_ID = 0x05dc;
const char USB_PRODUCT_DESCRIPTION[] = "Hyperion led controller";

const int INTERFACE_NUMBER = 0;

// Commands to the Device
enum Commands {
	CMD_WRITE_WS2801 = 10,
	CMD_WRITE_WS2812 = 11
};

}

LedDeviceHyperionUsbasp::LedDeviceHyperionUsbasp(const QJsonObject &deviceConfig)
	: LedDevice(deviceConfig)
	  , _libusbContext(nullptr)
	  , _device(nullptr)
	  , _deviceHandle(nullptr)
	  , _busNumber(-1)
	  , _addressNumber(-1)
	  , _serialNumber()
	  , _writeLedsCommand(CMD_WRITE_WS2801)
{
}

LedDeviceHyperionUsbasp::~LedDeviceHyperionUsbasp()
{
	if (_libusbContext != nullptr)
	{
		libusb_exit(_libusbContext);
	}
}

LedDevice* LedDeviceHyperionUsbasp::construct(const QJsonObject &deviceConfig)
{
	return new LedDeviceHyperionUsbasp(deviceConfig);
}

bool LedDeviceHyperionUsbasp::init(const QJsonObject &deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if ( LedDevice::init(deviceConfig) )
	{
		QString ledType = deviceConfig["ledType"].toString("ws2801");
		if (ledType != "ws2801" && ledType != "ws2812")
		{
			QString errortext = QString ("Invalid LED-type; must be 'ws2801' or 'ws2812'.");
			this->setInError(errortext);
			isInitOK = false;
		}
		else
		{
			_writeLedsCommand = (ledType == "ws2801") ? CMD_WRITE_WS2801 : CMD_WRITE_WS2812;

			int error;
			// initialize the USB context
			if ( (error = libusb_init(&_libusbContext)) != LIBUSB_SUCCESS )
			{
				_libusbContext = nullptr;

				QString errortext = QString ("Error while initializing USB context(%1):%2").arg(error).arg(libusb_error_name(error));
				this->setInError(errortext);
				isInitOK = false;
			}
			else
			{
				Debug(_log, "USB context initialized");

				if ( _log->getLogLevel() == Logger::LogLevel::DEBUG )
				{
					int logLevel = LIBUSB_LOG_LEVEL_DEBUG;
					#if LIBUSB_API_VERSION >= 0x01000106
						libusb_set_option(_libusbContext, LIBUSB_OPTION_LOG_LEVEL, logLevel);
					#else
						libusb_set_debug(_libusbContext, logLevel);
					#endif
				}

				// retrieve the list of USB devices
				libusb_device ** deviceList;
				ssize_t deviceCount = libusb_get_device_list(_libusbContext, &deviceList);

				Debug(_log, "USB devices found: %d", deviceCount );

				bool deviceFound = true;

				// iterate the list of devices
				for (ssize_t i = 0 ; i < deviceCount; ++i)
				{
					// try to open and initialize the device
					deviceFound = searchDevice(deviceList[i], _serialNumber);
					if ( deviceFound )
					{
						_device = deviceList[i];
						// a device was successfully opened. break from list
						break;
					}
				}

				// free the device list
				libusb_free_device_list(deviceList, 1);

				if (!deviceFound)
				{
					QString errortext;
					errortext = QString ("No %1 devices were found").arg(getActiveDeviceType());
					this->setInError( errortext );
				}
				else
				{
					isInitOK = true;
				}
			}
		}
	}

	return isInitOK;
}

int LedDeviceHyperionUsbasp::open()
{
	int retval = -1;
	_isDeviceReady = false;

	if ( _device != nullptr)
	{
		openDevice(_device, &_deviceHandle);

		if ( _deviceHandle == nullptr )
		{
			QString errortext = QString ("Failed to open %1 device.").arg(this->getActiveDeviceType());
			this->setInError(errortext);
			retval = -1;
		}
		else
		{
			// Everything is OK
			_isDeviceReady = true;

			Info(_log, "%s device successfully opened", QSTRING_CSTR(this->getActiveDeviceType()));
			retval = 0;
		}
	}
	return retval;
}

int LedDeviceHyperionUsbasp::close()
{
	int retval = 0;
	_isDeviceReady = false;

	if ( _deviceHandle != nullptr)
	{
		closeDevice(_deviceHandle);
		_deviceHandle = nullptr;
	}

	return retval;
}

bool LedDeviceHyperionUsbasp::searchDevice(libusb_device * device, const QString & requestedSerialNumber)
{
	Debug(_log,"");
	bool deviceFound = false;

	libusb_device_descriptor deviceDescriptor;
	int error = libusb_get_device_descriptor(device, &deviceDescriptor);
	if (error != LIBUSB_SUCCESS)
	{
		Error(_log, "Error while retrieving device descriptor(%d): %s", error, libusb_error_name(error));
		return false;
	}

	if (deviceDescriptor.idVendor == USB_VENDOR_ID && deviceDescriptor.idProduct == USB_PRODUCT_ID)
	{
		Info(_log, "Found a HID-device with matching vendor & product IDs. Retrieving more information...", QSTRING_CSTR(getActiveDeviceType()));

		Debug(_log, "vendorIdentifier : %s", QSTRING_CSTR(QString("0x%1").arg(static_cast<ushort>(deviceDescriptor.idVendor),0,16)));
		Debug(_log, "productIdentifier: %s", QSTRING_CSTR(QString("0x%1").arg(static_cast<ushort>(deviceDescriptor.idProduct),0,16)));
		Debug(_log, "release_number   : %s", QSTRING_CSTR(QString("0x%1").arg(static_cast<ushort>(deviceDescriptor.bcdDevice),0,16)));
		Debug(_log, "manufacturer     : %s", QSTRING_CSTR(getProperty(device, deviceDescriptor.iManufacturer)));

		QString product = LedDeviceHyperionUsbasp::getProperty(device, deviceDescriptor.iProduct);
		Debug(_log, "product          : %s", QSTRING_CSTR(product));

		QString serialNumber = LedDeviceHyperionUsbasp::getProperty(device, deviceDescriptor.iSerialNumber);
		Debug(_log, "serial_number    : %s", QSTRING_CSTR(serialNumber));

		// get the hardware address
		int busNumber = libusb_get_bus_number(device);
		int addressNumber = libusb_get_device_address(device);

		Debug(_log,"Checking: %s device found: bus=%d address=%d product=%s serial=%s", QSTRING_CSTR(getActiveDeviceType()), busNumber, addressNumber, QSTRING_CSTR(product), QSTRING_CSTR(serialNumber));

		// check if this is the device we are looking for
		if ( product != USB_PRODUCT_DESCRIPTION)
		{
			Error(_log,"%s device is not a '%s'", QSTRING_CSTR(getActiveDeviceType()), USB_PRODUCT_DESCRIPTION);
		}
		else
		{
			if (requestedSerialNumber.isEmpty() || requestedSerialNumber == serialNumber)
			{
				libusb_device_handle * deviceHandle;
				if ( openDevice(device, &deviceHandle ) == 0 )
				{
					_serialNumber = serialNumber;
					_busNumber = busNumber;
					_addressNumber = addressNumber;

					closeDevice(deviceHandle);

					Debug(_log, "%s device found: bus=%d address=%d serial=%s", QSTRING_CSTR(getActiveDeviceType()), _busNumber, _addressNumber, QSTRING_CSTR(_serialNumber));
					deviceFound = true;

				}
				else
				{
					Warning(_log, "Unable to open %s device. Searching for other device", QSTRING_CSTR(getActiveDeviceType()));
				}
			}
		}
	}

	return deviceFound;
}

int LedDeviceHyperionUsbasp::write(const std::vector<ColorRgb> &ledValues)
{
	int rc = 0;
	Debug(_log, "request: [%u], data size [%d], _ledCount [%u]",_writeLedsCommand, ledValues.size(), _ledCount);

	int error = libusb_control_transfer(
				_deviceHandle, // device handle
				LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT, // request type
				_writeLedsCommand, // request
				0, // value
				0, // index
				(uint8_t *) ledValues.data(), // data
				(3*_ledCount) & 0xffff, // length
				5000); // timeout

	// Disabling interrupts for a little while on the device results in a PIPE error. All seems to keep functioning though...
	if(error < 0 && error != LIBUSB_ERROR_PIPE)
	{
		rc = -1;
		Error(_log, "Unable to write to %s device (%d): %s", QSTRING_CSTR(getActiveDeviceType()), error, libusb_error_name(error));
	}

	return rc;
}

int LedDeviceHyperionUsbasp::openDevice(libusb_device *device, libusb_device_handle ** deviceHandle)
{
	Debug(_log,"");
	int rc = 0;

	libusb_device_handle * handle = nullptr;
	int error = libusb_open(device, &handle);
	if (error != LIBUSB_SUCCESS)
	{
		Error(_log, "%s: unable to open device(%d): %s", QSTRING_CSTR(getActiveDeviceType()), error, libusb_error_name(error));
		rc = -1;
	}
	else
	{
		// detach kernel driver if it is active
		if (libusb_kernel_driver_active(handle, INTERFACE_NUMBER) == 1)
		{
			error = libusb_detach_kernel_driver(handle, INTERFACE_NUMBER);
			if (error != LIBUSB_SUCCESS)
			{
				Error(_log, "%s: unable to detach kernel driver(%d): %s", QSTRING_CSTR(getActiveDeviceType()), error, libusb_error_name(error));
				libusb_close(handle);
				rc = -1;
			}
		}

		error = libusb_claim_interface(handle, INTERFACE_NUMBER);
		if (error != LIBUSB_SUCCESS)
		{
			Error(_log, "%s: unable to claim interface(%d): %s", QSTRING_CSTR(getActiveDeviceType()), error, libusb_error_name(error));
			libusb_attach_kernel_driver(handle, INTERFACE_NUMBER);
			libusb_close(handle);
			rc = -1;
		}
	}
	*deviceHandle = handle;

	return rc;
}

int LedDeviceHyperionUsbasp::closeDevice(libusb_device_handle * deviceHandle)
{
	Debug(_log,"");
	int rc = 0;

	int error = libusb_release_interface(deviceHandle, INTERFACE_NUMBER);
	if (error != LIBUSB_SUCCESS)
	{
		Debug(_log, "Error while releasing interface (%d): %s", error, libusb_error_name(error));
		rc = -1;
	}

	error = libusb_attach_kernel_driver(deviceHandle, INTERFACE_NUMBER);
	if (error != LIBUSB_SUCCESS)
	{
		Debug(_log, "Error while attaching kernel driver (%d): %s", error, libusb_error_name(error));
		rc = -1;
	}

	libusb_close(deviceHandle);

	return rc;
}

QString LedDeviceHyperionUsbasp::getProperty(libusb_device * device, int stringDescriptorIndex)
{
	QString value;

	if ( stringDescriptorIndex != 0 )
	{
		libusb_device_handle * handle = nullptr;
		if ( libusb_open(device, &handle) == LIBUSB_SUCCESS )
		{
			char buffer[256];
			int error = libusb_get_string_descriptor_ascii(handle, stringDescriptorIndex, reinterpret_cast<unsigned char *>(buffer), sizeof(buffer));

			if (error > 0)
			{
				value = QString(QByteArray(buffer, error));
			}
			libusb_close(handle);
		}
		else
		{
			Debug(_log,"libusb_open: NO LIBUSB_SUCCESS");
		}
	}
	return value;
}
