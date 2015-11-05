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

#pragma once

namespace MediaFoundationProvider {

class MediaFoundationWrapper
{
public:

    MediaFoundationWrapper();
    virtual ~MediaFoundationWrapper();

    HRESULT Initialize(_In_ LPCWSTR targetDeviceId);
    HRESULT Shutdown();
    HRESULT Start();
    HRESULT Stop(bool waitForStop);

    HRESULT SubscribeReadFrame(const WRL::ComPtr<AWST::IWorkItemHandler>& readFrameCallback, EventRegistrationToken* pEventToken);
    HRESULT SubscribeAvailableChanged(const WRL::ComPtr<AWST::IWorkItemHandler>& availableChangedCallback, EventRegistrationToken* pEventToken);
    HRESULT UnsubscribeReadFrame(EventRegistrationToken eventToken);
    HRESULT UnsubscribeAvailableChanged(EventRegistrationToken eventToken);

    WRL::ComPtr<IMFMediaType> GetSourceAttributes() { return _deviceManager.GetSourceAttributes(); }
    WRL::ComPtr<IMFSample> GetCurrentFrame() { _readFrameLock.Lock(); { return _currentFrame; } }
    HRESULT GetCurrentFrameResult() { _readFrameLock.Lock(); { return _currentFrameResult; } }
    LPWSTR GetUniqueSourceID() { return _deviceManager.GetUniqueSourceID(); }
    LPWSTR GetFriendSourceName() { return _deviceManager.GetFriendSourceName(); }
    bool IsInitialized() { return _threadPool != nullptr; }
    bool IsRunning() { return _running; }
    bool IsAvailable() { return CheckIfAvailable(); }
    bool IsMirrored() { return _deviceManager.IsMirrored(); }

private:

    HRESULT CreateReadFrameAsyncTask();
    void ReadFrameProc();
    inline bool CheckIfKeepRunning();
    bool CheckIfAvailable();

    MediaDeviceManager _deviceManager;

    WRL::ComPtr<AWST::IThreadPoolStatics> _threadPool;
    WRL::ComPtr<IMFSample> _currentFrame;

    WRL::EventSource<AWST::IWorkItemHandler> _readFrameEvents;
    WRL::EventSource<AWST::IWorkItemHandler> _availableChangedEvents;

    HANDLE _readFrameFinished;
    HANDLE _stopReadingFrames;
    WRLW::CriticalSection _readFrameLock;

    HRESULT _currentFrameResult;
    bool _running;
};

} // end namespace