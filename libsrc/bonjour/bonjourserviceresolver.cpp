/*
Copyright (c) 2007, Trenton Schulz,
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

#include <QtCore/QSocketNotifier>
#include <QtNetwork/QHostInfo>
#include <QtEndian>

#include "bonjour/bonjourrecord.h"
#include "bonjour/bonjourserviceresolver.h"
#include <utils/Logger.h>
#include <dns_sd.h>

BonjourServiceResolver::BonjourServiceResolver(QObject *parent)
	: QObject(parent)
	, _dnssref(nullptr)
	, _bonjourSocket(nullptr)
	, _bonjourPort(-1)
{
}

BonjourServiceResolver::~BonjourServiceResolver()
{
	cleanupResolve();
}

void BonjourServiceResolver::cleanupResolve()
{
	Debug(Logger::getInstance("BonJour"), "cleanupResolve - [%d],",_dnssref);
	if (_dnssref != nullptr)
	{
		DNSServiceRefDeallocate(_dnssref);
		_dnssref = nullptr;
		delete _bonjourSocket;
		_bonjourPort = -1;
	}
}

bool BonjourServiceResolver::resolveBonjourRecord(const BonjourRecord &record)
{
	Debug(Logger::getInstance("BonJour"), "resolveBonjourRecord - service [%s %s %s %d]",
		   QSTRING_CSTR(record.serviceName), QSTRING_CSTR(record.registeredType),  QSTRING_CSTR(record.replyDomain), record.interfaceIndex);
	if (_dnssref != nullptr)
	{
		qWarning("resolve in process, aborting");
		Debug(Logger::getInstance("BonJour"), "resolve in process, aborting, {[%d]",_dnssref);

		return false;
	}
	DNSServiceErrorType err = DNSServiceResolve(&_dnssref, 0, 0,
												record.serviceName.toUtf8().constData(),
												record.registeredType.toUtf8().constData(),
												record.replyDomain.toUtf8().constData(),
												 reinterpret_cast<DNSServiceResolveReply>(bonjourResolveReply), this);

	Debug(Logger::getInstance("BonJour"), "resolveBonjourRecord - (%d) [%d], , service [%s %s %s %d]", err, _dnssref,
		   QSTRING_CSTR(record.serviceName), QSTRING_CSTR(record.registeredType),  QSTRING_CSTR(record.replyDomain), record.interfaceIndex);

	if (err != kDNSServiceErr_NoError)
	{
		Debug(Logger::getInstance("BonJour"), "resolveBonjourRecord error [%d]", err);
		emit error(err);
	}
	else
	{
		char fullName [kDNSServiceMaxDomainName];
		DNSServiceConstructFullName	(fullName,record.serviceName.toLocal8Bit().data(),record.registeredType.toLocal8Bit().data(), record.replyDomain.toLocal8Bit().data());

		_bonjourFullname = fullName;
		Debug(Logger::getInstance("BonJour"), "resolveBonjourRecord - %s:%d",QSTRING_CSTR(_bonjourFullname),_bonjourPort);

		int sockfd = DNSServiceRefSockFD(_dnssref);
		if (sockfd == -1)
		{
			Debug(Logger::getInstance("BonJour"), "resolveBonjourRecord sockfd error [%d]", sockfd);
			emit error(kDNSServiceErr_Invalid);
		}
		else
		{
			_bonjourSocket = new QSocketNotifier(sockfd, QSocketNotifier::Read, this);
			connect(_bonjourSocket, &QSocketNotifier::activated, this, &BonjourServiceResolver::bonjourSocketReadyRead);
		}
	}
	return true;
}

void BonjourServiceResolver::bonjourSocketReadyRead()
{
	DNSServiceErrorType err = DNSServiceProcessResult(_dnssref);

	Debug(Logger::getInstance("BonJour"), "bonjourSocketReadyRead - (%d) [%d],", err,_dnssref);

	if (err != kDNSServiceErr_NoError)
	{
		Debug(Logger::getInstance("BonJour"), "bonjourSocketReadyRead error [%d]", err);
		emit error(err);
	}
}

void BonjourServiceResolver::bonjourResolveReply(DNSServiceRef /*sdRef*/, DNSServiceFlags flags,
									quint32 /*unused*/, DNSServiceErrorType errorCode,
									const char * fullname, const char *hosttarget, quint16 port,
									quint16 txtLen, const char *txtRecord, void *context)
{
	Debug(Logger::getInstance("BonJour"), "bonjourResolveReply - flags [%d] (%d), service [%s] %s:%d", flags, errorCode,
		   fullname,
		   hosttarget, port );

	BonjourServiceResolver *serviceResolver = static_cast<BonjourServiceResolver *>(context);

	if (errorCode != kDNSServiceErr_NoError) {
		Debug(Logger::getInstance("BonJour"), "bonjourResolveReply error [%d]", errorCode);
		emit serviceResolver->error(errorCode);


		QHostInfo hostInfo;
		hostInfo.setError(QHostInfo::HostNotFound);

		emit serviceResolver->finishConnect(hostInfo);

		//QMetaObject::invokeMethod(serviceResolver, "cleanupResolve", Qt::QueuedConnection);
		return;
	}

	serviceResolver->_bonjourFullname = fullname;
	if ( serviceResolver->_bonjourFullname.endsWith('.'))
	{
		serviceResolver->_bonjourFullname.chop(1);
	}

	serviceResolver->_bonjourPort = static_cast<int>(qFromBigEndian<quint16>(port));

	Debug(Logger::getInstance("BonJour"), "bonjourResolveReply - (%d), service [%s] %s:%d", errorCode,
		   QSTRING_CSTR(serviceResolver->_bonjourFullname),
		   hosttarget, serviceResolver->_bonjourPort );



	const uint16_t keyBufLen = 256;
	char key[keyBufLen];
	uint16_t index=0;
	unsigned char valueLen;
	const void *voidValue = nullptr;

	QMap<QString,QByteArray> map;
	while (TXTRecordGetItemAtIndex(txtLen,txtRecord,index,keyBufLen,key,&valueLen,
									 &voidValue) == kDNSServiceErr_NoError)
	{
		if (voidValue != nullptr)
		{
			map[QString::fromUtf8(key)]=QByteArray(static_cast<const char *>(voidValue),valueLen);
		}
		else
		{
			map[QString::fromUtf8(key)].clear();
		}
		++index;
	}

	serviceResolver->_bonjourTxtData = map;

	//QMetaObject::invokeMethod(serviceResolver, "cleanupResolve", Qt::QueuedConnection);

	QHostInfo::lookupHost(QString::fromUtf8(hosttarget), serviceResolver, SLOT(finishConnect(const QHostInfo &)));
}

void BonjourServiceResolver::finishConnect(const QHostInfo &hostInfo)
{
	emit bonjourRecordResolved(_bonjourFullname, hostInfo, _bonjourPort, _bonjourTxtData);
	QMetaObject::invokeMethod(this, "cleanupResolve", Qt::QueuedConnection);
}
