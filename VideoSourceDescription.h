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

ref class VideoSourceDescription sealed : Platform::Object
{
public:

    VideoSourceDescription(
        Windows::Graphics::Imaging::BitmapPixelFormat format,
        Windows::Graphics::Imaging::BitmapAlphaMode alpha,
        int width,
        int height,
        double pixelAspectRatio,
        Windows::Foundation::TimeSpan frameDuration);

    VideoSourceDescription(Windows::Foundation::Collections::IPropertySet^ propertySet);

    property Windows::Graphics::Imaging::BitmapPixelFormat BitmapPixelFormat { Windows::Graphics::Imaging::BitmapPixelFormat get(); }
    property Windows::Graphics::Imaging::BitmapAlphaMode BitmapAlphaMode { Windows::Graphics::Imaging::BitmapAlphaMode get(); }
    property int PixelWidth { int get(); }
    property int PixelHeight { int get(); }
    property Windows::Foundation::Size PixelSize { Windows::Foundation::Size get(); }
    property double PixelAspectRatio { double get(); }
    property Windows::Foundation::TimeSpan FrameDuration { Windows::Foundation::TimeSpan get(); }

internal:

    bool IsEqual(VideoSourceDescription^ videoDescription);
    Windows::Foundation::Collections::IPropertySet^ AsPropertySet();

private:

    Windows::Graphics::Imaging::BitmapPixelFormat _bitmapPixelFormat;
    Windows::Graphics::Imaging::BitmapAlphaMode _bitmapAlphaMode;
    int _pixelWidth;
    int _pixelHeight;
    double _pixelAspectRatio;
    Windows::Foundation::TimeSpan _frameDuration;
};

} // end namespace
