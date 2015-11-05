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

using namespace ABI::Windows::System::Threading;
using namespace ABI::Windows::System::Threading::Core;

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

namespace MediaFoundationProvider
{

MediaDeviceManager::MediaDeviceManager() :
    _uniqueSourceID(nullptr),
    _friendlySourceName(nullptr),
    _streamIndex(static_cast<DWORD>(-1)),
    _cachedIsMirrored(false)
{
}

MediaDeviceManager::~MediaDeviceManager()
{
    Shutdown();
}

HRESULT MediaDeviceManager::Initialize(_In_ LPCWSTR targetDeviceId)
{
    auto lock = _threadLocker.Lock();

    HRESULT hr = S_OK;
    if (!IsInitialized())
    {
        hr = InitializeSourceDevice(targetDeviceId);
    }

    return hr;
}

HRESULT MediaDeviceManager::Shutdown()
{
    auto lock = _threadLocker.Lock();

    _sourceAttributes.Reset();
    _sourceReader.Reset();

    // Safe to pass in NULL pointers
    CoTaskMemFree(_uniqueSourceID);
    CoTaskMemFree(_friendlySourceName);

    _uniqueSourceID = nullptr;
    _friendlySourceName = nullptr;
    _streamIndex = static_cast<DWORD>(-1);

    return S_OK;
}

HRESULT MediaDeviceManager::ActivateStream(bool newActiveState)
{
    auto lock = _threadLocker.Lock();

    // Don't return an error if we're attempting to deactivate the stream after reader has been shutdown
    if (_sourceReader == nullptr && !newActiveState)
    {
        return S_OK;
    }
    else if (_sourceReader == nullptr)
    {
        return E_NOT_VALID_STATE;
    }

    return _sourceReader->SetStreamSelection(_streamIndex, newActiveState);
}

HRESULT MediaDeviceManager::ReadSample(ComPtr<IMFSample>& sampleData, _Out_ bool* readerStillValid)
{
    auto lock = _threadLocker.Lock();

    sampleData.Reset();

    bool readerValidLocal = false;
    HRESULT hr = E_NOT_VALID_STATE;

    if (_sourceReader != nullptr)
    {
        BOOL currActiveState;

        // Check if the stream is currently selected, if not don't attempt to read a frame (it'll fail)
        hr = _sourceReader->GetStreamSelection(_streamIndex, &currActiveState);
        if (SUCCEEDED(hr))
        {
            if (currActiveState)
            {
                DWORD dummy;
                DWORD flags;
                LONGLONG timeStamp;

                // Call ReadSample to try and acquire the next video frame from the capture device
                // A failed HR usually means the capture device is no longer available, i.e. camera was unplugged,
                // however the call may still "succeed" but not return a valid sample, need to check both cases.
                hr = _sourceReader->ReadSample(_streamIndex, 0, &dummy, &flags, &timeStamp, sampleData.GetAddressOf());
                if (FAILED(hr) || ((flags & MF_SOURCE_READERF_ERROR) != 0))
                {
                    readerValidLocal = false;    // Reader is no longer valid; need to call ReleaseConnection
                }
                else if (sampleData == nullptr)
                {
                    readerValidLocal = true;     // Didn't get a sample (return failed) but reader itself is still valid
                    hr = E_NOT_SET;
                }
                else
                {
                    readerValidLocal = true;     // Received a valid sample; everything is good
                }
            }
            else
            {
                // The stream is deselected but reader object is still valid
                readerValidLocal = true;
                hr = E_NOT_VALID_STATE;
            }
        }
    }

    if (readerStillValid != nullptr)
    {
        *readerStillValid = readerValidLocal;
    }
    return hr;
}

HRESULT MediaDeviceManager::RefreshStreamPropertyCache()
{
    auto lock = _threadLocker.Lock();

    if (_sourceReader == nullptr) return E_NOT_VALID_STATE;

    // Update the "mirrored" state of the video stream
    bool isMirrored = false;
    HRESULT hr = QueryIsMirroredState(&isMirrored);

    _cachedIsMirrored = SUCCEEDED(hr) ? isMirrored : false;

    return S_OK;
}

HRESULT MediaDeviceManager::InitializeSourceDevice(_In_ LPCWSTR targetDeviceId)
{
    ComPtr<IMFAttributes> enumAttributes;
    ComPtr<IMFActivate> sourceActivation;
    ComPtr<IMFSourceReader> sourceReader;
    DWORD streamIndex = static_cast<DWORD>(-1);

    // Create an attribute store to specify the enumeration parameters
    // Specify "VIDCAP" devices as the major type to enumerate
    HRESULT hr = MFCreateAttributes(enumAttributes.GetAddressOf(), 1);
    if (SUCCEEDED(hr))
    {
        hr = enumAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

        //
        // TODO: If appropriate, restrict enumeration to KSCATEGORY_SENSOR_CAMERA devices (set SampleFrameProvider::_requiredKsSensorDevice in FrameProvider.h)
        // This will eliminate any device that does not have the attribute set from our enumeration
        //
        if (SUCCEEDED(hr) && SampleFrameProvider::_requiredKsSensorDevice)
        {
            hr = enumAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY, KSCATEGORY_SENSOR_CAMERA);
        }
    }

    // Use the enumAttributes to find a compatible device on the system; receive a MediaSource activator
    if (SUCCEEDED(hr))
    {
        hr = EnumMediaCaptureDevices(targetDeviceId, enumAttributes, sourceActivation);
    }

    // Use the returned activator to create a MediaSourceReader object and identify the stream index we'll read frames
    if (SUCCEEDED(hr))
    {
        hr = InitializeMediaSourceReader(sourceActivation, sourceReader, &streamIndex);
    }

    // Acquire an ID and Friendly Name strings from the Activation attributes
    if (SUCCEEDED(hr))
    {
        hr = AcquireIdentificationStrings(sourceActivation);
    }

    // If all went well, save the key object references and tag the class as Initialized
    if (SUCCEEDED(hr))
    {
        _sourceReader = sourceReader;
        _streamIndex = streamIndex;
    }

    // Read device properties and save them to cached values
    if (SUCCEEDED(hr))
    {
        RefreshStreamPropertyCache();
    }

    return hr;
}

HRESULT MediaDeviceManager::AcquireIdentificationStrings(_In_ const ComPtr<IMFActivate>& sourceActivation)
{
    LPWSTR sourceID = nullptr;
    LPWSTR friendlyName = nullptr;
    UINT32 stringLength;

    HRESULT hr = sourceActivation->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &sourceID, &stringLength);
    if (SUCCEEDED(hr))
    {
        hr = sourceActivation->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &friendlyName, &stringLength);
    }

    if (SUCCEEDED(hr))
    {
        _uniqueSourceID = sourceID;
        _friendlySourceName = friendlyName;
    }
    else
    {
        // Safe to pass in NULL pointers
        CoTaskMemFree(sourceID);
        CoTaskMemFree(friendlyName);
    }

    return hr;
}

HRESULT MediaDeviceManager::EnumMediaCaptureDevices(_In_ LPCWSTR targetDeviceId, _In_ const ComPtr<IMFAttributes>& enumAtributes, _Out_ ComPtr<IMFActivate>& mediaSourceActivate)
{
    IMFActivate **ppDevices = NULL;
    UINT32 deviceCount;

    // Enumerate devices; FAIL if we're unable to find any devices
    // NOTE: We must still release each IMFActivate object and free the memory for the array
    HRESULT hr = MFEnumDeviceSources(enumAtributes.Get(), &ppDevices, &deviceCount);
    if (SUCCEEDED(hr) && deviceCount == 0)
    {
        hr = E_NOT_SET;
    }

    // Select the media source that matches the passed in targetDeviceId
    if (SUCCEEDED(hr))
    {
        bool foundDevice = false;
        for (UINT32 i = 0; i < deviceCount && !foundDevice; i++)
        {
            ComPtr<IMFActivate> currDevice = ppDevices[i];
            LPWSTR sourceID = nullptr;
            UINT32 stringLength;

            // The "symbolic link" is actually the unique device interface ID
            if (SUCCEEDED(currDevice->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &sourceID, &stringLength)))
            {
                // Compare the current source ID string against our target  device
                // If it matches we'll return it's Activation object
                if (_wcsnicmp(targetDeviceId, sourceID, stringLength) == 0)
                {
                    mediaSourceActivate = ppDevices[i];
                    foundDevice = true;
                }
                CoTaskMemFree(sourceID);
            }
        }

        // If a device matching targetDeviceId wasn't found then fail
        if (!foundDevice)
        {
            hr = E_NOT_SET;
        }
    }

    // Each device object returned and the containing array must be released
    for (UINT32 i = 0; i < deviceCount; i++)
    {
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);

    if (FAILED(hr))
    {
        mediaSourceActivate.Reset();
    }
    return hr;
}

HRESULT MediaDeviceManager::InitializeMediaSourceReader(_In_ const ComPtr<IMFActivate>& mediaSourceActivate, _Out_ ComPtr<IMFSourceReader>& sourceReader, _Out_ DWORD* streamIndex)
{
    // Create a MediaSource object from the passed in Activator
    ComPtr<IMFMediaSource> mediaSource;
    HRESULT hr = mediaSourceActivate->ActivateObject(IID_PPV_ARGS(mediaSource.GetAddressOf()));

    // Create a MediaSourceReader object from the activated MediaSource
    if (SUCCEEDED(hr))
    {
        ComPtr<IMFAttributes> readerAttributes;

        hr = MFCreateAttributes(readerAttributes.GetAddressOf(), 1);
        if (SUCCEEDED(hr))
        {
            hr = readerAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        }
        if (SUCCEEDED(hr))
        {
            hr = MFCreateSourceReaderFromMediaSource(mediaSource.Get(), readerAttributes.Get(), sourceReader.GetAddressOf());
        }
    }

    // Examine the media types available from the reader and select one that best matches our needs (if any)
    // Set the choosen MediaType and Stream index to the reader object
    if (SUCCEEDED(hr))
    {
        ComPtr<IMFMediaType> chosenProfile;
        hr = FindCompatibleMediaProfile(sourceReader, chosenProfile.GetAddressOf(), streamIndex);

        if (SUCCEEDED(hr))
        {
            hr = sourceReader->SetCurrentMediaType(*streamIndex, NULL, chosenProfile.Get());
        }
        if (SUCCEEDED(hr))
        {
            // Make sure stream is not selected at this point, it'll be enabled when Start is called by the service
            hr = sourceReader->SetStreamSelection(*streamIndex, FALSE);
        }
        if (SUCCEEDED(hr))
        {
            _sourceAttributes = chosenProfile;
        }
    }

    if (FAILED(hr))
    {
        sourceReader.Reset();
        *streamIndex = static_cast<DWORD>(-1);
    }

    return hr;
}

HRESULT MediaDeviceManager::FindCompatibleMediaProfile(_In_ const ComPtr<IMFSourceReader>& sourceReader, _Outptr_ IMFMediaType** chosenType, _Out_ DWORD* chosenStreamIndex)
{
    std::vector<ComPtr<IMFMediaType>> mediaCandidates;
    HRESULT hr;

    *chosenStreamIndex = static_cast<DWORD>(-1);

    for (UINT32 streamIndex = 0;; streamIndex++)
    {
        for (UINT32 typeIndex = 0;; typeIndex++)
        {
            ComPtr<IMFMediaType> mediaType;

            hr = sourceReader->GetNativeMediaType(streamIndex, typeIndex, mediaType.ReleaseAndGetAddressOf());
            if (SUCCEEDED(hr))
            {
                // Check the sample meets our minimum requirements and if
                // it passes, add it to the list of types we'll consider
                if (IsMediaProfileValid(mediaType))
                {
                    mediaCandidates.push_back(mediaType);
                }
            }
            else if (hr == MF_E_NO_MORE_TYPES || hr == MF_E_INVALIDSTREAMNUMBER) break;
        }

        // If we've found at least 1 valid media type from this stream we don't need to continue enumerating media types
        // Otherwise if no valid media types were found, continue to the next stream (if any more)
        if (!mediaCandidates.empty())
        {
            *chosenStreamIndex = streamIndex;
            break;
        }
        else if (hr == MF_E_INVALIDSTREAMNUMBER) break;
    }

    // Simply select the first valid device
    if (!mediaCandidates.empty())
    {
        *chosenType = mediaCandidates[0].Get();
        (*chosenType)->AddRef();
        hr = S_OK;
    }
    else
    {
        hr = E_NOT_SET;
    }

    return hr;
}

HRESULT MediaDeviceManager::QueryIsMirroredState(_Out_ bool* isMirrored)
{
    if (isMirrored == nullptr)
    {
        return E_INVALIDARG;
    }

    *isMirrored = false;

    ComPtr<IMFMediaSource> mediaSource;
    ComPtr<IKsControl> ksControl;
    HRESULT hr = _sourceReader->GetServiceForStream(static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE), GUID_NULL, IID_PPV_ARGS(&mediaSource));
    if (SUCCEEDED(hr))
    {
        hr = mediaSource.As(&ksControl);
    }

    // First check if device supports this property
    if (SUCCEEDED(hr))
    {
        KSPROPERTY_VIDEOCONTROL_CAPS_S capsQ = { 0 };
        KSPROPERTY_VIDEOCONTROL_CAPS_S capsR = { 0 };

        capsQ.Property.Set = PROPSETID_VIDCAP_VIDEOCONTROL;
        capsQ.Property.Id = KSPROPERTY_VIDEOCONTROL_CAPS;
        capsQ.Property.Flags = KSPROPERTY_TYPE_GET;
        capsQ.StreamIndex = _streamIndex;

        ULONG bytesReturned = 0;
        hr = ksControl->KsProperty((PKSPROPERTY)&capsQ, sizeof(capsQ), &capsR, sizeof(capsR), &bytesReturned);
        if (SUCCEEDED(hr))
        {
            // Fail if mirroring isn't supported on this device 
            if ((capsR.VideoControlCaps & KS_VideoControlFlag_FlipHorizontal) == 0)
            {
                hr = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
        }
    }

    // Query the current mirroring state from the device
    if (SUCCEEDED(hr))
    {
        KSPROPERTY_VIDEOCONTROL_MODE_S mode = { 0 };

        mode.Property.Set = PROPSETID_VIDCAP_VIDEOCONTROL;
        mode.Property.Id = KSPROPERTY_VIDEOCONTROL_MODE;
        mode.Property.Flags = KSPROPERTY_TYPE_GET;
        mode.StreamIndex = _streamIndex;
        mode.Mode = 0;

        ULONG bytesReturned = 0;
        hr = ksControl->KsProperty((PKSPROPERTY)&mode, sizeof(mode), &mode, sizeof(mode), &bytesReturned);
        if (SUCCEEDED(hr))
        {
            *isMirrored = (mode.Mode & KS_VideoControlFlag_FlipHorizontal) != 0;
        }
    }

    return hr;
}

bool MediaDeviceManager::IsMediaProfileValid(_In_ const ComPtr<IMFMediaType>& workingProfile)
{
    GUID guidType;
    UINT32 uintType;

    // Verify media is VIDEO
    HRESULT hr = workingProfile->GetGUID(MF_MT_MAJOR_TYPE, &guidType);
    if (SUCCEEDED(hr))
    {
        if (!IsEqualGUID(guidType, MFMediaType_Video)) { hr = E_FAIL; }
    }

    // Must natively supporty YUY2 video format
    if (SUCCEEDED(hr))
    {
        hr = workingProfile->GetGUID(MF_MT_SUBTYPE, &guidType);
        if (SUCCEEDED(hr))
        {
            if (!IsEqualGUID(MFVideoFormat_YUY2, guidType)) { hr = E_FAIL; }
        }
    }

    // Sample sizes must be fixed
    if (SUCCEEDED(hr))
    {
        hr = workingProfile->GetUINT32(MF_MT_FIXED_SIZE_SAMPLES, &uintType);
        if (SUCCEEDED(hr))
        {
            if (!uintType) { hr = E_FAIL; }
        }
    }

    // Video frames cannot be interlaced
    if (SUCCEEDED(hr))
    {
        hr = workingProfile->GetUINT32(MF_MT_INTERLACE_MODE, &uintType);
        if (SUCCEEDED(hr))
        {
            if (uintType != MFVideoInterlace_Progressive) { hr = E_FAIL; }
        }
    }

    // Use the !! trick to safely convert the int value returned by SUCCEEDED macro to a bool value
    return !!SUCCEEDED(hr);
}

} // end namespace
