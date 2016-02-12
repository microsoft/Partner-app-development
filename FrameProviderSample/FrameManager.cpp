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

namespace MediaFoundationProvider {

SampleFrameProviderManager::SampleFrameProviderManager() :
    _providerMap(ref new Platform::Collections::Map<Platform::String^, WDPP::IPerceptionFrameProvider^>(std::less<Platform::String^>())),
    _controlGroup(nullptr),
    _faceAuthGroup(nullptr),
    _deviceWatcher(nullptr),
    _destructorCalled(false)

{
    // Create a DeviceWatcher to enumerate and select the target video device and also signal when changes to this device occur
    // A different query method is used for DEBUG and RELEASE build configurations

#if _DEBUG

    // Here a general device query to is used to select the first available device according to the following modes:
    //
    // - Default mode: Enumerates all available "webcam" devices (KSCATEGORY_VIDEO_CAMERA) on the PC and selects the first available device
    //   This mode is intended for development and testing when a driver INF for the device isn't available, i.e. using the default UVC driver
    //   Set SampleFrameProvider::_requiredKsSensorDevice = false to select this mode
    //
    // - Sensor mode: Enumerates only KSCATEGORY_SENSOR_CAMERA devices and selects the first available device
    //   This mode requires the device to be registered under KSCATEGORY_SENSOR_CAMERA DeviceClass in order to be selected
    //   Set SampleFrameProvider::_requiredKsSensorDevice = true to select this mode
    //

    if (SampleFrameProvider::_requiredKsSensorDevice)
    {
        Platform::Guid sensorCameraClassGuid = KSCATEGORY_SENSOR_CAMERA;
        Platform::String^ queryString = L"System.Devices.InterfaceClassGuid:=\"" + sensorCameraClassGuid.ToString() + "\"";
        _deviceWatcher = Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(queryString);
    }
    else
    {
        _deviceWatcher = Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(Windows::Devices::Enumeration::DeviceClass::VideoCapture);
    }

#else

    // The RELEASE configuration requires a specific device query string
    //
    // *** IMPORTANT ***
    //
    // The actual implementation of your IFrameProvider must query for and connect to a single device on the system. This means the
    // enumeration performed by this sample is inadequate and must be replaced with an Advance Query Syntax string that returns your
    // IR sensor camera and nothing else
    //

    Platform::String^ vendorId = L"1234";    // Replace with your own Vendor ID number
    Platform::String^ productId = L"5678";   // Replace with this sensor's Product ID number
    Platform::Guid sensorCameraClassGuid = KSCATEGORY_SENSOR_CAMERA;

    Platform::String^ queryString = L"System.Devices.InterfaceClassGuid:=\"" + sensorCameraClassGuid.ToString() + "\"" +
                                    L" AND System.DeviceInterface.WinUsb.UsbVendorId:=" + vendorId + 
                                    L" AND System.DeviceInterface.WinUsb.UsbProductId:=" + productId;

    _deviceWatcher = Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(queryString);

#endif // #if _DEBUG

    _deviceAddedRegToken = _deviceWatcher->Added +=
        ref new Windows::Foundation::TypedEventHandler<WDE::DeviceWatcher ^, WDE::DeviceInformation ^>
        (this, &SampleFrameProviderManager::DeviceAddedEventHandler);
    
    _deviceRemovedRegToken = _deviceWatcher->Removed +=
        ref new Windows::Foundation::TypedEventHandler<WDE::DeviceWatcher ^, WDE::DeviceInformationUpdate ^>
        (this, &SampleFrameProviderManager::DeviceRemovedEventHandler);
            
    _deviceWatcher->Start();
}

SampleFrameProviderManager::~SampleFrameProviderManager()
{
    auto lock = _destructionLocker.Lock();

    _destructorCalled = true;

    if (_deviceWatcher != nullptr)
    {
        _deviceWatcher->Stop();

        _deviceWatcher->Added -= _deviceAddedRegToken;
        _deviceWatcher->Removed -= _deviceRemovedRegToken;
        _deviceWatcher = nullptr;
    }

    ReleaseProvider();
}

WDPP::IPerceptionFrameProvider^ SampleFrameProviderManager::GetFrameProvider(WDPP::PerceptionFrameProviderInfo^ providerInfo)
{
    if (providerInfo != nullptr && _providerMap->HasKey(providerInfo->Id))
    {
        return _providerMap->Lookup(providerInfo->Id);
    }
    return nullptr;
}

void SampleFrameProviderManager::DeviceAddedEventHandler(WDE::DeviceWatcher^ sender, WDE::DeviceInformation^ args)
{
    auto lock = _destructionLocker.Lock();

    // Do NOT create the FrameProvider if this object's destructor has been invoked
    if (!_destructorCalled)
    {
        CreateProvider(args->Id);
    }
}

void SampleFrameProviderManager::DeviceRemovedEventHandler(WDE::DeviceWatcher^ sender, WDE::DeviceInformationUpdate^ args)
{
    auto lock = _destructionLocker.Lock();

    ReleaseProvider();
}

void SampleFrameProviderManager::CreateProvider(Platform::String^ targetDeviceId)
{
    auto lock = _connectionLocker.Lock();

    try
    {
        // Initialize our IFrameProvider object and add it to the Manager's internal map
        SampleFrameProvider^ provider = ref new SampleFrameProvider(targetDeviceId);
        _providerMap->Insert(provider->FrameProviderInfo->Id, provider);
        
        // Register our Provider object with the service
        WDPP::PerceptionFrameProviderManagerService::RegisterFrameProviderInfo(this, provider->FrameProviderInfo);

        // Initialize a ControlGroup object which allows the clients to set/change properties on the Provider
        auto controllerModeIds = ref new Platform::Collections::Vector<Platform::String^>();
        controllerModeIds->Append(provider->FrameProviderInfo->Id);
        _controlGroup = ref new WDPP::PerceptionControlGroup(controllerModeIds->GetView());
        WDPP::PerceptionFrameProviderManagerService::RegisterControlGroup(this, _controlGroup);

        // Perform the steps necessary to register our Provider for FaceAuthentication
        // Create a list of all Providers that can support FaceAuthentication; only a single Provider in this implementation
        auto faceAuthProviderIds = ref new Platform::Collections::Vector<Platform::String^>();
        faceAuthProviderIds->Append(provider->FrameProviderInfo->Id);

        // Create handlers for the "Start" and "Stop" FaceAuthentication events
        auto startFaceAuthHandler = ref new WDPP::PerceptionStartFaceAuthenticationHandler(
            this,
            &SampleFrameProviderManager::StartFaceAuthentication);

        auto stopFaceAuthHandler = ref new WDPP::PerceptionStopFaceAuthenticationHandler(
            this,
            &SampleFrameProviderManager::StopFaceAuthentication);

        // Create a FaceAuthentication group containing our Providers list and event handlers
        _faceAuthGroup = ref new WDPP::PerceptionFaceAuthenticationGroup(
            faceAuthProviderIds->GetView(),
            startFaceAuthHandler,
            stopFaceAuthHandler);

        // Finally register our FaceAuthentication group with the service
        WDPP::PerceptionFrameProviderManagerService::RegisterFaceAuthenticationGroup(this, _faceAuthGroup);
    }
    catch (Platform::Exception^ ex)
    {
        // Clean up Provider and Group objects if something goes wrong
        ReleaseProvider();
    }
}

void SampleFrameProviderManager::ReleaseProvider()
{
    auto lock = _connectionLocker.Lock();

    // NOTE: When SensorDataService unloads a given FrameProvider, it directly Closes (aka Disposes) the IPerceptionFrameProvider object before
    // Closing the IPerceptionFrameProviderManager object that created it. This means, any references to the FrameProvider object are now invalid 
    // and any API calls made using this Provider, e.g. UnregisterControlGroup, will throw an InvalidArgumentException.
 
    // Do not attempt to Unregister control groups or release the FrameProvider when this manager is being destroyed
    if (!_destructorCalled)
    {
        // Unregister the FaceAuthentication group
        if (_faceAuthGroup != nullptr)
        {
            WDPP::PerceptionFrameProviderManagerService::UnregisterFaceAuthenticationGroup(this, _faceAuthGroup);
            _faceAuthGroup = nullptr;
        }

        // Unregister the ControlGroup
        if (_controlGroup != nullptr)
        {
            WDPP::PerceptionFrameProviderManagerService::UnregisterControlGroup(this, _controlGroup);
            _controlGroup = nullptr;
        }

        // Unregister our Provider object and release it
        if (_providerMap->Size > 0)
        {
            auto firstItem = _providerMap->First();
            WDPP::PerceptionFrameProviderInfo^ providerInfo = firstItem->Current->Value->FrameProviderInfo;
            if (providerInfo != nullptr)
            {
                WDPP::PerceptionFrameProviderManagerService::UnregisterFrameProviderInfo(this, providerInfo);
            }

            _providerMap->Clear();
        }
    }
    else
    {
        _faceAuthGroup = nullptr;
        _controlGroup = nullptr;
        _providerMap->Clear();
    }

    //
    // TODO: Add any other device specific shutdown code
    //
}

bool SampleFrameProviderManager::StartFaceAuthentication(WDPP::PerceptionFaceAuthenticationGroup^ group)
{
    // Sensor specific functionality
    // Toggle whatever settings are needed to make your Provider meet the FaceAuthentication spec
    // For example, setting exposure, changing framerate, etc.

    // Return true when the Provider is fully prepared to handle FaceAuthentication frames
    // If false is returned, FaceAuthentication will fail all the way to the client
    return true;
}

void SampleFrameProviderManager::StopFaceAuthentication(WDPP::PerceptionFaceAuthenticationGroup^ group)
{
    // Sensor specific functionality
    // Undo any changes that were made in EnterFaceAuthentication
}

} // end namespace


