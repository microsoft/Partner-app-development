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

namespace MediaFoundationProvider
{

class MediaDeviceManager
{
public:

    MediaDeviceManager();
    virtual ~MediaDeviceManager();

    WRL::ComPtr<IMFMediaType> GetSourceAttributes() { _threadLocker.Lock(); return _sourceAttributes; }
    LPWSTR GetUniqueSourceID() { _threadLocker.Lock(); return _uniqueSourceID; }
    LPWSTR GetFriendSourceName() { _threadLocker.Lock(); return _friendlySourceName; }
    bool IsInitialized() { _threadLocker.Lock(); return _sourceReader != nullptr; }
    bool IsMirrored() { _threadLocker.Lock(); return _cachedIsMirrored; }

    HRESULT Initialize(_In_ LPCWSTR targetDeviceId);
    HRESULT Shutdown();
    HRESULT ActivateStream(bool newActiveState);
    HRESULT ReadSample(WRL::ComPtr<IMFSample>& sampleData, _Out_ bool* readerStillValid);
    HRESULT RefreshStreamPropertyCache();

private:

    HRESULT InitializeSourceDevice(_In_ LPCWSTR targetDeviceId);
    HRESULT EnumMediaCaptureDevices(_In_ LPCWSTR targetDeviceId, _In_ const WRL::ComPtr<IMFAttributes>& enumAtributes, _Out_ WRL::ComPtr<IMFActivate>& mediaSourceActivate);
    HRESULT InitializeMediaSourceReader(_In_ const WRL::ComPtr<IMFActivate>& mediaSourceActivate, _Out_ WRL::ComPtr<IMFSourceReader>& sourceReader, _Out_ DWORD* streamIndex);
    HRESULT FindCompatibleMediaProfile(_In_ const WRL::ComPtr<IMFSourceReader>& sourceReader, _Outptr_ IMFMediaType** chosenType, _Out_ DWORD* chosesStreamIndex);
    HRESULT AcquireIdentificationStrings(_In_ const WRL::ComPtr<IMFActivate>& sourceActivation);
    HRESULT QueryIsMirroredState(_Out_ bool* isMirrored);
    bool IsMediaProfileValid(_In_ const WRL::ComPtr<IMFMediaType>& workingProfile);

    WRL::ComPtr<IMFSourceReader> _sourceReader;
    WRL::ComPtr<IMFMediaType> _sourceAttributes;

    WRLW::CriticalSection _threadLocker;
    LPWSTR _uniqueSourceID;
    LPWSTR _friendlySourceName;
    DWORD _streamIndex;
    bool _cachedIsMirrored;
};

} // end namespace