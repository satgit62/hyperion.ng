#pragma once

#include <mfapi.h>
#include <mftransform.h>
#include <dmo.h>
#include <wmcodecdsp.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <mferror.h>
#include <strmif.h>
#include <comdef.h>

#pragma comment (lib, "ole32.lib")
#pragma comment (lib, "mf.lib")
#pragma comment (lib, "mfplat.lib")
#pragma comment (lib, "mfuuid.lib")
#pragma comment (lib, "mfreadwrite.lib")
#pragma comment (lib, "strmiids.lib")
#pragma comment (lib, "wmcodecdspuuid.lib")

#include <grabber/MFGrabber.h>

#define SAFE_RELEASE(x) if(x) { x->Release(); x = nullptr; }

class SourceReaderCB : public IMFSourceReaderCallback
{
public:
	SourceReaderCB(MFGrabber* grabber)
		: _nRefCount(1)
		, _grabber(grabber)
		, _bEOS(FALSE)
		, _hrStatus(S_OK)
		, _isBusy(false)
	{
		// Initialize critical section.
		InitializeCriticalSection(&_critsec);
	}

	// IUnknown methods
	STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
	{
		static const QITAB qit[] =
		{
			QITABENT(SourceReaderCB, IMFSourceReaderCallback),
			{ 0 },
		};
		return QISearch(this, qit, iid, ppv);
	}

	STDMETHODIMP_(ULONG) AddRef()
	{
		return InterlockedIncrement(&_nRefCount);
	}

	STDMETHODIMP_(ULONG) Release()
	{
		ULONG uCount = InterlockedDecrement(&_nRefCount);
		if (uCount == 0)
		{
			delete this;
		}
		return uCount;
	}

	// IMFSourceReaderCallback methods
	STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD /*dwStreamIndex*/,
		DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample* pSample)
	{
		EnterCriticalSection(&_critsec);
		_isBusy = true;

		if (_grabber->_sourceReader == nullptr)
		{
			_isBusy = false;
			LeaveCriticalSection(&_critsec);
			return S_OK;
		}

		if (dwStreamFlags & MF_SOURCE_READERF_STREAMTICK)
		{
			Debug(_grabber->_log, "Skipping stream gap");
			LeaveCriticalSection(&_critsec);
			_grabber->_sourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr, nullptr, nullptr);
			return S_OK;
		}

		// Variables declaration
		IMFMediaBuffer* buffer = nullptr;
		BYTE* data = nullptr;
		DWORD maxLength = 0, currentLength = 0;

		if (FAILED(hrStatus))
		{
			_hrStatus = hrStatus;
			Error(_grabber->_log, "0x%08x: %s", _hrStatus, std::system_category().message(_hrStatus).c_str());
			goto done;
		}

		if (!pSample)
		{
			Error(_grabber->_log, "Media sample is empty");
			goto done;
		}

		_hrStatus = pSample->ConvertToContiguousBuffer(&buffer);
		if (FAILED(_hrStatus))
		{
			Error(_grabber->_log, "Buffer conversion failed => 0x%08x: %s", _hrStatus, std::system_category().message(_hrStatus).c_str());
			goto done;
		}

		_hrStatus = buffer->Lock(&data, &maxLength, &currentLength);
		if (FAILED(_hrStatus))
		{
			Error(_grabber->_log, "Access to the buffer memory failed => 0x%08x: %s", _hrStatus, std::system_category().message(_hrStatus).c_str());
			goto done;
		}

		_grabber->receive_image(data, currentLength);

		_hrStatus = buffer->Unlock();
		if (FAILED(_hrStatus))
		{
			Error(_grabber->_log, "Unlocking the buffer memory failed => 0x%08x: %s", _hrStatus, std::system_category().message(_hrStatus).c_str());
		}

	done:
		SAFE_RELEASE(buffer);

		if (MF_SOURCE_READERF_ENDOFSTREAM & dwStreamFlags)
		{
			_bEOS = TRUE; // Reached the end of the stream.
		}

		_isBusy = false;
		LeaveCriticalSection(&_critsec);
		return _hrStatus;
	}

	BOOL isBusy()
	{
		EnterCriticalSection(&_critsec);
		BOOL result = _isBusy;
		LeaveCriticalSection(&_critsec);

		return result;
	}

	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent*) { return S_OK; }
	STDMETHODIMP OnFlush(DWORD) { return S_OK; }

private:
	virtual ~SourceReaderCB()
	{
		// Delete critical section.
		DeleteCriticalSection(&_critsec);
	}

private:
	long				_nRefCount;
	CRITICAL_SECTION	_critsec;
	MFGrabber*			_grabber;
	BOOL				_bEOS;
	HRESULT				_hrStatus;
	std::atomic<bool>	_isBusy;
};
