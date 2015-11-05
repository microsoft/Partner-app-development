//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"
#include "MediaFoundationWrapper.h"

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::System::Threading;
using namespace ABI::Windows::System::Threading::Core;

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

namespace MediaFoundationProvider {

MediaFoundationWrapper::MediaFoundationWrapper() :
    _readFrameFinished(NULL),
    _stopReadingFrames(NULL),
    _currentFrameResult(0),
    _running(false)
{
    RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);

    _stopReadingFrames = CreateEvent(NULL, TRUE, FALSE, L"MediaWrapper_StopReadingFrames");
    _readFrameFinished = CreateEvent(NULL, FALSE, FALSE, L"MediaWrapper_ReadFrameFinished");
}

MediaFoundationWrapper::~MediaFoundationWrapper()
{
    Shutdown();

    CloseHandle(_readFrameFinished);
    CloseHandle(_stopReadingFrames);

    MFShutdown();
    RoUninitialize();
}

HRESULT MediaFoundationWrapper::Initialize(_In_ LPCWSTR targetDeviceId)
{
    HRESULT hr;

    if (IsInitialized()) return E_ABORT;

    // Attempt to connect to our source device and initialize the frame reader.
    hr = _deviceManager.Initialize(targetDeviceId);
    if (SUCCEEDED(hr))
    {
        hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_System_Threading_ThreadPool).Get(), &_threadPool);
    }

    return hr;
}

HRESULT MediaFoundationWrapper::Shutdown()
{
    // We must wait for ReadFrameProc to complete before shutting down MediaFoundation, etc. or bad stuff may happen
    // ReadFrameProc should exit within 1 frame (typically 33 ms) or less
    Stop(true);

    _deviceManager.Shutdown();
    _threadPool.Reset();
    return S_OK;
}

HRESULT MediaFoundationWrapper::Start()
{
    HRESULT hr;

    if (!IsInitialized()) return E_ABORT;
    if (_running) return E_ACCESSDENIED;

    // Check if we're still in the "stopping" state: _stopReadingFrames is signaled
    // Must wait for prior session to exit before starting a new one
    if (WaitForSingleObject(_stopReadingFrames, 0) == WAIT_OBJECT_0)
    {
        return E_ACCESSDENIED;
    }

    hr = CreateReadFrameAsyncTask();
    if (SUCCEEDED(hr))
    {
        hr = _deviceManager.ActivateStream(true);
        if (SUCCEEDED(hr))
        {
            _running = true;
        }

        // Update cached values of device properties
        if (SUCCEEDED(hr))
        {
            _deviceManager.RefreshStreamPropertyCache();
        }
    }
    return hr;
}

HRESULT MediaFoundationWrapper::Stop(bool waitForStop)
{
    if (!IsInitialized()) return E_ABORT;
    if (!_running) return E_ACCESSDENIED;

    // Signal ReadFrameProc to stop running, i.e. exit immediately and don't spawn a new task
    if (!SetEvent(_stopReadingFrames))
    {
        return E_UNEXPECTED;
    }

    // Deactivating the stream in MediaFoundation is what actually stops the session
    HRESULT hr = _deviceManager.ActivateStream(false);
    if (FAILED(hr))
    {
        return hr;
    }

    _running = false;

    // Block according to waitForStop parameter to wait a short time for current task to exit
    // If we don't wait, ReadFrameProc may continue running for a short time after Stop returns
    // NOTE: Start will FAIL if called before current task completes
    WaitForSingleObject(_readFrameFinished, waitForStop ? 100 : 0);

    // Reset internal Running state and clear out current Frame Data
    {
        auto lock = _readFrameLock.Lock();

        _currentFrameResult = 0;
        _currentFrame.Reset();
    }

    return S_OK;
}

HRESULT MediaFoundationWrapper::SubscribeReadFrame(const ComPtr<IWorkItemHandler>& readFrameCallback, EventRegistrationToken* pEventToken)
{
    if (!IsInitialized()) return E_ABORT;
    if (_running) return E_ACCESSDENIED;

    if (readFrameCallback == nullptr) return E_INVALIDARG;
    if (pEventToken == nullptr) return E_POINTER;

    return _readFrameEvents.Add(readFrameCallback.Get(), pEventToken);
}

HRESULT MediaFoundationWrapper::SubscribeAvailableChanged(const ComPtr<IWorkItemHandler>& availableChangedCallback, EventRegistrationToken* pEventToken)
{
    if (!IsInitialized()) return E_ABORT;
    if (_running) return E_ACCESSDENIED;

    if (availableChangedCallback == nullptr) return E_INVALIDARG;
    if (pEventToken == nullptr) return E_POINTER;

    return _availableChangedEvents.Add(availableChangedCallback.Get(), pEventToken);
}

HRESULT MediaFoundationWrapper::UnsubscribeReadFrame(EventRegistrationToken eventToken)
{
    if (!IsInitialized()) return E_ABORT;
    if (_running) return E_ACCESSDENIED;

    return _readFrameEvents.Remove(eventToken);
}

HRESULT MediaFoundationWrapper::UnsubscribeAvailableChanged(EventRegistrationToken eventToken)
{
    if (!IsInitialized()) return E_ABORT;
    if (_running) return E_ACCESSDENIED;

    return _availableChangedEvents.Remove(eventToken);
}

HRESULT MediaFoundationWrapper::CreateReadFrameAsyncTask()
{
    HRESULT hr;

    // Since we're about to start a new ReadFrame task, ensure _readFrameFinished event is not signaled
    // NOTE: We don't keep the reference to IAsyncAction since only one task runs at a time and running
    // status and errors are communicated through member variables.
    ResetEvent(_readFrameFinished);

    ComPtr<IAsyncAction> asyncAction;
    hr = _threadPool->RunAsync(Callback<IWorkItemHandler>([this](IAsyncAction* asyncAction) -> HRESULT
    {
        UNREFERENCED_PARAMETER(asyncAction);

        this->ReadFrameProc();
        return S_OK;
    }).Get(), &asyncAction);

    return hr;
}

void MediaFoundationWrapper::ReadFrameProc()
{
    bool availablityChanged = false;
    HRESULT hr = S_OK;

    // At each stage of the task, check that _stopReadingFrames is not signalled
    // Otherwise, exit the proc as quickly as possible
    if (CheckIfKeepRunning())
    {
        auto lock = _readFrameLock.Lock();

        // If the device is connected, attempt to read the next frame from the device
        // If this fails, we'll assume device is no longer available and signal this to ISourceProvider.
        ComPtr<IMFSample> sampleData;
        bool readerStillValid;

        if (_deviceManager.IsInitialized())
        {
            hr = _deviceManager.ReadSample(sampleData, &readerStillValid);
            if (!readerStillValid)
            {
                _deviceManager.Shutdown(); // Device objects are no longer valid so get rid of them
                availablityChanged = true;
            }
        }
        else hr = E_NOT_SET;

        // Don't update current results if we've been signaled to exit; Stop() will reset the values
        if (CheckIfKeepRunning())
        {
            // If ReadFrame succeeded and produce a valid sample save it to current results
            // Else if we failed because availabity changed clear frame data (it's invalid) but save HR
            // Otherwise do nothing
            if (SUCCEEDED(hr))
            {
                _currentFrame = sampleData;
                _currentFrameResult = hr;
            }
            else if (availablityChanged)
            {
                _currentFrame.Reset();
                _currentFrameResult = hr;
            }
        }
    } // Leave CriticalSection

    // If availablity of the sensor has changed, call event handlers
    if (CheckIfKeepRunning())
    {
        if (availablityChanged)
        {
            if (FAILED(_availableChangedEvents.InvokeAll(nullptr)))
            {
                // This doesn't impact ReadFrame functionality so we can ignore failures
            }
        }
    }

    // If succesfully acquired new frame data, call event handlers
    if (CheckIfKeepRunning())
    {
        if (SUCCEEDED(hr))
        {
            if (FAILED(_readFrameEvents.InvokeAll(nullptr)))
            {
                // This doesn't impact ReadFrame functionality so we can ignore failures
            }
        }
    }

    // If we're still running, queue up another ReadFrameProc task
    // NOTE: Once we've committed to spawning a new task don't check the event again
    bool commitedToKeepRunning = CheckIfKeepRunning();
    if (commitedToKeepRunning)
    {
        CreateReadFrameAsyncTask();
    }

    // If we've been signaled to Stop, set _stopReadingFrames indicating we're exiting the
    // ReadFrame proc and Manually reset _stopReadingFrames
    //
    // NOTE: Reset _stopReadingFrames event BEFORE signaling _readFrameFinished, this should
    // prevent a possible race condition where immediately calling Start() after Stop() fails
    // because the _stopReadingFrames was still signaled
    if (!commitedToKeepRunning)
    {
        ResetEvent(_stopReadingFrames);
        SetEvent(_readFrameFinished);
    }
}

inline bool MediaFoundationWrapper::CheckIfKeepRunning()
{
    return WaitForSingleObject(_stopReadingFrames, 0) != WAIT_OBJECT_0;
}

bool MediaFoundationWrapper::CheckIfAvailable()
{
    bool currentlyAvailable = _deviceManager.IsInitialized();

    // We must poll IMFSourceReader to see if the source device is still connected,
    // basically if ReadSample succeeds we're good but otherwise the device isn't available.
    // If we're "Running" (reading frames) we simply return the connection status of the device,
    // otherwise we have to try and read a frame to make sure the device is still available.
    if (!_running)
    {
        if (currentlyAvailable)
        {
            ComPtr<IMFSample> dummyData;
            bool availablityChanged = false;

            _deviceManager.ReadSample(dummyData, &currentlyAvailable);
            if (!currentlyAvailable)
            {
                _deviceManager.Shutdown(); // Device objects are no longer valid so get rid of them
                availablityChanged = true;
            }

            if (availablityChanged)
            {
                if (FAILED(_availableChangedEvents.InvokeAll(nullptr)))
                {
                    // This doesn't impact function so we can ignore failures
                }
            }
        }
    }

    return currentlyAvailable;
}

} // end namespace