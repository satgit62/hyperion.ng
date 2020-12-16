#ifndef LEDEVICEHYPERIONUSBASP_H
#define LEDEVICEHYPERIONUSBASP_H

// stl includes
#include <vector>
#include <cstdint>

// libusb include
#include <libusb.h>

// Hyperion includes
#include <leddevice/LedDevice.h>

///
/// LedDevice implementation for a USBASP programmer with modified firmware (https://github.com/poljvd/hyperion-usbasp)
///
class LedDeviceHyperionUsbasp : public LedDevice
{
public:

	///
	/// Constructs a Hyperion USBASP LedDevice
	///
	/// @param deviceConfig json device config
	///
	explicit LedDeviceHyperionUsbasp(const QJsonObject &deviceConfig);

	///
	/// @brief Destructor of the LedDevice
	///
	~LedDeviceHyperionUsbasp() override;

	///
	/// @brief Constructs the LED-device
	///
	/// @param[in] deviceConfig Device's configuration as JSON-Object
	/// @return LedDevice constructed
	///
	static LedDevice* construct(const QJsonObject &deviceConfig);

	///
	/// @brief Initialise the device's configuration
	///
	/// @param[in] deviceConfig the JSON device configuration
	/// @return True, if success
	///
	bool init(const QJsonObject &deviceConfig) override;

	///
	/// @brief Opens the output device.
	///
	/// @return Zero on success (i.e. device is ready), else negative
	///
	int open() override;

	///
	/// @brief Closes the output device.
	///
	/// @return Zero on success (i.e. device is closed), else negative
	///
	int close() override;

protected:

	///
	/// @brief Writes the RGB-Color values to the LEDs.
	///
	/// @param[in] ledValues The RGB-color per LED
	/// @return Zero on success, else negative
	///
	int write(const std::vector<ColorRgb> & ledValues) override;

private:

	///
	/// Search for a LightPack Device (first one found or matching a given serial number)
	///
	/// @param[in] requestedSerialNumber serial number of Lightpack to be search
	/// @return True on Lightpack found
	///
	bool searchDevice(libusb_device * device, const QString & requestedSerialNumber);

	int openDevice(libusb_device *device, libusb_device_handle ** deviceHandle);
	int closeDevice(libusb_device_handle * deviceHandle);

	QString getProperty(libusb_device * device, int stringDescriptorIndex);

	/// libusb context
	libusb_context * _libusbContext;

	/// libusb device
	libusb_device * _device;

	/// libusb device handle
	libusb_device_handle * _deviceHandle;

	/// hardware bus number
	int _busNumber;

	/// hardware address number
	int  _addressNumber;

	/// device serial number
	QString _serialNumber;

	/// command to write the LEDs
	uint8_t _writeLedsCommand;
};

#endif // LEDEVICEHYPERIONUSBASP_H
