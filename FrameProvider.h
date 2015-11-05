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

// Defines the basic types of IR illumination types sensors FaceAuthentication sensors can produce
// SampleFrameProvider specifies one of these types to determine which Provider flags to set
enum class SensorIRIlluminationTypes
{
    // The sensor's IR illuminator flashes on/off every other frame and ambient light
    // subtraction is not performed by the device or driver; the service performs this task
    InterleavedIllumination_UncleanedIR,

    // The sensor's IR illuminator flashes on/off every other frame but ambient light subtraction
    // is performed by the device or driver and "clean IR" frames are submitted to the service
    InterleavedIllumination_CleanIR,

    // The sensor's IR illuminator remains on for every frame; ambient light subtraction
    // is performed by the device or driver to produce "clean IR"
    ContinuousIllumination_CleanIR,
};

ref class SampleFrameProvider : public WDPP::IPerceptionFrameProvider
{
internal:

    SampleFrameProvider(Platform::String^ targetDeviceId);

    // Specify the IR illumination type for this provider's sensor; change if type doesn't match your device
    // This type dictates the Properties set by the provider
    const SensorIRIlluminationTypes _sensorIRIllumination = SensorIRIlluminationTypes::InterleavedIllumination_UncleanedIR;

public:

    virtual ~SampleFrameProvider();

    // IFrameProvider interface properties
    virtual property bool Available { bool get(); }
    virtual property WDPP::PerceptionFrameProviderInfo^ FrameProviderInfo{ WDPP::PerceptionFrameProviderInfo^ get(); }
    virtual property WFC::IPropertySet^ Properties { WFC::IPropertySet^ get(); }

    // IFrameProvider interface methods
    virtual void Start();
    virtual void Stop();
    virtual void SetProperty(WDPP::PerceptionPropertyChangeRequest^ propertyValue);

internal:

    static const bool _requiredKsSensorDevice = false; // If set limits enumeration to devices with KSCATEGORY_SENSOR_CAMERA attribute

private:

    // Internal methods
    WDPP::PerceptionFrame^ CopyMediaSampleToPerceptionFrame();
    VideoSourceDescription^ CreateVideoDescriptionFromMediaSource();
    bool IsExposureCompensationSupported();
    bool SetExposureCompensation(_Inout_ float& newValue);
    void InitializeSourceVideoProperties();
    void UpdateSourceVideoProperties();
    
    // Class fields
    MediaFoundationWrapper _mediaWrapper;
    EventRegistrationToken _readFrameCallbackToken;
    EventRegistrationToken _availablityChangedCallbackToken;

    WFC::IPropertySet^ _properties;
    WDPP::PerceptionFrameProviderInfo^ _providerInfo;
    WDPP::PerceptionVideoFrameAllocator^ _frameAllocator;
    Platform::Agile<WMC::MediaCapture> _mediaCapture;
};

} // end namespace