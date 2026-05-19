/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2025 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <fstream>
#include "devicesettings.h"
#include "FrontPanelIndicatorMock.h"
#include "deepSleepMgr.h"
#include "PowerManagerHalMock.h"
#include "MfrMock.h"

#include <mutex>
#include <condition_variable>
 
 
#define JSON_TIMEOUT   (1000)
#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define DISPLAYSETTINGS_CALLSIGN  _T("org.rdk.DisplaySettings.1")
#define DISPLAYSETTINGSL2TEST_CALLSIGN _T("L2tests.1")
using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;


class DisplaySettings_L2test : public L2TestMocks {
protected:
    Core::JSONRPC::Message message;
    string response;
    // testing::NiceMock<FrontPanelIndicatorMock> frontPanelIndicatorMock;
    // IARM_EventHandler_t checkAtmosCapsEventHandler = nullptr;
    // IARM_EventHandler_t DisplResolutionHandler = nullptr;
    // IARM_EventHandler_t dsHdmiEventHandler = nullptr;
    // IARM_EventHandler_t formatUpdateEventHandler = nullptr;
    // IARM_EventHandler_t powerEventHandler = nullptr;
    // IARM_EventHandler_t audioPortStateEventHandler = nullptr;
    // IARM_EventHandler_t dsSettingsChangeEventHandler = nullptr;
    // IARM_EventHandler_t ResolutionPostChange = nullptr;

    virtual ~DisplaySettings_L2test() override;
        
    public:
        DisplaySettings_L2test();
        device::Host::IDisplayEvents* de_listener;
        device::Host::IAudioOutputPortEvents* aope_listener;
        device::Host::IDisplayDeviceEvents* dde_listener;
        device::Host::IHdmiInEvents* hie_listener;
        device::Host::IVideoDeviceEvents* vde_listener;
        device::Host::IVideoOutputPortEvents* vope_listener;

};
 
/**
* @brief Constructor for DisplaySettings L2 test class
*/
DisplaySettings_L2test::DisplaySettings_L2test()
        : L2TestMocks()
{
    
        printf("DISPLAYSETTINGS Constructor\n");
        uint32_t status = Core::ERROR_GENERAL;

        EXPECT_CALL(*p_powerManagerHalMock, PLAT_DS_INIT())
        .WillOnce(::testing::Return(DEEPSLEEPMGR_SUCCESS));

        EXPECT_CALL(*p_powerManagerHalMock, PLAT_INIT())
        .WillRepeatedly(::testing::Return(PWRMGR_SUCCESS));

        EXPECT_CALL(*p_powerManagerHalMock, PLAT_API_SetWakeupSrc(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(PWRMGR_SUCCESS));

        ON_CALL(*p_rfcApiImplMock, getRFCParameter(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
        [](char* pcCallerID, const char* pcParameterName, RFC_ParamData_t* pstParamData) {
           if (strcmp("RFC_DATA_ThermalProtection_POLL_INTERVAL", pcParameterName) == 0) {
               strcpy(pstParamData->value, "2");
               return WDMP_SUCCESS;
           } else if (strcmp("RFC_ENABLE_ThermalProtection", pcParameterName) == 0) {
               strcpy(pstParamData->value, "true");
               return WDMP_SUCCESS;
           } else if (strcmp("RFC_DATA_ThermalProtection_DEEPSLEEP_GRACE_INTERVAL", pcParameterName) == 0) {
               strcpy(pstParamData->value, "6");
               return WDMP_SUCCESS;
           } else {
               /* The default threshold values will assign, if RFC call failed */
               return WDMP_FAILURE;
           }
        }));

        EXPECT_CALL(*p_mfrMock, mfrSetTempThresholds(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke(
        [](int high, int critical) {
           EXPECT_EQ(high, 100);
           EXPECT_EQ(critical, 110);
           return mfrERR_NONE;
        }));

        EXPECT_CALL(*p_powerManagerHalMock, PLAT_API_GetPowerState(::testing::_))
        .WillRepeatedly(::testing::Invoke(
        [](PWRMgr_PowerState_t* powerState) {
           *powerState = PWRMGR_POWERSTATE_OFF; // by default over boot up, return PowerState OFF
           return PWRMGR_SUCCESS;
        }));

        EXPECT_CALL(*p_powerManagerHalMock, PLAT_API_SetPowerState(::testing::_))
        .WillRepeatedly(::testing::Invoke(
        [](PWRMgr_PowerState_t powerState) {
           // All tests are run without settings file
           // so default expected power state is ON
           return PWRMGR_SUCCESS;
        }));

        EXPECT_CALL(*p_mfrMock, mfrGetTemperature(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke(
            [&](mfrTemperatureState_t* curState, int* curTemperature, int* wifiTemperature) {
                *curTemperature  = 90; // safe temperature
                *curState        = (mfrTemperatureState_t)0;
                *wifiTemperature = 25;
                return mfrERR_NONE;
        }));

         /* Activate plugin in constructor */
         status = ActivateService("org.rdk.PowerManager");
         EXPECT_EQ(Core::ERROR_NONE, status);

         ON_CALL(*p_hostImplMock, Register(::testing::Matcher<device::Host::IDisplayEvents*>(::testing::_)))
             .WillByDefault(::testing::Invoke(
                 [&](device::Host::IDisplayEvents* listener) {
                         de_listener = listener;
                     return dsERR_NONE;
         }));

         ON_CALL(*p_hostImplMock, Register(::testing::Matcher<device::Host::IAudioOutputPortEvents*>(::testing::_)))
             .WillByDefault(::testing::Invoke(
                 [&](device::Host::IAudioOutputPortEvents* listener) {
                         aope_listener = listener;
                     return dsERR_NONE;
         }));

         ON_CALL(*p_hostImplMock, Register(::testing::Matcher<device::Host::IDisplayDeviceEvents*>(::testing::_)))
             .WillByDefault(::testing::Invoke(
                 [&](device::Host::IDisplayDeviceEvents* listener) {
                         dde_listener = listener;
                     return dsERR_NONE;
         }));

         ON_CALL(*p_hostImplMock, Register(::testing::Matcher<device::Host::IHdmiInEvents*>(::testing::_)))
             .WillByDefault(::testing::Invoke(
                 [&](device::Host::IHdmiInEvents* listener) {
                         hie_listener = listener;
                     return dsERR_NONE;
         }));

         ON_CALL(*p_hostImplMock, Register(::testing::Matcher<device::Host::IVideoDeviceEvents*>(::testing::_)))
             .WillByDefault(::testing::Invoke(
                 [&](device::Host::IVideoDeviceEvents* listener) {
                         vde_listener = listener;
                     return dsERR_NONE;
         }));

         ON_CALL(*p_hostImplMock, Register(::testing::Matcher<device::Host::IVideoOutputPortEvents*>(::testing::_)))
             .WillByDefault(::testing::Invoke(
                 [&](device::Host::IVideoOutputPortEvents* listener) {
                         vope_listener = listener;
                     return dsERR_NONE;
         }));

         status = ActivateService("org.rdk.DisplaySettings");
         EXPECT_EQ(Core::ERROR_NONE, status);
 
}
 
/**
* @brief Destructor for DisplaySettings L2 test class
*/
DisplaySettings_L2test::~DisplaySettings_L2test()
{
    printf("DISPLAYSETTINGS Destructor\n");
    uint32_t status = Core::ERROR_GENERAL;

    ON_CALL(*p_hostImplMock, UnRegister(::testing::Matcher<device::Host::IDisplayEvents*>(::testing::_)))
        .WillByDefault(::testing::Invoke(
            [&](device::Host::IDisplayEvents* listener) {
                    de_listener = nullptr;
                return dsERR_NONE;
    }));

    ON_CALL(*p_hostImplMock, UnRegister(::testing::Matcher<device::Host::IAudioOutputPortEvents*>(::testing::_)))
        .WillByDefault(::testing::Invoke(
            [&](device::Host::IAudioOutputPortEvents* listener) {
                    aope_listener = nullptr;
                return dsERR_NONE;
    }));

    ON_CALL(*p_hostImplMock, UnRegister(::testing::Matcher<device::Host::IDisplayDeviceEvents*>(::testing::_)))
        .WillByDefault(::testing::Invoke(
            [&](device::Host::IDisplayDeviceEvents* listener) {
                    dde_listener = nullptr;
                return dsERR_NONE;
    }));

    ON_CALL(*p_hostImplMock, UnRegister(::testing::Matcher<device::Host::IHdmiInEvents*>(::testing::_)))
        .WillByDefault(::testing::Invoke(
            [&](device::Host::IHdmiInEvents* listener) {
                    hie_listener = nullptr;
                return dsERR_NONE;
    }));

    ON_CALL(*p_hostImplMock, UnRegister(::testing::Matcher<device::Host::IVideoDeviceEvents*>(::testing::_)))
        .WillByDefault(::testing::Invoke(
            [&](device::Host::IVideoDeviceEvents* listener) {
                    vde_listener = nullptr;
                return dsERR_NONE;
    }));

    ON_CALL(*p_hostImplMock, UnRegister(::testing::Matcher<device::Host::IVideoOutputPortEvents*>(::testing::_)))
        .WillByDefault(::testing::Invoke(
            [&](device::Host::IVideoOutputPortEvents* listener) {
                    vope_listener = nullptr;
                return dsERR_NONE;
    }));

    status = DeactivateService("org.rdk.DisplaySettings");
    EXPECT_EQ(Core::ERROR_NONE, status);

    EXPECT_CALL(*p_powerManagerHalMock, PLAT_TERM())
        .WillOnce(::testing::Return(PWRMGR_SUCCESS));

    EXPECT_CALL(*p_powerManagerHalMock, PLAT_DS_TERM())
        .WillOnce(::testing::Return(DEEPSLEEPMGR_SUCCESS));

    status = DeactivateService("org.rdk.PowerManager");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_MethodTest)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(DISPLAYSETTINGS_CALLSIGN, DISPLAYSETTINGSL2TEST_CALLSIGN);
    JsonObject result, params;

    string videoPort(_T("HDMI0"));
    string audioPort(_T("HDMI0"));

    device::VideoOutputPort videoOutputPort;
    device::VideoDevice videoDevice;
    device::VideoResolution videoResolution;
    device::VideoOutputPortType videoOutputPortType;
    device::VideoDFC actualVideoDFC;
    device::AudioOutputPort audioOutputPort;
    string videoDFCName(_T("FULL"));
    string videoPortSupportedResolution(_T("1080p"));
    device::Display display;
    device::AspectRatio aspectRatio;

    ON_CALL(*p_hostImplMock, getDefaultVideoPortName())
        .WillByDefault(::testing::Return(videoPort));
    ON_CALL(*p_videoOutputPortConfigImplMock, getPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPort));
    ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
        .WillByDefault(::testing::Return(true));
    ON_CALL(*p_videoOutputPortMock, isEnabled())
        .WillByDefault(::testing::Return(true));

    /*********************DisplaySettings Calls - Start*********************************************/
    device::AudioOutputPort audioFormat;
    ON_CALL(*p_hostImplMock, getCurrentAudioFormat(testing::_))
    .WillByDefault(testing::Invoke(
        [](dsAudioFormat_t audioFormat) {
            audioFormat=dsAUDIO_FORMAT_NONE;
            return 0;
            }));

    // ON_CALL(*p_hostImplMock, getDefaultVideoPortName())
    //     .WillByDefault(::testing::Return(videoPort));

    ON_CALL(*p_videoOutputPortMock, getDefaultResolution())
        .WillByDefault(::testing::ReturnRef(videoResolution));

    ON_CALL(*p_hostImplMock, getVideoOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPort));

    // ON_CALL(*p_videoOutputPortConfigImplMock, getPort(::testing::_))
    //     .WillByDefault(::testing::ReturnRef(videoOutputPort));
    ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
        .WillByDefault(::testing::Return(true));

    ON_CALL(*p_videoOutputPortMock, getName())
        .WillByDefault(::testing::ReturnRef(videoPort));

    ON_CALL(*p_audioOutputPortMock, getName())
        .WillByDefault(::testing::ReturnRef(audioPort));

    ON_CALL(*p_videoOutputPortMock, getVideoEOTF())
        .WillByDefault(::testing::Return(1));

    ON_CALL(*p_hostImplMock, getVideoDevices())
        .WillByDefault(::testing::Return(device::List<device::VideoDevice>({ videoDevice })));

    ON_CALL(*p_videoDeviceMock, getHDRCapabilities(testing::_))
        .WillByDefault([](int* capabilities) {
            if (capabilities) {
                *capabilities = dsHDRSTANDARD_TechnicolorPrime;
            }
        });

    ON_CALL(*p_videoOutputPortMock, getColorDepthCapabilities(testing::_))
        .WillByDefault(testing::Invoke(
            [&](unsigned int* capabilities) {
            *capabilities = dsDISPLAY_COLORDEPTH_8BIT;
        }));

    ON_CALL(*p_videoOutputPortMock, setResolution(testing::_, testing::_, testing::_))
    .WillByDefault(testing::Invoke(
        [&](std::string resolution, bool persist, bool isIgnoreEdid) {
                        printf("Inside setResolution Mock\n");
                        EXPECT_EQ(resolution, "1080p60");
                        EXPECT_EQ(persist, true);

        std::cout << "Resolution: " << resolution 
                << ", Persist: " << persist 
                << ", Ignore EDID: " << isIgnoreEdid << std::endl;
    }));

    ON_CALL(*p_videoOutputPortMock, getCurrentOutputSettings(testing::_, testing::_, testing::_, testing::_, testing::_))
        .WillByDefault([](int& videoEOTF, int& matrixCoefficients, int& colorSpace, 
                        int& colorDepth, int& quantizationRange) {
            videoEOTF = 1;  // example values
            matrixCoefficients = 0;
            colorSpace = 3;
            colorDepth = 10;
            quantizationRange = 4;

            return true;
        });

    ON_CALL(*p_videoOutputPortConfigImplMock, getPortType(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPortType));
    ON_CALL(*p_videoOutputPortMock, getType())
        .WillByDefault(::testing::ReturnRef(videoOutputPortType));
    ON_CALL(*p_videoOutputPortTypeMock, getId())
        .WillByDefault(::testing::Return(0));
    // device::VideoResolution videoResolution;
    ON_CALL(*p_videoOutputPortTypeMock, getSupportedResolutions())
        .WillByDefault(::testing::Return(device::List<device::VideoResolution>({ videoResolution })));
    ON_CALL(*p_videoResolutionMock, getName())
        .WillByDefault(::testing::ReturnRef(videoPortSupportedResolution));

    ON_CALL(*p_videoOutputPortMock, setForceHDRMode(::testing::_))
        .WillByDefault(::testing::Return(true));

    ON_CALL(*p_videoOutputPortMock, getResolution())
    .WillByDefault([]() -> const device::VideoResolution& {
        static device::VideoResolution dynamicResolution;
        return dynamicResolution;
    });

    ON_CALL(*p_videoDeviceMock, getDFC())
        .WillByDefault(::testing::Invoke([&]() -> device::VideoDFC& {
            return actualVideoDFC;
        }));

    ON_CALL(*p_videoDFCMock, getName())
    .WillByDefault(::testing::ReturnRef(videoDFCName));

    ON_CALL(*p_videoOutputPortMock, getTVHDRCapabilities(testing::_))
        .WillByDefault(testing::Invoke(
            [&](int* capabilities) {
            *capabilities = dsHDRSTANDARD_HLG | dsHDRSTANDARD_HDR10;
        }));

    ON_CALL(*p_hostImplMock, getAudioOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(audioOutputPort));

    ON_CALL(*p_hostImplMock, getAudioOutputPorts())
        .WillByDefault(::testing::Return(device::List<device::AudioOutputPort>({ audioOutputPort })));
    ON_CALL(*p_videoOutputPortMock, getDisplay())
    	.WillByDefault(::testing::ReturnRef(display));

    ON_CALL(*p_displayMock, getAspectRatio())
    	.WillByDefault(::testing::ReturnRef(aspectRatio));
    /*********************DisplaySettings Calls - End*********************************************/
    /**************getCurrentResolution********************/

    {
        JsonObject result, params;
        uint32_t status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getCurrentResolution", params, result);
    }
    /************************getAudioFormat***************************/
    uint32_t status = Core::ERROR_NONE;

    {
        JsonObject result, params;
        uint32_t status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getAudioFormat", params, result);
    }
    /************************getVideoFormat***************************/

    {
        JsonObject result, params;
        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getVideoFormat", params, result);
    }

    /****************getColorDepthCapabilities***************/
    
    {
        JsonObject result, params;
        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getColorDepthCapabilities", params, result);
    }

    /****************setCurrentResolution***************/

    {
        JsonObject params2, result;
        params2["videoDisplay"] = "HDMI0";
        params2["resolution"] = "1080p60";
        params2["persist"] = true;
        params2["ignoreEdid"] = false;

        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "setCurrentResolution", params2, result);
    }


    /****************getCurrentOutputSettings***************/

    {
        JsonObject result, params;
        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getCurrentOutputSettings", params, result);
    }

    /**************getSupportedResolutions********************/

    // Test the disconnected-display 
    ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
        .WillByDefault(::testing::Return(false));
    {
        JsonObject result, params;
        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getSupportedResolutions", params, result);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result["success"].Boolean());
        ASSERT_TRUE(result.HasLabel("supportedResolutions"));
        auto arr = result["supportedResolutions"].Array();
        EXPECT_EQ(arr.Length(), 0u);
    }

    // Restore display connected for the remaining getSupportedResolutions tests
    ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
        .WillByDefault(::testing::Return(true));

    // Test: default port, display connected
    {
        JsonObject result, params;
        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getSupportedResolutions", params, result);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result["success"].Boolean());
        ASSERT_TRUE(result.HasLabel("supportedResolutions"));
    }

    // Test with specific videoDisplay parameter
    {
        JsonObject result, params;
        params["videoDisplay"] = "HDMI0";
        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getSupportedResolutions", params, result);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result["success"].Boolean());
        ASSERT_TRUE(result.HasLabel("supportedResolutions"));
    }

    /****************setForceHDRMode***************/

    {
        JsonObject result, params;
        params["hdr_mode"] = "DV";
        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "setForceHDRMode", params, result);
    }

    /**************getCurrentResolution********************/

    {
        JsonObject result, params;
        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getCurrentResolution", params, result);
    }

   /************************getZoomSetting***************************/
 
    {
        JsonObject result, params;
        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getZoomSetting", params, result);
    }

     /************************getTVHDRCapabilities***************************/

    {
        JsonObject result, params;
        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getTVHDRCapabilities", params, result);
    }

    /************************getSinkAtmosCapability***************************/
    {
        JsonObject result, params;
        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getSinkAtmosCapability", params, result);
    }

    /****************setPrimaryLanguage***************/
    {
        JsonObject params2, result;
        params2["lang"] = "US-en";
        params2["audioPort"] = "HDMI0";

        status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "setPrimaryLanguage", params2, result);
    }

    /*******************setAudioDucking - success ******************/
    {
        TEST_LOG("Testing setAudioDucking success\n");
        JsonObject result, params;
        params["audioPort"] = "HDMI0";
        params["mode"] = "mute";
        params["mute"] = true;

        EXPECT_CALL(*p_audioOutputPortMock, setAudioDucking(::testing::_, ::testing::_, ::testing::_))
            .Times(1);

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setAudioDucking", params, result);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_TRUE(result["success"].Boolean());
    }

    /*******************setAudioDucking - failure *******************/
    {
        TEST_LOG("Testing setAudioDucking invalid mode\n");
        JsonObject result, params;
        params["audioPort"] = "HDMI0";
        params["mode"] = "invalid_mode";

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setAudioDucking", params, result);
        EXPECT_NE(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_FALSE(result["success"].Boolean());
    }

    /******************setEnableVideoPort - success ******************/
    {
        TEST_LOG("Testing setEnableVideoPort success\n");
        JsonObject result, params;
        params["videoDisplay"] = "HDMI0";
        params["enable"] = true;

        ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
            .WillByDefault(::testing::Return(true));
        EXPECT_CALL(*p_videoOutputPortMock, enable())
            .Times(1);

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setEnableVideoPort", params, result);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_TRUE(result["success"].Boolean());
    }

    /******************setEnableVideoPort - failure ******************/
    {
        TEST_LOG("Testing setEnableVideoPort disconnected display\n");
        JsonObject result, params;
        params["videoDisplay"] = "HDMI0";
        params["enable"] = false;

        ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
            .WillByDefault(::testing::Return(false));

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setEnableVideoPort", params, result);
        EXPECT_NE(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_FALSE(result["success"].Boolean());
    }

    /******************getEnableVideoPort - success ******************/
    {
        TEST_LOG("Testing getEnableVideoPort success\n");
        JsonObject result, params;
        params["videoDisplay"] = "HDMI0";

        ON_CALL(*p_videoOutputPortMock, isEnabled())
            .WillByDefault(::testing::Return(true));

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "getEnableVideoPort", params, result);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_TRUE(result["success"].Boolean());
        EXPECT_TRUE(result.HasLabel("enable"));
        EXPECT_TRUE(result["enable"].Boolean());
    }

    /******************getEnableVideoPort - failure ******************/
    {
        TEST_LOG("Testing getEnableVideoPort missing videoDisplay\n");
        JsonObject result, params; // no videoDisplay

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "getEnableVideoPort", params, result);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_FALSE(result["success"].Boolean());
    }

    /******************getSupportedVideoCodingFormats - success ******************/
    {
        TEST_LOG("Testing getSupportedVideoCodingFormats success\n");
        JsonObject result, params;

        ON_CALL(*p_videoDeviceMock, getSupportedVideoCodingFormats())
            .WillByDefault(::testing::Return(
                dsVIDEO_CODEC_MPEGHPART2 | dsVIDEO_CODEC_MPEG4PART10 | dsVIDEO_CODEC_MPEG2));

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "getSupportedVideoCodingFormats", params, result);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_TRUE(result["success"].Boolean());
        EXPECT_TRUE(result.HasLabel("supportedFormats"));
    }

    /******************getSupportedVideoCodingFormats - failure ******************/
    {
        TEST_LOG("Testing getSupportedVideoCodingFormats no video devices\n");
        JsonObject result, params;

        ON_CALL(*p_hostImplMock, getVideoDevices())
            .WillByDefault(::testing::Return(device::List<device::VideoDevice>()));

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "getSupportedVideoCodingFormats", params, result);
        EXPECT_NE(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_FALSE(result["success"].Boolean());

        ON_CALL(*p_hostImplMock, getVideoDevices())
            .WillByDefault(::testing::Return(device::List<device::VideoDevice>({ videoDevice })));
    }

    /******************getVideoCodecInfo - success ******************/
    {
        TEST_LOG("Testing getVideoCodecInfo success\n");
        JsonObject result, params;
        params["codec"] = "HEVC";

        dsVideoCodecInfo_t codecInfo {};
        codecInfo.num_entries = 1;
        codecInfo.entries[0].profile = dsVIDEO_CODEC_HEVC_PROFILE_MAIN;
        codecInfo.entries[0].level = 120;

        ON_CALL(*p_videoDeviceMock, getVideoCodecInfo(::testing::_))
            .WillByDefault(::testing::Return(codecInfo));

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "getVideoCodecInfo", params, result);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_TRUE(result["success"].Boolean());
        EXPECT_TRUE(result.HasLabel("entries"));
    }

    /******************getVideoCodecInfo - failure ******************/
    {
        TEST_LOG("Testing getVideoCodecInfo unsupported codec\n");
        JsonObject result, params;
        params["codec"] = "VP9";

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "getVideoCodecInfo", params, result);
        EXPECT_NE(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_FALSE(result["success"].Boolean());
    }

    /******************getAudioEncoding - success ******************/
    {
        TEST_LOG("Testing getAudioEncoding success\n");
        JsonObject result, params;
        params["audioPort"] = "HDMI0";

        device::AudioEncoding encPCM(dsAUDIO_ENC_PCM);
        ON_CALL(*p_audioOutputPortMock, getEncoding())
            .WillByDefault(::testing::ReturnRef(encPCM));

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "getAudioEncoding", params, result);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_TRUE(result["success"].Boolean());
        EXPECT_TRUE(result.HasLabel("encoding"));
        EXPECT_TRUE(result.HasLabel("encodingId"));
    }

    /******************getAudioEncoding - failure ******************/
    {
        TEST_LOG("Testing getAudioEncoding invalid audioPort\n");
        JsonObject result, params;
        params["audioPort"] = "INVALID_PORT";

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "getAudioEncoding", params, result);
        EXPECT_NE(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_FALSE(result["success"].Boolean());
    }

    /******************setAudioEncoding - success ******************/
    {
        TEST_LOG("Testing setAudioEncoding success\n");
        JsonObject result, params;
        params["audioPort"] = "HDMI0";
        params["encoding"] = "AC3";

        EXPECT_CALL(*p_audioOutputPortMock, setEncoding(::testing::A<const std::string&>()))
            .Times(1);

        device::AudioEncoding encAC3(dsAUDIO_ENC_AC3);
        ON_CALL(*p_audioOutputPortMock, getEncoding())
            .WillByDefault(::testing::ReturnRef(encAC3));

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setAudioEncoding", params, result);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_TRUE(result["success"].Boolean());
        EXPECT_TRUE(result.HasLabel("encoding"));
    }

    /******************setAudioEncoding - failure ******************/
    {
        TEST_LOG("Testing setAudioEncoding missing encoding\n");
        JsonObject result, params;
        params["audioPort"] = "SPDIF0"; // encoding omitted

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setAudioEncoding", params, result);
        EXPECT_NE(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_FALSE(result["success"].Boolean());
    }

    /******************getDisplayAspectRatio - success ******************/
    {
        TEST_LOG("Testing getDisplayAspectRatio success\n");
        JsonObject result, params;
        params["videoDisplay"] = "HDMI0";

        ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
            .WillByDefault(::testing::Return(true));

        uint32_t status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "getDisplayAspectRatio", params, result);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
        EXPECT_TRUE(result["success"].Boolean());
        EXPECT_TRUE(result.HasLabel("aspectRatio"));
        EXPECT_TRUE(result.HasLabel("aspectRatioValue"));
    }

    dsDisplayEvent_t displayEvent = dsDISPLAY_RXSENSE_ON;
    de_listener->OnDisplayRxSense(displayEvent);
    /* IAudioOutputPortEvents*/
    aope_listener->OnAudioOutHotPlug(dsAudioPortType_t::dsAUDIOPORT_TYPE_HDMI, 1, true);
    aope_listener->OnAudioFormatUpdate(dsAudioFormat_t::dsAUDIO_FORMAT_DOLBY_AC3);
    aope_listener->OnDolbyAtmosCapabilitiesChanged(dsATMOSCapability_t::dsAUDIO_ATMOS_DDPLUSSTREAM, true);
    aope_listener->OnAudioPortStateChanged(dsAudioPortState_t::dsAUDIOPORT_STATE_INITIALIZED);
    aope_listener->OnAssociatedAudioMixingChanged(true);
    aope_listener->OnAudioFaderControlChanged(10);
    aope_listener->OnAudioPrimaryLanguageChanged("US-en");
    aope_listener->OnAudioSecondaryLanguageChanged("US-en");

    /* IDisplayDeviceEvents */
    dde_listener->OnDisplayHDMIHotPlug(dsDisplayEvent_t::dsDISPLAY_HDCPPROTOCOL_CHANGE);

    /* IHdmiInEvents*/
    hie_listener->OnHdmiInEventHotPlug(dsHdmiInPort_t::dsHDMI_IN_PORT_0, true);

    /* IVideoDeviceEvents */
    vde_listener->OnZoomSettingsChanged(dsVideoZoom_t::dsVIDEO_ZOOM_FULL);

    /* IVideoOutputPortEvents */
    vope_listener->OnResolutionPreChange(1,1);
    vope_listener->OnResolutionPostChange(1,1);
    vope_listener->OnVideoFormatUpdate(dsHDRStandard_t::dsHDRSTANDARD_HDR10);

}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_setAudioDucking_Attenuate_Success)
{
    JsonObject result, params;
    uint32_t status = Core::ERROR_NONE;

    device::AudioOutputPort audioOutputPort;
    ON_CALL(*p_hostImplMock, getAudioOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(audioOutputPort));

    params["audioPort"] = "HDMI0";
    params["mode"] = "attenuate";
    params["enable"] = true;
    params["relative"] = true;
    params["volume"] = 0.37; // expected level = 37

    EXPECT_CALL(*p_audioOutputPortMock,
                setAudioDucking(dsAUDIO_DUCKINGACTION_START,
                                dsAUDIO_DUCKINGTYPE_RELATIVE,
                                ::testing::_))
        .Times(1);

    status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setAudioDucking", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_setAudioDucking_Attenuate_InvalidVolume)
{
    JsonObject result, params;
    uint32_t status = Core::ERROR_NONE;

    device::AudioOutputPort audioOutputPort;
    ON_CALL(*p_hostImplMock, getAudioOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(audioOutputPort));

    params["audioPort"] = "HDMI0";
    params["mode"] = "attenuate";
    params["enable"] = true;
    params["relative"] = false;
    params["volume"] = 1.5; // invalid (>1.0)

    EXPECT_CALL(*p_audioOutputPortMock,
                setAudioDucking(dsAUDIO_DUCKINGACTION_START,
                                dsAUDIO_DUCKINGTYPE_ABSOLUTE,
                                static_cast<unsigned char>(100)))
        .Times(1);

    status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setAudioDucking", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_setAudioDucking_Raw_Success_StartRelative)
{
    JsonObject result, params;
    uint32_t status = Core::ERROR_NONE;

    device::AudioOutputPort audioOutputPort;
    ON_CALL(*p_hostImplMock, getAudioOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(audioOutputPort));

    params["audioPort"] = "HDMI0";
    params["mode"] = "raw";
    params["action"] = "start";
    params["duckingType"] = "relative";
    params["level"] = 56.0; // rounded to 56 in plugin

    EXPECT_CALL(*p_audioOutputPortMock,
                setAudioDucking(dsAUDIO_DUCKINGACTION_START,
                                dsAUDIO_DUCKINGTYPE_RELATIVE,
                                ::testing::_))
        .Times(1);

    status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setAudioDucking", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_setAudioDucking_Raw_Success_StopAbsolute)
{
    JsonObject result, params;
    uint32_t status = Core::ERROR_NONE;

    device::AudioOutputPort audioOutputPort;
    ON_CALL(*p_hostImplMock, getAudioOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(audioOutputPort));

    params["audioPort"] = "HDMI0";
    params["mode"] = "raw";
    params["action"] = "stop";
    params["duckingType"] = "absolute";
    params["level"] = 100.0;

    EXPECT_CALL(*p_audioOutputPortMock,
                setAudioDucking(dsAUDIO_DUCKINGACTION_STOP,
                                dsAUDIO_DUCKINGTYPE_ABSOLUTE,
                                static_cast<unsigned char>(100)))
        .Times(1);

    status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setAudioDucking", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_setAudioDucking_Raw_InvalidLevel)
{
    JsonObject result, params;
    uint32_t status = Core::ERROR_NONE;

    device::AudioOutputPort audioOutputPort;
    ON_CALL(*p_hostImplMock, getAudioOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(audioOutputPort));

    params["audioPort"] = "HDMI0";
    params["mode"] = "raw";
    params["action"] = "start";
    params["duckingType"] = "absolute";
    params["level"] = 101; // invalid

    EXPECT_CALL(*p_audioOutputPortMock, setAudioDucking(::testing::_, ::testing::_, ::testing::_))
        .Times(0);

    status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setAudioDucking", params, result);
    EXPECT_NE(Core::ERROR_NONE, status);
    EXPECT_FALSE(result.HasLabel("success"));
}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_setAudioDucking_Raw_InvalidAction)
{
    JsonObject result, params;
    uint32_t status = Core::ERROR_NONE;

    device::AudioOutputPort audioOutputPort;
    ON_CALL(*p_hostImplMock, getAudioOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(audioOutputPort));

    params["audioPort"] = "HDMI0";
    params["mode"] = "raw";
    params["action"] = "pause"; // invalid
    params["duckingType"] = "relative";
    params["level"] = 40;

    EXPECT_CALL(*p_audioOutputPortMock, setAudioDucking(::testing::_, ::testing::_, ::testing::_))
        .Times(0);

    status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setAudioDucking", params, result);
    EXPECT_NE(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_setAudioDucking_Raw_InvalidDuckingType)
{
    JsonObject result, params;
    uint32_t status = Core::ERROR_NONE;

    device::AudioOutputPort audioOutputPort;
    ON_CALL(*p_hostImplMock, getAudioOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(audioOutputPort));

    params["audioPort"] = "HDMI0";
    params["mode"] = "raw";
    params["action"] = "start";
    params["duckingType"] = "bad_type"; // invalid
    params["level"] = 40;

    EXPECT_CALL(*p_audioOutputPortMock, setAudioDucking(::testing::_, ::testing::_, ::testing::_))
        .Times(0);

    status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setAudioDucking", params, result);
    EXPECT_NE(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_setAudioDucking_Catch_DeviceException)
{
    JsonObject result, params;
    uint32_t status = Core::ERROR_NONE;

    device::AudioOutputPort audioOutputPort;
    ON_CALL(*p_hostImplMock, getAudioOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(audioOutputPort));

    params["audioPort"] = "HDMI0";
    params["mode"] = "mute";
    params["mute"] = true;

    EXPECT_CALL(*p_audioOutputPortMock, setAudioDucking(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](dsAudioDuckingAction_t, dsAudioDuckingType_t, const unsigned char) {
                throw device::Exception("mock setAudioDucking failure");
            }));

    status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setAudioDucking", params, result);
    EXPECT_NE(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_setEnableVideoPort_Disable_Success)
{
    TEST_LOG("Testing setEnableVideoPort disable success\n");

    JsonObject result, params;
    uint32_t status = Core::ERROR_NONE;

    device::VideoOutputPort videoOutputPort;
    ON_CALL(*p_hostImplMock, getVideoOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPort));

    ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
        .WillByDefault(::testing::Return(true));

    params["videoDisplay"] = "HDMI0";
    params["enable"] = false;

    EXPECT_CALL(*p_videoOutputPortMock, disable())
        .Times(1);

    status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setEnableVideoPort", params, result);

    // Plugin returns ERROR_NONE and success:true for disable success
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_setEnableVideoPort_Disable_DeviceException)
{
    TEST_LOG("Testing setEnableVideoPort disable throws device::Exception\n");

    JsonObject result, params;
    uint32_t status = Core::ERROR_NONE;

    device::VideoOutputPort videoOutputPort;
    ON_CALL(*p_hostImplMock, getVideoOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPort));

    ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
        .WillByDefault(::testing::Return(true));

    params["videoDisplay"] = "HDMI0";
    params["enable"] = false;

    EXPECT_CALL(*p_videoOutputPortMock, disable())
        .WillOnce(::testing::Invoke([]() {
            throw device::Exception("mock disable exception");
        }));

    status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "setEnableVideoPort", params, result);

    EXPECT_NE(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_getVideoCodecInfo_H264_MapsToMPEG4Part10)
{
    JsonObject result, params;
    uint32_t status = Core::ERROR_NONE;
    device::VideoDevice videoDevice;
    ON_CALL(*p_hostImplMock, getVideoDevices())
        .WillByDefault(::testing::Return(device::List<device::VideoDevice>({ videoDevice })));

    dsVideoCodecInfo_t codecInfo {};
    codecInfo.num_entries = 1;
    codecInfo.entries[0].profile = static_cast<decltype(codecInfo.entries[0].profile)>(1);
    codecInfo.entries[0].level = 42;

    EXPECT_CALL(*p_videoDeviceMock, getVideoCodecInfo(dsVIDEO_CODEC_MPEG4PART10))
        .WillOnce(::testing::Return(codecInfo));

    params["codec"] = "H264";

    status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "getVideoCodecInfo", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_TRUE(result.HasLabel("entries"));
}

TEST_F(DisplaySettings_L2test, DisplaySettings_L2_getVideoCodecInfo_MPEG2_MapsToMPEG2Enum)
{
    JsonObject result, params;
    uint32_t status = Core::ERROR_NONE;
    device::VideoDevice videoDevice;
    ON_CALL(*p_hostImplMock, getVideoDevices())
        .WillByDefault(::testing::Return(device::List<device::VideoDevice>({ videoDevice })));

    dsVideoCodecInfo_t codecInfo {};
    codecInfo.num_entries = 1;
    codecInfo.entries[0].profile = static_cast<decltype(codecInfo.entries[0].profile)>(2);
    codecInfo.entries[0].level = 30;

    EXPECT_CALL(*p_videoDeviceMock, getVideoCodecInfo(dsVIDEO_CODEC_MPEG2))
        .WillOnce(::testing::Return(codecInfo));

    params["codec"] = "MPEG2";

    status = InvokeServiceMethod(DISPLAYSETTINGS_CALLSIGN, "getVideoCodecInfo", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_TRUE(result.HasLabel("entries"));
}

/**
 * @brief Verifies getSupportedResolutions returns a non-empty list when an external display is connected.
 *
 * Scenario: External display connected returns EDID resolutions
 */
TEST_F(DisplaySettings_L2test, getSupportedResolutions_ExternalDisplayConnected)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(DISPLAYSETTINGS_CALLSIGN, DISPLAYSETTINGSL2TEST_CALLSIGN);

    device::VideoOutputPort videoOutputPort;
    device::VideoOutputPortType videoOutputPortType;
    device::VideoResolution videoResolution;
    string videoPort(_T("HDMI0"));
    string supportedRes(_T("1080p"));

    ON_CALL(*p_hostImplMock, getVideoOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPort));
    ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
        .WillByDefault(::testing::Return(true));
    ON_CALL(*p_videoOutputPortMock, getType())
        .WillByDefault(::testing::ReturnRef(videoOutputPortType));
    ON_CALL(*p_videoOutputPortTypeMock, getId())
        .WillByDefault(::testing::Return(0));
    ON_CALL(*p_videoOutputPortConfigImplMock, getPortType(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPortType));
    ON_CALL(*p_videoOutputPortTypeMock, getSupportedResolutions())
        .WillByDefault(::testing::Return(device::List<device::VideoResolution>({ videoResolution })));
    ON_CALL(*p_videoResolutionMock, getName())
        .WillByDefault(::testing::ReturnRef(supportedRes));

    JsonObject result, params;
    uint32_t status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getSupportedResolutions", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_FALSE(result["supportedResolutions"].Array().IsNull());
    EXPECT_GT(result["supportedResolutions"].Array().Length(), 0u);
}

/**
 * @brief Verifies getSupportedResolutions returns an empty list when no external display is connected.
 *
 * Scenario: No display connected on external port
 */
TEST_F(DisplaySettings_L2test, getSupportedResolutions_NoDisplayConnected)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(DISPLAYSETTINGS_CALLSIGN, DISPLAYSETTINGSL2TEST_CALLSIGN);

    device::VideoOutputPort videoOutputPort;

    ON_CALL(*p_hostImplMock, getVideoOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPort));
    ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
        .WillByDefault(::testing::Return(false));

    JsonObject result, params;
    uint32_t status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getSupportedResolutions", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_EQ(result["supportedResolutions"].Array().Length(), 0u);
}

/**
 * @brief Verifies getSupportedResolutions defaults to HDMI0 when videoDisplay parameter is omitted.
 *
 * Scenario: No videoDisplay parameter supplied
 */
TEST_F(DisplaySettings_L2test, getSupportedResolutions_DefaultsToHDMI0)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(DISPLAYSETTINGS_CALLSIGN, DISPLAYSETTINGSL2TEST_CALLSIGN);

    device::VideoOutputPort videoOutputPort;
    device::VideoOutputPortType videoOutputPortType;
    device::VideoResolution videoResolution;
    string supportedRes(_T("720p"));

    ON_CALL(*p_hostImplMock, getDefaultVideoPortName())
        .WillByDefault(::testing::Return(std::string("HDMI0")));
    EXPECT_CALL(*p_hostImplMock, getVideoOutputPort(::testing::Eq(std::string("HDMI0"))))
        .WillOnce(::testing::ReturnRef(videoOutputPort));
    ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
        .WillByDefault(::testing::Return(true));
    ON_CALL(*p_videoOutputPortMock, getType())
        .WillByDefault(::testing::ReturnRef(videoOutputPortType));
    ON_CALL(*p_videoOutputPortTypeMock, getId())
        .WillByDefault(::testing::Return(0));
    ON_CALL(*p_videoOutputPortConfigImplMock, getPortType(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPortType));
    ON_CALL(*p_videoOutputPortTypeMock, getSupportedResolutions())
        .WillByDefault(::testing::Return(device::List<device::VideoResolution>({ videoResolution })));
    ON_CALL(*p_videoResolutionMock, getName())
        .WillByDefault(::testing::ReturnRef(supportedRes));

    /* Call without videoDisplay param to verify default is HDMI0 */
    JsonObject result, params;
    uint32_t status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getSupportedResolutions", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/**
 * @brief Verifies getSupportedResolutions returns the platform capability list for a built-in display port.
 *
 * Scenario: Built-in display port returns platform capability list
 */
TEST_F(DisplaySettings_L2test, getSupportedResolutions_BuiltInDisplay)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(DISPLAYSETTINGS_CALLSIGN, DISPLAYSETTINGSL2TEST_CALLSIGN);

    device::VideoOutputPort videoOutputPort;
    device::VideoOutputPortType videoOutputPortType;
    device::VideoResolution videoResolution;
    string builtInPort(_T("COMPONENT0"));
    string supportedRes(_T("1080i"));

    ON_CALL(*p_hostImplMock, getVideoOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPort));
    /* Built-in display: isDisplayConnected() returns true regardless of physical state */
    ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
        .WillByDefault(::testing::Return(true));
    ON_CALL(*p_videoOutputPortMock, getType())
        .WillByDefault(::testing::ReturnRef(videoOutputPortType));
    ON_CALL(*p_videoOutputPortTypeMock, getId())
        .WillByDefault(::testing::Return(1));
    ON_CALL(*p_videoOutputPortConfigImplMock, getPortType(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPortType));
    ON_CALL(*p_videoOutputPortTypeMock, getSupportedResolutions())
        .WillByDefault(::testing::Return(device::List<device::VideoResolution>({ videoResolution })));
    ON_CALL(*p_videoResolutionMock, getName())
        .WillByDefault(::testing::ReturnRef(supportedRes));

    JsonObject result, params;
    params["videoDisplay"] = builtInPort;
    uint32_t status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getSupportedResolutions", params, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_GT(result["supportedResolutions"].Array().Length(), 0u);
}

/**
 * @brief Verifies getSupportedResolutions reflects updated capabilities after a display reconnect event.
 *
 * Scenario: Different display connected after initial query
 */
TEST_F(DisplaySettings_L2test, getSupportedResolutions_CacheRefreshOnReconnect)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(DISPLAYSETTINGS_CALLSIGN, DISPLAYSETTINGSL2TEST_CALLSIGN);

    device::VideoOutputPort videoOutputPort;
    device::VideoOutputPortType videoOutputPortType;
    device::VideoResolution videoResolution;
    string firstRes(_T("720p"));
    string secondRes(_T("1080p60"));

    ON_CALL(*p_hostImplMock, getVideoOutputPort(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPort));
    ON_CALL(*p_videoOutputPortMock, isDisplayConnected())
        .WillByDefault(::testing::Return(true));
    ON_CALL(*p_videoOutputPortMock, getType())
        .WillByDefault(::testing::ReturnRef(videoOutputPortType));
    ON_CALL(*p_videoOutputPortTypeMock, getId())
        .WillByDefault(::testing::Return(0));
    ON_CALL(*p_videoOutputPortConfigImplMock, getPortType(::testing::_))
        .WillByDefault(::testing::ReturnRef(videoOutputPortType));

    /* First call: mock returns "720p" */
    ON_CALL(*p_videoOutputPortTypeMock, getSupportedResolutions())
        .WillByDefault(::testing::Return(device::List<device::VideoResolution>({ videoResolution })));
    ON_CALL(*p_videoResolutionMock, getName())
        .WillByDefault(::testing::ReturnRef(firstRes));

    JsonObject result1, params1;
    uint32_t status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getSupportedResolutions", params1, result1);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result1["success"].Boolean());
    EXPECT_GT(result1["supportedResolutions"].Array().Length(), 0u);

    /* Simulate display reconnect hotplug event */
    dde_listener->OnDisplayHDMIHotPlug(dsDisplayEvent_t::dsDISPLAY_EVENT_CONNECTED);

    /* Second call: mock now returns "1080p60" to simulate new display EDID */
    ON_CALL(*p_videoResolutionMock, getName())
        .WillByDefault(::testing::ReturnRef(secondRes));

    JsonObject result2, params2;
    status = InvokeServiceMethod("org.rdk.DisplaySettings.1", "getSupportedResolutions", params2, result2);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result2["success"].Boolean());
    EXPECT_GT(result2["supportedResolutions"].Array().Length(), 0u);
}
