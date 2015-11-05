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

// dllmain.h : Declaration of module class.

class CFrameProviderSampleMft0Module : public ATL::CAtlDllModuleT< CFrameProviderSampleMft0Module >
{
public :
    DECLARE_LIBID(LIBID_FrameProviderSampleMft0Lib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_FRAMEPROVIDERSAMPLEMFT0, "{9DA12F8B-54A4-4C67-A3D2-AD09F1A79102}")
};

extern class CFrameProviderSampleMft0Module _AtlModule;
