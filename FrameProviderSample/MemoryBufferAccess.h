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

#include "pch.h"

//
//
// Helper class to retrieve byte* and capacity from an IMemoryBuffer
// The class guarantees the validity of the byte* for entire object lifetime.
//
// ex:
//      MemoryBufferByteAccess byteAccess;
//      unsigned char *pData = nullptr;
//      unsigned int capacity;
//      HRESULT hr = byteAccess.GetBuffer(spBuffer.Get(), &pData, &capacity);
//
class MemoryBufferByteAccess
{
public:

    HRESULT GetBuffer(_In_ IInspectable *pBuffer, _Outptr_result_bytebuffer_(*pCapacity) BYTE **ppData, _Out_ UINT32 *pCapacity)
    {
#ifdef ____x_Windows_CFoundation_CIMemoryBuffer_FWD_DEFINED__
        Microsoft::WRL::ComPtr<Windows::Foundation::IMemoryBuffer> spMemoryBuffer;
        Microsoft::WRL::ComPtr<Windows::Foundation::IMemoryBufferReference> spBufferReference;
#else
        Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IMemoryBuffer> spMemoryBuffer;
        Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IMemoryBufferReference> spBufferReference;
#endif

        *ppData = nullptr;
        *pCapacity = 0;

        HRESULT hr = pBuffer->QueryInterface(IID_PPV_ARGS(&spMemoryBuffer));

        if (SUCCEEDED(hr))
        {
            hr = spMemoryBuffer->CreateReference(&spBufferReference);
        }

        if (SUCCEEDED(hr))
        {
            hr = spBufferReference.As(&m_spByteAccess);
        }

        if (SUCCEEDED(hr))
        {
            hr = m_spByteAccess->GetBuffer(ppData, pCapacity);
        }

        return hr;
    }

private:

    Microsoft::WRL::ComPtr<Windows::Foundation::IMemoryBufferByteAccess> m_spByteAccess;
};
