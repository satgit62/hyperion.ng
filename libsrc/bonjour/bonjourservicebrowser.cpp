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

#include "bonjour/bonjourservicebrowser.h"
#include <utils/Logger.h>

#include <QtCore/QSocketNotifier>
#include <QDebug>

BonjourServiceBrowser::BonjourServiceBrowser(QObject *parent)
	: QObject(parent)
	, dnssref(nullptr)
	, bonjourSocket(nullptr)
{
}

BonjourServiceBrowser::~BonjourServiceBrowser()
{
	if (dnssref != nullptr)
	{
		DNSServiceRefDeallocate(dnssref);
		dnssref = nullptr;
	}
}

void BonjourServiceBrowser::browseForServiceType(const QString &serviceType)
{
	DNSServiceErrorType err = DNSServiceBrowse(&dnssref, 0, 0, serviceType.toUtf8().constData(), nullptr, bonjourBrowseReply, this);

	Debug(Logger::getInstance("BonJour"), "browseForServiceType - (%d), service type [%s]", err,QSTRING_CSTR(serviceType));

	if (err != kDNSServiceErr_NoError)
	{
		Debug(Logger::getInstance("BonJour"), "browseForServiceType error [%d]", err);
		emit error(err);
	}
	else
	{
		int sockfd = DNSServiceRefSockFD(dnssref);
		if (sockfd == -1)
		{
			Debug(Logger::getInstance("BonJour"), "browseForServiceType sockfd error [%d]", sockfd);
			emit error(kDNSServiceErr_Invalid);
		}
		else
		{
			bonjourSocket = new QSocketNotifier(sockfd, QSocketNotifier::Read, this);
			connect(bonjourSocket, &QSocketNotifier::activated, this, &BonjourServiceBrowser::bonjourSocketReadyRead);
		}
	}
}

void BonjourServiceBrowser::bonjourSocketReadyRead()
{
	DNSServiceErrorType err = DNSServiceProcessResult(dnssref);
	if (err != kDNSServiceErr_NoError)
	{
		Debug(Logger::getInstance("BonJour"), "bonjourSocketReadyRead error [%d]", err);
		emit error(err);
	}
}

void BonjourServiceBrowser::bonjourBrowseReply(DNSServiceRef /*unused*/, DNSServiceFlags flags,
											   quint32 interfaceIndex, DNSServiceErrorType errorCode,
											   const char *serviceName, const char *regType,
											   const char *replyDomain, void *context)
{
	BonjourServiceBrowser *serviceBrowser = static_cast<BonjourServiceBrowser *>(context);
	if (errorCode != kDNSServiceErr_NoError)
	{
		Debug(Logger::getInstance("BonJour"), "bonjourBrowseReply error [%d]", errorCode);
		emit serviceBrowser->error(errorCode);
	}
	else
	{
		BonjourRecord bonjourRecord(serviceName, regType, replyDomain, interfaceIndex);

		if ( bonjourRecord.registeredType.endsWith('.'))
		{
			bonjourRecord.registeredType.chop(1);
		}

		if ((flags & kDNSServiceFlagsAdd) != 0)
		{
			if (!serviceBrowser->bonjourRecords.contains(bonjourRecord))
			{
				Debug(Logger::getInstance("BonJour"), "bonjourBrowseReply contains [%s:%s]", regType, serviceName);
				serviceBrowser->bonjourRecords.append(bonjourRecord);
				//qDebug() << "bonjourBrowseReply: type: " << serviceBrowser->browsingType << ", bonjourRecords" << serviceBrowser->bonjourRecords;
			}
		}
		else
		{
			Debug(Logger::getInstance("BonJour"), "bonjourBrowseReply removeAll [%s:%s]", regType, serviceName);
			serviceBrowser->bonjourRecords.removeAll(bonjourRecord);
		}
		if (!(flags & kDNSServiceFlagsMoreComing))
		{
			emit serviceBrowser->currentBonjourRecordsChanged(serviceBrowser->bonjourRecords);
		}
	}
}
