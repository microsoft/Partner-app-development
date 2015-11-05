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

VideoSourceDescription::VideoSourceDescription(
    Windows::Graphics::Imaging::BitmapPixelFormat format,
    Windows::Graphics::Imaging::BitmapAlphaMode alpha,
    int width,
    int height,
    double pixelAspectRatio,
    Windows::Foundation::TimeSpan frameDuration) :
    _bitmapPixelFormat(format),
    _bitmapAlphaMode(alpha),
    _pixelWidth(width),
    _pixelHeight(height),
    _pixelAspectRatio(pixelAspectRatio),
    _frameDuration(frameDuration)
{
}

VideoSourceDescription::VideoSourceDescription(Windows::Foundation::Collections::IPropertySet^ propertySet) :
    _bitmapPixelFormat(),
    _bitmapAlphaMode(),
    _pixelWidth(),
    _pixelHeight(),
    _pixelAspectRatio(),
    _frameDuration()
{
    if (propertySet == nullptr)
    {
        throw ref new Platform::NullReferenceException("VideoSourceDescription ctor requires a valid IPropertySet parameter");
    }

    Platform::Object^ returnValue;

    returnValue = propertySet->Lookup(Windows::Devices::Perception::KnownPerceptionVideoProfileProperties::BitmapPixelFormat);
    _bitmapPixelFormat = (Windows::Graphics::Imaging::BitmapPixelFormat)safe_cast<UINT32>(returnValue);

    returnValue = propertySet->Lookup(Windows::Devices::Perception::KnownPerceptionVideoProfileProperties::BitmapAlphaMode);
    _bitmapAlphaMode = (Windows::Graphics::Imaging::BitmapAlphaMode)safe_cast<UINT32>(returnValue);

    returnValue = propertySet->Lookup(Windows::Devices::Perception::KnownPerceptionVideoProfileProperties::Width);
    _pixelWidth = safe_cast<int>(returnValue);

    returnValue = propertySet->Lookup(Windows::Devices::Perception::KnownPerceptionVideoProfileProperties::Height);
    _pixelHeight = safe_cast<int>(returnValue);

    returnValue = propertySet->Lookup(Windows::Devices::Perception::KnownPerceptionVideoProfileProperties::FrameDuration);
    _frameDuration = safe_cast<Windows::Foundation::TimeSpan>(returnValue);
}

Windows::Graphics::Imaging::BitmapPixelFormat VideoSourceDescription::BitmapPixelFormat::get()
{
    return _bitmapPixelFormat;
}

Windows::Graphics::Imaging::BitmapAlphaMode VideoSourceDescription::BitmapAlphaMode::get()
{
    return _bitmapAlphaMode;
}

int VideoSourceDescription::PixelWidth::get()
{
    return _pixelWidth;
}

int VideoSourceDescription::PixelHeight::get()
{
    return _pixelHeight;
}

Windows::Foundation::Size VideoSourceDescription::PixelSize::get()
{
    return Windows::Foundation::Size(static_cast<float>(_pixelWidth), static_cast<float>(_pixelHeight));
}

double VideoSourceDescription::PixelAspectRatio::get()
{
    return _pixelAspectRatio;
}

Windows::Foundation::TimeSpan VideoSourceDescription::FrameDuration::get()
{
    return _frameDuration;
}


Windows::Foundation::Collections::IPropertySet^ VideoSourceDescription::AsPropertySet()
{
    Windows::Foundation::Collections::PropertySet^ propSet = ref new Windows::Foundation::Collections::PropertySet();

    propSet->Insert(Windows::Devices::Perception::KnownPerceptionVideoProfileProperties::BitmapPixelFormat, (UINT32)_bitmapPixelFormat);
    propSet->Insert(Windows::Devices::Perception::KnownPerceptionVideoProfileProperties::BitmapAlphaMode, (UINT32)_bitmapAlphaMode);
    propSet->Insert(Windows::Devices::Perception::KnownPerceptionVideoProfileProperties::Width, _pixelWidth);
    propSet->Insert(Windows::Devices::Perception::KnownPerceptionVideoProfileProperties::Height, _pixelHeight);
    propSet->Insert(Windows::Devices::Perception::KnownPerceptionVideoProfileProperties::FrameDuration, _frameDuration);

    return propSet;
}

bool VideoSourceDescription::IsEqual(VideoSourceDescription^ videoDescription)
{
    return  (videoDescription != nullptr) &&
        (_bitmapPixelFormat == videoDescription->BitmapPixelFormat) &&
        (_bitmapAlphaMode == videoDescription->BitmapAlphaMode) &&
        (_pixelWidth == videoDescription->_pixelWidth) &&
        (_pixelHeight == videoDescription->_pixelHeight) &&
        (_pixelAspectRatio == videoDescription->PixelAspectRatio) &&
        (_frameDuration.Duration == videoDescription->FrameDuration.Duration);
}

} // end namespace