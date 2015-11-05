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

#include <vector>
#include <collection.h>
#include <ppltasks.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Mferror.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "Mfuuid.lib")
#pragma comment(lib, "runtimeobject.lib")

#include <agile.h>
#include <Windows.Foundation.Numerics.h>
#include <windows.system.threading.h>
#include <windows.system.threading.core.h>
#include <MemoryBuffer.h>

#include <wrl.h>
#include <ks.h>
#include <ksproxy.h>
#include <ksmedia.h>

// Disable false code analysis warnings
#pragma warning(disable: 26110) // Caller failing to hold lock
#pragma warning(disable: 26165) // Possibly failing to release lock
#pragma warning(disable: 26135) // Missing annotation _Acquires_exclusive_lock_/_Acquires_shared_lock_
#pragma warning(disable: 4127)  // Conditional expression is constant - const flag used to demonstrate different behavior

// Define abbreviations for long namespace names
namespace WDE = Windows::Devices::Enumeration;
namespace WDP = Windows::Devices::Perception;
namespace WDPP = Windows::Devices::Perception::Provider;
namespace WFC = Windows::Foundation::Collections;
namespace WST = Windows::System::Threading;
namespace WMD = Windows::Media::Devices;
namespace WMC = Windows::Media::Capture;

namespace WRL = Microsoft::WRL;
namespace WRLW = Microsoft::WRL::Wrappers;
namespace AWST = ABI::Windows::System::Threading;

namespace MediaFoundationProvider {

// Define GUID for our custom MediaFoundation attribute indicating if LED illumination is on for the current media sample
// {24AE8CD8-117D-41B4-88F6-2AE46EF9AF58}
static const GUID WDPP_ACTIVE_ILLUMINATION_ENABLED = { 0x24ae8cd8, 0x117d, 0x41b4, { 0x88, 0xf6, 0x2a, 0xe4, 0x6e, 0xf9, 0xaf, 0x58 } };

EXTERN_GUID(MF_INTERNAL_CAPTURESOURCE_DEVICECONTROL_COOKIE, 0xA612CDFD, 0xDC58, 0x43CC, 0x9C, 0x0B, 0x81, 0x6B, 0x69, 0x07, 0x50, 0x4F);


inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw Platform::Exception::CreateException(hr);
    }
}

inline void ThrowIfFailed(HRESULT hr, Platform::String^ message)
{
    if (FAILED(hr))
    {
        throw Platform::Exception::CreateException(hr, message);
    }
}

} // end namespace

#include "VideoSourceDescription.h"
#include "MediaDeviceManager.h"
#include "MediaFoundationWrapper.h"
#include "FrameProvider.h"
#include "FrameManager.h"
