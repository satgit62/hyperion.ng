/*
Copyright (c) 2007, Trenton Schulz
2020, Updates Lord-Grey

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 3. The name of the author may not be used to endorse or promote products
    derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <bonjour/bonjourserviceregister.h>
#include <stdlib.h>

#include <QtCore/QSocketNotifier>
#include <QHostInfo>
#include <QtEndian>

#include <utils/Logger.h>
#include <HyperionConfig.h>
#include <hyperion/AuthManager.h>

BonjourServiceRegister::BonjourServiceRegister(QObject *parent)
	: QObject(parent), dnssref(nullptr), bonjourSocket(nullptr)
{
	setenv("AVAHI_COMPAT_NOWARN", "1", 1);
}

BonjourServiceRegister::~BonjourServiceRegister()
{
	if (dnssref != nullptr)
	{
		DNSServiceRefDeallocate(dnssref);
		dnssref = nullptr;
	}
}

void BonjourServiceRegister::registerService(const QString& service, int port)
{
	_port = static_cast<quint16>(port);
	// zeroconf $configname@$hostname:port
	// TODO add name of the main instance
	registerService(
		BonjourRecord(QHostInfo::localHostName()+ ":" + QString::number(port),
			service,
			QString(),
			0
		),
		static_cast<quint16>(port)
	);
	emit error(4711);
}

void BonjourServiceRegister::registerService(const BonjourRecord &record, quint16 servicePort, const std::vector<std::pair<std::string, std::string>>& txt)
{
	if (dnssref != nullptr)
	{
		Warning(Logger::getInstance("BonJour"), "Already registered a service for this object, aborting new register");
		return;
	}

	quint16 bigEndianPort = qToBigEndian<quint16>(servicePort);

	// base txtRec
	std::vector<std::pair<std::string, std::string> > txtBase = {{"id",AuthManager::getInstance()->getID().toStdString()},{"version",HYPERION_VERSION}};
    // create txt record
    TXTRecordRef txtRec;
	TXTRecordCreate(&txtRec,0,nullptr);

	if(!txt.empty())
    {
		txtBase.insert(txtBase.end(), txt.begin(), txt.end());
	}
    // add txt records
    for(std::vector<std::pair<std::string, std::string> >::const_iterator it = txtBase.begin(); it != txtBase.end(); ++it)
    {
		//Debug(Logger::getInstance("BonJour"), "TXTRecord: key:%s, value:%s",it->first.c_str(),it->second.c_str());
		uint8_t txtLen = static_cast<uint8_t>(strlen(it->second.c_str()));
        TXTRecordSetValue(&txtRec, it->first.c_str(), txtLen, it->second.c_str());
    }


	DNSServiceErrorType err = DNSServiceRegister(&dnssref, 0, record.interfaceIndex, record.serviceName.toUtf8().constData(),
	                                            record.registeredType.toUtf8().constData(),
												(record.replyDomain.isEmpty() ? nullptr : record.replyDomain.toUtf8().constData()),
												nullptr, bigEndianPort, TXTRecordGetLength(&txtRec), TXTRecordGetBytesPtr(&txtRec), bonjourRegisterService, this);
	if (err != kDNSServiceErr_NoError)
	{
		Debug(Logger::getInstance("BonJour"), "registerService error [%d]", err);
		emit error(err);
	}
	else
	{
		int sockfd = DNSServiceRefSockFD(dnssref);
		if (sockfd == -1)
		{
			Debug(Logger::getInstance("BonJour"), "registerService sockfd error [%d]", sockfd);
			emit error(kDNSServiceErr_Invalid);
		}
		else
		{
			bonjourSocket = new QSocketNotifier(sockfd, QSocketNotifier::Read, this);
			connect(bonjourSocket, &QSocketNotifier::activated, this, &BonjourServiceRegister::bonjourSocketReadyRead);
		}
	}
}

void BonjourServiceRegister::bonjourSocketReadyRead()
{
	DNSServiceErrorType err = DNSServiceProcessResult(dnssref);
	if (err != kDNSServiceErr_NoError)
	{
		Debug(Logger::getInstance("BonJour"), "bonjourSocketReadyRead error [%d]", err);
		emit error(err);
	}
}

void BonjourServiceRegister::bonjourRegisterService(DNSServiceRef /*unused*/, DNSServiceFlags /*unused*/,
                                                   DNSServiceErrorType errorCode, const char *name,
                                                   const char *regtype, const char *domain,
                                                   void *data)
{
	BonjourServiceRegister *serviceRegister = static_cast<BonjourServiceRegister *>(data);
	if (errorCode != kDNSServiceErr_NoError)
	{

		emit serviceRegister->error(errorCode);
	}
	else
	{
		serviceRegister->finalRecord = BonjourRecord(QString::fromUtf8(name),
		                                        QString::fromUtf8(regtype),
												QString::fromUtf8(domain),
												0
										);
		emit serviceRegister->serviceRegistered(serviceRegister->finalRecord);
	}
}
