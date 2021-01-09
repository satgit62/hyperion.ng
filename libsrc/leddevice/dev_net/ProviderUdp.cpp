// Local Hyperion includes
#include "ProviderUdp.h"

// mDNS/bonjour wrapper
#ifndef __APPLE__
#include <mdns/mdnsEngineWrapper.h>
#endif

#include <QStringList>
#include <QUdpSocket>
#include <QHostInfo>
#include <QThread>

#include <chrono>

// Constants
namespace {

	const ushort MAX_PORT = 65535;

	// mDNS Hostname resolution
#ifndef __APPLE__
	const int DEFAULT_HOSTNAME_RESOLUTION_RETRIES = 6;
	constexpr std::chrono::milliseconds DEFAULT_HOSTNAME_RESOLUTION_WAIT_TIME{ 500 };
#endif

} //End of constants

ProviderUdp::ProviderUdp(const QJsonObject& deviceConfig)
	: LedDevice(deviceConfig)
	  , _udpSocket(nullptr)
	  , _port(1)
	  , _defaultHost("127.0.0.1")
#ifndef __APPLE__
	, _mdnsEngine(MdnsEngineWrapper::getInstance())
#endif
{
	_latchTime_ms = 0;
}

ProviderUdp::~ProviderUdp()
{
	delete _udpSocket;
}

bool ProviderUdp::init(const QJsonObject& deviceConfig)
{
	bool isInitOK = false;

	// Initialise sub-class
	if (LedDevice::init(deviceConfig))
	{
		QString _hostName = deviceConfig["host"].toString(_defaultHost);

#ifndef __APPLE__
		if (_hostName.endsWith(".local."))
		{
			qDebug() << "ProviderUdp::init" << QThread::currentThread();

			QHostAddress hostAddress = _mdnsEngine->getHostAddress(_hostName);

			int retries = DEFAULT_HOSTNAME_RESOLUTION_RETRIES;
			while (hostAddress.isNull() && retries > 0)
			{
				--retries;
				Debug(_log, "retries left: [%d], hostAddress: [%s]", retries, QSTRING_CSTR(hostAddress.toString()));
				QThread::msleep(DEFAULT_HOSTNAME_RESOLUTION_WAIT_TIME.count());
				hostAddress = _mdnsEngine->getHostAddress(_hostName);
			}
			Debug(_log, "getHostAddress finished - retries left: [%d], IP-address [%s]", retries, QSTRING_CSTR(hostAddress.toString()));

			if (retries == 0)
			{
				Error(_log, "Resolving IP-address for hostName [%s] failed.", QSTRING_CSTR(_hostName));
			}

			_hostName = hostAddress.toString();
		}
		else
#endif
		{
			_address.setAddress(_hostName);
		}

		if (!_address.isNull())
		{
			Debug(_log, "Successfully parsed %s as an IP-address.", QSTRING_CSTR(_address.toString()));
		}
		else
		{
			QHostInfo hostInfo = QHostInfo::fromName(_hostName);
			if (hostInfo.error() == QHostInfo::NoError)
			{
				_address = hostInfo.addresses().first();
				Debug(_log, "Successfully resolved IP-address (%s) for hostname (%s).", QSTRING_CSTR(_address.toString()), QSTRING_CSTR(_hostName));
			}
			else
			{
				QString errortext = QString("Failed resolving IP-address for [%1], (%2) %3").arg(_hostName).arg(hostInfo.error()).arg(hostInfo.errorString());
				this->setInError(errortext);
				isInitOK = false;
			}
		}

		if (!_isDeviceInError)
		{
			int config_port = deviceConfig["port"].toInt(_port);
			if (config_port <= 0 || config_port > MAX_PORT)
			{
				QString errortext = QString("Invalid target port [%1]!").arg(config_port);
				this->setInError(errortext);
				isInitOK = false;
			}
			else
			{
				_port = static_cast<quint16>(config_port);
				Debug(_log, "UDP socket will write to %s:%u", QSTRING_CSTR(_address.toString()), _port);

				_udpSocket = new QUdpSocket(this);

				isInitOK = true;
			}
		}
	}
	return isInitOK;
}

int ProviderUdp::open()
{
	int retval = -1;
	_isDeviceReady = false;

	// Try to bind the UDP-Socket
	if (_udpSocket != nullptr)
	{
		if (_udpSocket->state() != QAbstractSocket::BoundState)
		{
			QHostAddress localAddress = QHostAddress::Any;
			quint16      localPort = 0;
			if (!_udpSocket->bind(localAddress, localPort))
			{
				QString warntext = QString("Could not bind local address: %1, (%2) %3").arg(localAddress.toString()).arg(_udpSocket->error()).arg(_udpSocket->errorString());
				Warning(_log, "%s", QSTRING_CSTR(warntext));
			}
		}

		// Everything is OK, device is ready
		_isDeviceReady = true;
		retval = 0;
	}
	else
	{
		this->setInError(" Open error. UDP Socket not initialised!");
	}
	return retval;
}

int ProviderUdp::close()
{
	int retval = 0;
	_isDeviceReady = false;

	if (_udpSocket != nullptr)
	{
		// Test, if device requires closing
		if (_udpSocket->isOpen())
		{
			Debug(_log, "Close UDP-device: %s", QSTRING_CSTR(this->getActiveDeviceType()));
			_udpSocket->close();
			// Everything is OK -> device is closed
		}
	}
	return retval;
}

int ProviderUdp::writeBytes(const unsigned size, const uint8_t* data)
{
	int rc = 0;
	qint64 bytesWritten = _udpSocket->writeDatagram(reinterpret_cast<const char*>(data), size, _address, _port);

	if (bytesWritten == -1 || bytesWritten != size)
	{
		Warning(_log, "%s", QSTRING_CSTR(QString("(%1:%2) Write Error: (%3) %4").arg(_address.toString()).arg(_port).arg(_udpSocket->error()).arg(_udpSocket->errorString())));
		rc = -1;
	}
	return  rc;
}

int ProviderUdp::writeBytes(const QByteArray& bytes)
{
	int rc = 0;
	qint64 bytesWritten = _udpSocket->writeDatagram(bytes, _address, _port);

	if (bytesWritten == -1 || bytesWritten != bytes.size())
	{
		Warning(_log, "%s", QSTRING_CSTR(QString("(%1:%2) Write Error: (%3) %4").arg(_address.toString()).arg(_port).arg(_udpSocket->error()).arg(_udpSocket->errorString())));
		rc = -1;
	}
	return  rc;
}
