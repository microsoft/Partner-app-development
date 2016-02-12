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

public ref class SampleFrameProviderManager sealed : public WDPP::IPerceptionFrameProviderManager
{
public:

    SampleFrameProviderManager();
    virtual ~SampleFrameProviderManager();

    // IFrameProviderManager interface methods
    virtual WDPP::IPerceptionFrameProvider^ GetFrameProvider(WDPP::PerceptionFrameProviderInfo^ frameProviderInfo);

private:

    // Internal methods
    void CreateProvider(Platform::String^ targetDeviceId);
    void ReleaseProvider();

    // Face Authentication event handlers
    bool StartFaceAuthentication(WDPP::PerceptionFaceAuthenticationGroup^ group);
    void StopFaceAuthentication(WDPP::PerceptionFaceAuthenticationGroup^ group);

    // DeviceWatcher event handlers
    void DeviceAddedEventHandler(WDE::DeviceWatcher^ sender, WDE::DeviceInformation^ args);
    void DeviceRemovedEventHandler(WDE::DeviceWatcher^ sender, WDE::DeviceInformationUpdate^ args);

    // Class fields
    Platform::Collections::Map<Platform::String^, WDPP::IPerceptionFrameProvider^>^ _providerMap;
    WDPP::PerceptionControlGroup^ _controlGroup;
    WDPP::PerceptionFaceAuthenticationGroup^ _faceAuthGroup;
    WDE::DeviceWatcher^ _deviceWatcher;
    WRLW::CriticalSection _connectionLocker;
    WRLW::CriticalSection _destructionLocker;

    bool _destructorCalled;

    Windows::Foundation::EventRegistrationToken _deviceAddedRegToken;
    Windows::Foundation::EventRegistrationToken _deviceRemovedRegToken;
};

} // end namespace