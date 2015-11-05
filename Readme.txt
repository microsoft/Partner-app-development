IFrameProvider implementation sample

This sample demonstrates a basic implementation of the Windows.Devices.Perception.Provider interfaces to
enable Windows Face Authentication (Windows Hello) for a generic infrared video camera.

Specifically, this sample shows how to:
    - Implement IFrameProvider and IFrameProviderManager to deliver infrared frames to SensorDataService
    - Implement an MFT0 module to interface with IFrameProvider and pass device specific properties 
    - Set IFrameProvider Properties to enable FaceAuthentication
    - Respond to Property change requests, specifically handle requests to change camera's ExposureCompensation property
    - Enumerate video capture devices using MediaCapture
    - Open a capture stream using MediaFoundation's IMFSourceReader
    - Read YUY2 video frames from MediaFoundation, convert them to 8-bit grayscale, and deliver them to SensorDataService

This sample is a generic implementation of IFrameProvider, intended to work across a wide variety of capture devices, and
serves as a starting point for developing device-specific providers. Depending on the specific device or scenario, additional
properties or functionality will need to be added.

In order to work with this IFrameProvider sample, a sensor must meet the following requirements:
    - Infrared sensor must function as a standard Windows Media Device, i.e. a webcam
    - Sensor must support YUY2 video mode
    - Video frames must be fixed size and cannot be interlaced

NOTE: The sample provider selects the first valid video capture device during enumeration. Therefore, the target sensor
      must be the only device available on the PC, including any integrated webcams.

IMPORTANT: By default, this sample enumerates basic webcam devices, i.e. KSCATEGORY_VIDEO_CAMERA devices. However, for the
           final implementation, the IR sensor needs to be registered as a KSCATEGORY_SENSOR_CAMERA device and IFrameProvider
           must change its enumeration to match.

           To aid this transition, a flag "SampleFrameProvider::_requiredKsSensorDevice" is provided to control the device
           enumeration behavior. Set this flag to "true" to enumerate and select KSCATEGORY_SENSOR_CAMERA devices.

System Requirements:
    - Windows 10 OS Build 10.0.10240.0 or higher
    - Visual Studio 2015 Build 14.0.22823.1 or higher
    - Windows Platform SDK Build 10.0.10240.0 or higher
    - Windows Driver Kit (WDK) 10.0.26639 or higher

Installing Registry keys:

Registry keys are used to identify the FrameProvider module path and ActivationClass name to the SensorDataService.
Keys for this sample are included but need to be manually installed:
    1. Locate "MediaFoundation.reg" file within the sample’s project folder
    2. Double-click this file to install the keys (must have Administrator access)
    3. Keys are installed to "HKLM\Software\Microsoft\Analog\Providers\MediaFoundation"

NOTE: If these keys are missing or mismatched, the FrameProvider will not function. Make sure to update these keys if
      any of the following are changed:
    - File name of the FrameProvider’s DLL i.e. "MediaFoundation.SourceProvider.dll"
    - File path to the FrameProvider DLL i.e. "%SystemDrive%\Analog\Providers"
    - Name of the class, including the namespace, implementing IFrameProviderManager i.e.
      "MediaFoundationProvider.SampleFrameProviderManager"

Installing MFT0 module:

The FrameProviderSampleMft0 project is derived from the existing "Driver MFT Sample" located here:
https://github.com/Microsoft/Windows-driver-samples/tree/master/avstream/samplemft0
Please refer to this sample for details on building and installing the MFT0 module.

NOTE: This sample defines a different set of class names and IDs (GUIDs) from those used in the Driver MFT0 Sample, and
      you must substitute the identifiers mentioned in the instructions with the corresponding values from this sample.

Specifically:
    - The "CameraPostProcessingPluginCLSID" registry key needs to be set to "{DDBE4BC1-541F-4D43-A25B-1F23E7AF4505}"
    - References to project name or binary file should be replaced with FrameProviderSampleMft0

NOTE: Installation of the MFT0 module isn't required for basic operation of the IFrameProvider, i.e. passing frames
      up to SensorDataService. However, in order for FaceAuthentication to work end-to-end, you must modify and properly
      install the MFT0 module so that IR frames are tagged with the correct properties.

Building and deploying the sample:
    1. Open the FrameProviderSample Solution in Visual Studio
    2. Select Build->Build Solution
    3. In the Solution Explorer window, right-click FrameProviderSample project and select Open Folder in File Explorer
    4. Open FrameProviderSample's output folder (depending on the build configuration)
	   NOTE: Use the x64 build if running on a 64-bit OS
    5. Copy "SampleFrameProvider.dll" to "%SystemDrive%\Analog\Providers"
    6. Follow the instructions in the Driver MFT0 Sample to install the FrameProviderSampleMft0 module
       https://code.msdn.microsoft.com/Driver-MFT-Sample-34ecfecb
    7. Launch the Windows Hello Face setup app under Settings->Accounts->Sign-in Options to start
       SensorDataService, which loads the provider DLL
    8. During enrollment, you should see frames from your sensor in the visualizer

NOTE: The Windows.Devices.Perception APIs can also be used to consume IR frames from SensorDataService and
      display them in your own app.
