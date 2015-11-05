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
#include "MemoryBufferAccess.h"

using namespace Microsoft::WRL;

namespace MediaFoundationProvider {

SampleFrameProvider::SampleFrameProvider(Platform::String^ targetDeviceId) :
    _readFrameCallbackToken(),
    _availablityChangedCallbackToken(),
    _providerInfo(nullptr),
    _frameAllocator(nullptr),
    _properties(nullptr),
    _mediaCapture(nullptr)
{
    _properties = ref new WFC::PropertySet();

    if (targetDeviceId == nullptr)
    {
        throw ref new Platform::InvalidArgumentException("targetDeviceId parameter wasn't specified");
    }

    // Initialize our MediaFoundation wrapper class which does most of the heavy setup work
    // The targetDeviceId is the unique device identifier acquired through device enumeration
    const HRESULT hr = _mediaWrapper.Initialize(targetDeviceId->Data());
    ThrowIfFailed(hr, L"Failed to initialize MediaFoundation");

    // Initialize MediaCapture in order to acquire a VideoDeviceController object later
    // NOTE: We are not using MediaCapture to stream frames (using MediaFoundation's IMFSourceReader)
    // but in order to access extended camera properties, e.g. ExposureCompensation, we must utilize
    // VideoDeviceController, which is only obtained through a MediaCapture instance
    WMC::MediaCaptureInitializationSettings^ initSettings = ref new WMC::MediaCaptureInitializationSettings();
    _mediaCapture = ref new WMC::MediaCapture();

    // Connect MediaCapture to the same device in MediaFoundation initialization
    initSettings->VideoDeviceId = targetDeviceId;
    initSettings->StreamingCaptureMode = WMC::StreamingCaptureMode::Video;

    // Provider doesn't need to be asynchronous, just wait for initialization to complete
    auto initTask = Concurrency::create_task(_mediaCapture->InitializeAsync(initSettings));
    initTask.wait();

    // Using the video mode selected by MediaWrapper, set essential video Properties for this FrameProvider
    InitializeSourceVideoProperties();

    // Fill ProviderInfo properties from MediaFoundation values
    _providerInfo = ref new WDPP::PerceptionFrameProviderInfo();
    _providerInfo->DeviceKind = L"com.microsoft.sample.webcam";
    _providerInfo->DisplayName = ref new Platform::String(_mediaWrapper.GetFriendSourceName());
    _providerInfo->Id = ref new Platform::String(_mediaWrapper.GetUniqueSourceID());
    _providerInfo->FrameKind = WDPP::KnownPerceptionFrameKind::Infrared; 
    _providerInfo->Hidden = false;
    
    // Subscribe to MediaFoundationWrapper's ReadFrame event, which is fired when a new sample is acquired from MediaFoundation
    // In response, this class will copy out the frame data and publish it to the Service
    _mediaWrapper.SubscribeReadFrame(Callback<ABI::Windows::System::Threading::IWorkItemHandler>([this](ABI::Windows::Foundation::IAsyncAction* /*asyncAction*/)
    {
        WDPP::PerceptionFrame^ outputFrame = this->CopyMediaSampleToPerceptionFrame();
        if (outputFrame != nullptr)
        {
            WDPP::PerceptionFrameProviderManagerService::PublishFrameForProvider(this, outputFrame);
        }
        return S_OK;
    }).Get(), &_readFrameCallbackToken);
        
    // Subscribe to MediaFoundationWrapper's AvailableChanged event, which is fired when we lose connectivity with the device
    // In response, publish the new Availablity state to the Service
    _mediaWrapper.SubscribeAvailableChanged(Callback<ABI::Windows::System::Threading::IWorkItemHandler>([this](ABI::Windows::Foundation::IAsyncAction* /*asyncAction*/)
    {
        WDPP::PerceptionFrameProviderManagerService::UpdateAvailabilityForProvider(this, _mediaWrapper.IsAvailable());

        return S_OK;
    }).Get(), &_availablityChangedCallbackToken);
}

SampleFrameProvider::~SampleFrameProvider()
{
}

void SampleFrameProvider::Start()
{
    if (!_mediaWrapper.IsRunning())
    {
        HRESULT hr = _mediaWrapper.Start();
        ThrowIfFailed(hr, L"Failed to start reading frames from MediaFoundation");

        UpdateSourceVideoProperties();
    }
}

void SampleFrameProvider::Stop()
{
    if (_mediaWrapper.IsRunning())
    {
        HRESULT hr = _mediaWrapper.Stop(false);
        ThrowIfFailed(hr, "Failed to stop reading frames from MediaFoundation");
    }
}

void SampleFrameProvider::SetProperty(WDPP::PerceptionPropertyChangeRequest^ request)
{
    WDP::PerceptionFrameSourcePropertyChangeStatus status;

    // This implementation of IFrameProvider supports only a single video profile and so changing profiles isn't supported.
    // If IFrameProvider were to support multiple profiles, then functionality to reinitialize the device using the new
    // valus needs to be added here.
    if (request->Name == WDP::KnownPerceptionVideoFrameSourceProperties::VideoProfile)
    {
        status = WDP::PerceptionFrameSourcePropertyChangeStatus::PropertyNotSupported;
    }
    else if ((request->Name == WDP::KnownPerceptionVideoFrameSourceProperties::SupportedVideoProfiles) ||
        (request->Name == WDP::KnownPerceptionVideoFrameSourceProperties::AvailableVideoProfiles))
    {
        status = WDP::PerceptionFrameSourcePropertyChangeStatus::PropertyReadOnly;
    }
    else if (request->Name == WDP::KnownPerceptionInfraredFrameSourceProperties::ExposureCompensation)
    {
        float requestedValue = safe_cast<float>(request->Value);

        if (!IsExposureCompensationSupported())
        {
            status = WDP::PerceptionFrameSourcePropertyChangeStatus::PropertyNotSupported;
        }
        else if (SetExposureCompensation(requestedValue))
        {
            // Attempt to set the new value to the device and if successful update Property value
            // NOTE: requestedValue passed by reference since value is modified if outside min/max range
            _properties->Insert(request->Name, requestedValue);
            status = WDP::PerceptionFrameSourcePropertyChangeStatus::Accepted;
        }
        else
        {
            status = WDP::PerceptionFrameSourcePropertyChangeStatus::ValueOutOfRange;
        }
    }
    else
    {
        _properties->Insert(request->Name, request->Value);
        status = WDP::PerceptionFrameSourcePropertyChangeStatus::Accepted;
    }

    request->Status = status;
}

bool SampleFrameProvider::Available::get()
{
    return _mediaWrapper.IsAvailable();
}

WDPP::PerceptionFrameProviderInfo^ SampleFrameProvider::FrameProviderInfo::get()
{
    return _providerInfo;
}

WFC::IPropertySet^ SampleFrameProvider::Properties::get()
{
    return _properties;
}

VideoSourceDescription^ SampleFrameProvider::CreateVideoDescriptionFromMediaSource()
{
    ComPtr<IMFMediaType> sourceAttributes;
    UINT32 numerator;
    UINT32 denominator;

    sourceAttributes = _mediaWrapper.GetSourceAttributes();

    // Verify source media format is YUY2
    GUID guidType;
    HRESULT hr = sourceAttributes->GetGUID(MF_MT_SUBTYPE, &guidType);
    ThrowIfFailed(hr, L"Failed to read video frame type from source media");
    
    if (!IsEqualGUID(MFVideoFormat_YUY2, guidType))
    {
        ThrowIfFailed(E_INVALID_PROTOCOL_FORMAT, L"Source media must return YUY2 format");
    }

    // Get the size (width/heigh) of each frame
    UINT32 width;
    UINT32 height;
    hr = MFGetAttributeSize(sourceAttributes.Get(), MF_MT_FRAME_SIZE, &width, &height);
    ThrowIfFailed(hr, L"Failed to read frame size from source media");

    // Get the aspect ratio of each frame
    hr = MFGetAttributeRatio(sourceAttributes.Get(), MF_MT_PIXEL_ASPECT_RATIO, &numerator, &denominator);
    ThrowIfFailed(hr, L"Failed to read pixel aspect ratio from source media");
    double aspectRatio = static_cast<double>(numerator) / static_cast<double>(denominator);

    // Get the frame rate
    hr = MFGetAttributeRatio(sourceAttributes.Get(), MF_MT_FRAME_RATE_RANGE_MAX, &numerator, &denominator);
    ThrowIfFailed(hr, L"Failed to read frame rate from source media");

    // Convert framerate to Timespan with ticks of 100 nanoseconds each
    double workingVar = static_cast<double>(numerator) / static_cast<double>(denominator);
    workingVar = (1.0e7 / workingVar) + 0.5;
    Windows::Foundation::TimeSpan frameDuration = Windows::Foundation::TimeSpan{ static_cast<INT64>(workingVar) };

    // NOTE: Only 8-bit IR is supported and if the sensor outputs IR at a higher bit depth it must be down sampled
    // For sensors that output YUY2 frames, the image must be manually converted to the Gray8
    VideoSourceDescription^ videoProfile = ref new VideoSourceDescription(
        Windows::Graphics::Imaging::BitmapPixelFormat::Gray8,
        Windows::Graphics::Imaging::BitmapAlphaMode::Ignore,
        width,
        height,
        aspectRatio,
        frameDuration);

    return videoProfile;
}

WDPP::PerceptionFrame^ SampleFrameProvider::CopyMediaSampleToPerceptionFrame()
{
    WDPP::PerceptionFrame^ outputFrame = nullptr;
    ComPtr<IMFSample> mediaSample;

    if (_frameAllocator == nullptr) return nullptr;

    HRESULT hr = _mediaWrapper.GetCurrentFrameResult();
    if (SUCCEEDED(hr))
    {
        mediaSample = _mediaWrapper.GetCurrentFrame();
        if (SUCCEEDED(hr))
        {
            outputFrame = _frameAllocator->AllocateFrame();
        }
    }

    if (mediaSample != nullptr && outputFrame != nullptr)
    {
        ComPtr<IMFMediaBuffer> sampleBuffer;
        hr = mediaSample->GetBufferByIndex(0, sampleBuffer.GetAddressOf());

        // Lock the source and destination buffers to perform the copy
        if (SUCCEEDED(hr))
        {
            BYTE* srcBuffer;
            DWORD srcLength;
            BYTE* destBuffer;
            UINT32 destLength;

            MemoryBufferByteAccess destByteAccess;
            hr = destByteAccess.GetBuffer(reinterpret_cast<IInspectable*>(outputFrame->FrameData), &destBuffer, &destLength);
            
            if (SUCCEEDED(hr))
            {
                hr = sampleBuffer->Lock(&srcBuffer, NULL, &srcLength);
                if (SUCCEEDED(hr))
                {
                    // The Gray8 destination buffer must be exactly half the size of the source YUY2 buffer
                    if (destLength * 2 == srcLength)
                    {
                        // Convert the YUY2 image data into Gray8
                        // Simply strip the Luminance byte (Y component) from the Source YU/V word
                        // and copy it into the corresponding byte of the destination buffer
                        SHORT* src = reinterpret_cast<SHORT*>(srcBuffer);
                        BYTE* dest = destBuffer;
                        for (UINT32 i = 0; i < destLength; i++)
                        {
                            dest[i] = static_cast<BYTE>(src[i] & 0xFF);
                        }
                    }

                    sampleBuffer->Unlock();
                }
            }

            // Read the "Illumination Enabled" attribute from the media sample and set the output Property accordingly
            // NOTE: Each frame must be tagged with this Property otherwise frames won't be delivered to the client app
            // and so a default value is set to ensure frames are always available
            // 
            // IMPORTANT: This is a custom GUID assigned to the sample within MFT0
            // The MFT0 code, which is installed and associated with the device driver, is responsible for
            // polling the LED illumination state from the device and tagging the sample with this value
            UINT32 illuminationEnabled = true;
            mediaSample->GetUINT32(WDPP_ACTIVE_ILLUMINATION_ENABLED, &illuminationEnabled);
            outputFrame->Properties->Insert(
                WDP::KnownPerceptionInfraredFrameSourceProperties::ActiveIlluminationEnabled,
                illuminationEnabled != 0 ? true : false);

            // Set the IsMirrored property for this frame according to cached value
            // This property is also set to Provider object's _properties field during initialization
            // NOTE: The Mirrored state is queried when device is Initialized or Activated and cached in a class field
            outputFrame->Properties->Insert(
                Windows::Devices::Perception::KnownPerceptionVideoFrameSourceProperties::IsMirrored,
                _mediaWrapper.IsMirrored());
        }

        // Set the output VideoFrame's timestamp
        // NOTE: Duration's value is in 100 nanosecond units (ticks) which is also used by MediaFoundation
        if (SUCCEEDED(hr))
        {
            Windows::Foundation::TimeSpan systemRelativeTime;
            systemRelativeTime.Duration = MFGetSystemTime();
            outputFrame->RelativeTime = systemRelativeTime;
        }
    }
    else
    {
        hr = E_FAIL;
    }

    return (SUCCEEDED(hr) ? outputFrame : nullptr);
}

bool SampleFrameProvider::IsExposureCompensationSupported()
{
    bool supported = false;

    try
    {
        WMD::ExposureCompensationControl^ exposureControl = _mediaCapture->VideoDeviceController->ExposureCompensationControl;
        supported = exposureControl->Supported;
    }
    catch (Platform::Exception^) { ; }

    return supported;
}

bool SampleFrameProvider::SetExposureCompensation(_Inout_ float& newValue)
{
    WMD::ExposureCompensationControl^ exposureControl = _mediaCapture->VideoDeviceController->ExposureCompensationControl;
    bool successful = false;

    // Catch any exceptions and return if set operation was succesful or not
    try
    {
        if (newValue < exposureControl->Min)
        {
            newValue = exposureControl->Min;
        }

        if (newValue > exposureControl->Max)
        {
            newValue = exposureControl->Max;
        }

        // Apply the new ExposureCompensation value to the device and wait for the result
        auto setTask = Concurrency::create_task(exposureControl->SetValueAsync(newValue));
        setTask.wait();

        successful = true;
    }
    catch (Platform::Exception^) { ; }

    return successful;
}

void SampleFrameProvider::InitializeSourceVideoProperties()
{
    // Collect the key video properties from the MediaFoundaton source object
    // VideoSourceDescription is a helper class to store the parameters
    VideoSourceDescription^ videoProfile = CreateVideoDescriptionFromMediaSource();

    // Initialize FrameAllocator object according to the video frame parameters acquired from MediaFoundation
    _frameAllocator = ref new WDPP::PerceptionVideoFrameAllocator(
        2, /*Num requested frames*/
        videoProfile->BitmapPixelFormat,
        videoProfile->PixelSize,
        videoProfile->BitmapAlphaMode);

    // IFrameProvider objects are required to expose the current video profile data via their Properties
    // We must set at least 1 video profile for the VideoProfile, SupportedVideoProfiles and AvailableVideoProfiles properties
    _properties->Insert(
        WDP::KnownPerceptionVideoFrameSourceProperties::VideoProfile,
        videoProfile->AsPropertySet());
    
    // Note that the Value for SupportedVideoProfiles and AvailableVideoProfiles is a Vector of IPropertySets since
    // a single IFrameProvider can support multiple video modes. 
    // However, in this implementation we only support a single profile
    auto supportedVideoProfiles = ref new Platform::Collections::Vector<WFC::IPropertySet^>();
    supportedVideoProfiles->Append(videoProfile->AsPropertySet());

    // Use this property to list all supported VideoProfiles supported by this Provider
    _properties->Insert(
        WDP::KnownPerceptionVideoFrameSourceProperties::SupportedVideoProfiles,
        supportedVideoProfiles->GetView());

    // Use this property to specify the set of VideoProfiles currently available at this time.
    // Again, since this implementation only support a single Profile, this value is the same as SupportedVideoProfiles
    _properties->Insert(
        WDP::KnownPerceptionVideoFrameSourceProperties::AvailableVideoProfiles,
        supportedVideoProfiles->GetView());

    // Package the Provider's ID into a String Vector
    // NOTE: For MediaFoundation we're using the SourceReader's "symbolic Link" string as our ID
    auto providerIds = ref new Platform::Collections::Vector<Platform::String^>();
    auto mediaSourceId = ref new Platform::String(_mediaWrapper.GetUniqueSourceID());
    providerIds->Append(mediaSourceId);

    // This property passes the unique device ID to the runtime
    _properties->Insert(
        WDP::KnownPerceptionFrameSourceProperties::PhysicalDeviceIds,
        providerIds->GetView());

    // Set the ExposureCompensation property according to the current state of the device
    WMD::ExposureCompensationControl^ exposureControl = _mediaCapture->VideoDeviceController->ExposureCompensationControl;
    if (exposureControl->Supported)
    {
        _properties->Insert(
            WDP::KnownPerceptionInfraredFrameSourceProperties::ExposureCompensation,
            exposureControl->Value);
    }

    // Set IsMirrored property according to the current state of the device
    // This property is also set to each individual PerceptionFrame’s PropertySet 
    // NOTE: The Mirrored state is queried when device is Initialized or Activated and cached in a class field
    _properties->Insert(
        Windows::Devices::Perception::KnownPerceptionVideoFrameSourceProperties::IsMirrored,
        _mediaWrapper.IsMirrored());

    //
    // NOTE: The following properties specify the illumination behavior of the IR sensor,
    // which must be precisely set in order for FaceAuthentication to work properly
    //
    // For this sample, these Properties are set according to the basic enum value specified
    // by _sensorIRIllumination. Please refer to the SensorIRIlluminationTypes definition
    // for details on each IR illumination type
    //

    switch (_sensorIRIllumination)
    {
        // Specifies Properties:
        // InterleavedIlluminationEnabled = true - Informs Service that IR illuminator flashes on/off
        // AmbientSubtractionEnabled = false - Sensor does NOT perform ambient light subtraction; FaceAuth must do it
        case SensorIRIlluminationTypes::InterleavedIllumination_UncleanedIR:

            _properties->Insert(
                WDP::KnownPerceptionInfraredFrameSourceProperties::InterleavedIlluminationEnabled,
                true);

            _properties->Insert(
                WDP::KnownPerceptionInfraredFrameSourceProperties::AmbientSubtractionEnabled,
                false);

            break;

        // Specifies Properties:
        // InterleavedIlluminationEnabled = true - Informs Service that IR illuminator flashes on/off
        // AmbientSubtractionEnabled = true - Ambient light subtract performed by device or driver; "Clean IR" frames are delivered
        case SensorIRIlluminationTypes::InterleavedIllumination_CleanIR:

            _properties->Insert(
                WDP::KnownPerceptionInfraredFrameSourceProperties::InterleavedIlluminationEnabled,
                true);

            _properties->Insert(
                WDP::KnownPerceptionInfraredFrameSourceProperties::AmbientSubtractionEnabled,
                true);

            break;

        // Specifies Properties:
        // InterleavedIlluminationEnabled = false - Informs Service that IR illuminator remains on (doesn't flash)
        // AmbientSubtractionEnabled = true - Ambient light subtract performed by device or driver; "Clean IR" frames are delivered
        case SensorIRIlluminationTypes::ContinuousIllumination_CleanIR:
            
            _properties->Insert(
                WDP::KnownPerceptionInfraredFrameSourceProperties::InterleavedIlluminationEnabled,
                false);

            _properties->Insert(
                WDP::KnownPerceptionInfraredFrameSourceProperties::AmbientSubtractionEnabled,
                true);

            break;

        default:

            throw ref new Platform::OutOfBoundsException(L"The enum value of _sensorIRIllumination is invalid");
    };

    //
    // TODO: Insert additional Properties as needed to fully describe your specific device (infrared camera)
    //  You should consider properties from these types
    //  - KnownPerceptionFrameSourceProperties
    //  - KnownPerceptionInfraredFrameSourceProperties
    //  - KnownCameraIntrinsicsProperties
    //
}

void SampleFrameProvider::UpdateSourceVideoProperties()
{
    // Update the PropertySet for device properties that may have changed since Provider was initialized

    // Update the ExposureCompensation property with the current device state
    WMD::ExposureCompensationControl^ exposureControl = _mediaCapture->VideoDeviceController->ExposureCompensationControl;
    if (exposureControl->Supported)
    {
        _properties->Insert(
            WDP::KnownPerceptionInfraredFrameSourceProperties::ExposureCompensation,
            exposureControl->Value);
    }

    // Refresh the IsMirrored Property 
    _properties->Insert(
        Windows::Devices::Perception::KnownPerceptionVideoFrameSourceProperties::IsMirrored,
        _mediaWrapper.IsMirrored());
}

} // end namespace