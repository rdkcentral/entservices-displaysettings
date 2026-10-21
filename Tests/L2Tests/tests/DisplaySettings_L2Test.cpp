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

#include "L2Tests.h"
#include "L2TestsMock.h"

using namespace WPEFramework;

namespace {
constexpr const char* kDisplaySettingsCallsign = "org.rdk.DisplaySettings.1";
}

class DisplaySettingsL2Test : public L2TestMocks {
protected:
    void ConfigureDeviceSettingsHal()
    {
        ON_CALL(*p_dsAudioHalMock, dsGetAudioPort(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([](dsAudioPortType_t, int, intptr_t* handle) {
                *handle = 2;
                return dsERR_NONE;
            }));
        ON_CALL(*p_dsVideoPortHalMock, dsGetVideoPort(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([](dsVideoPortType_t, int, intptr_t* handle) {
                *handle = 1;
                return dsERR_NONE;
            }));
        ON_CALL(*p_dsVideoDeviceHalMock, dsGetVideoDevice(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([](int, intptr_t* handle) {
                *handle = 3;
                return dsERR_NONE;
            }));
        ON_CALL(*p_dsDisplayHalMock, dsGetDisplay(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([](dsVideoPortType_t, int, intptr_t* handle) {
                *handle = 4;
                return dsERR_NONE;
            }));
        ON_CALL(*p_dsVideoPortHalMock, dsIsDisplayConnected(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([](intptr_t, bool* connected) {
                *connected = true;
                return dsERR_NONE;
            }));
        ON_CALL(*p_dsVideoPortHalMock, dsIsVideoPortEnabled(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([](intptr_t, bool* enabled) {
                *enabled = true;
                return dsERR_NONE;
            }));
        ON_CALL(*p_dsAudioHalMock, dsIsAudioPortEnabled(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([](intptr_t, bool* enabled) {
                *enabled = true;
                return dsERR_NONE;
            }));
        ON_CALL(*p_dsAudioHalMock, dsGetAudioLevel(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([](intptr_t, float* level) {
                *level = 50.0F;
                return dsERR_NONE;
            }));
        ON_CALL(*p_dsAudioHalMock, dsSetAudioLevel(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(dsERR_NONE));
        ON_CALL(*p_dsVideoPortHalMock, dsGetTVHDRCapabilities(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([](intptr_t, int* capabilities) {
                *capabilities = dsHDRSTANDARD_HLG | dsHDRSTANDARD_HDR10;
                return dsERR_NONE;
            }));
        ON_CALL(*p_dsVideoDeviceHalMock, dsGetSupportedVideoCodingFormats(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([](intptr_t, unsigned int* formats) {
                *formats = dsVIDEO_CODEC_MPEGHPART2 | dsVIDEO_CODEC_MPEG4PART10 | dsVIDEO_CODEC_MPEG2;
                return dsERR_NONE;
            }));
        ON_CALL(*p_dsVideoDeviceHalMock, dsGetVideoCodecInfo(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke([](intptr_t, dsVideoCodingFormat_t, dsVideoCodecInfo_t* info) {
                info->num_entries = 1;
                info->entries[0].profile = static_cast<decltype(info->entries[0].profile)>(1);
                info->entries[0].level = 42;
                return dsERR_NONE;
            }));
    }

    void ConfigurePowerManagerHal()
    {
        ON_CALL(*p_powerManagerHalMock, PLAT_DS_INIT())
            .WillByDefault(::testing::Return(DEEPSLEEPMGR_SUCCESS));
        ON_CALL(*p_powerManagerHalMock, PLAT_INIT())
            .WillByDefault(::testing::Return(PWRMGR_SUCCESS));
        ON_CALL(*p_powerManagerHalMock, PLAT_API_SetWakeupSrc(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(PWRMGR_SUCCESS));
        ON_CALL(*p_powerManagerHalMock, PLAT_API_GetPowerState(::testing::_))
            .WillByDefault(::testing::Invoke([](PWRMgr_PowerState_t* powerState) {
                *powerState = PWRMGR_POWERSTATE_ON;
                return PWRMGR_SUCCESS;
            }));
        ON_CALL(*p_powerManagerHalMock, PLAT_API_SetPowerState(::testing::_))
            .WillByDefault(::testing::Return(PWRMGR_SUCCESS));
        ON_CALL(*p_mfrMock, mfrGetTemperature(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [](mfrTemperatureState_t* state, int* temperature, int* wifiTemperature) {
                    *state = static_cast<mfrTemperatureState_t>(0);
                    *temperature = 90;
                    *wifiTemperature = 25;
                    return mfrERR_NONE;
                }));
    }

    void DeactivateAndWait(const char* callsign)
    {
        std::string state;
        const uint32_t stateStatus = GetPluginState(callsign, state);
        EXPECT_EQ(Core::ERROR_NONE, stateStatus);
        if (stateStatus != Core::ERROR_NONE || state == "deactivated") {
            return;
        }

        if (state == "activated" || state == "suspended") {
            (void)DeactivateService(callsign);
        }

        EXPECT_EQ(Core::ERROR_NONE,
            WaitForPluginState(callsign, "deactivated", 10000));
    }

    void ActivateAndWait(const char* callsign)
    {
        std::string state;
        if ((GetPluginState(callsign, state) == Core::ERROR_NONE) && (state == "activated")) {
            return;
        }

        (void)ActivateServiceWithRetry(callsign, 3, 500);
        EXPECT_EQ(Core::ERROR_NONE, WaitForPluginState(callsign, "activated", 10000));
    }

    DisplaySettingsL2Test()
    {
        ConfigureDeviceSettingsHal();
        ConfigurePowerManagerHal();
        ActivateAndWait("org.rdk.PowerManager");
        ActivateAndWait("org.rdk.DeviceSettings");
        ActivateAndWait("org.rdk.DisplaySettings");
    }

    ~DisplaySettingsL2Test() override
    {
        DeactivateAndWait("org.rdk.DisplaySettings");
        DeactivateAndWait("org.rdk.DeviceSettings");
        DeactivateAndWait("org.rdk.PowerManager");
    }
};

TEST_F(DisplaySettingsL2Test, SetAudioDuckingRejectsMissingMode)
{
    JsonObject parameters;
    JsonObject result;

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDucking", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetEnableVideoPortRejectsMissingDisplay)
{
    JsonObject parameters;
    JsonObject result;
    parameters["enable"] = true;

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setEnableVideoPort", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetAudioDuckingRejectsInvalidMode)
{
    JsonObject parameters;
    JsonObject result;
    parameters["mode"] = "invalid_mode";

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDucking", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetAudioDuckingRejectsMissingMute)
{
    JsonObject parameters;
    JsonObject result;
    parameters["mode"] = "mute";

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDucking", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetAudioDuckingMuteSucceeds)
{
    JsonObject parameters;
    JsonObject result;
    parameters["mode"] = "mute";
    parameters["mute"] = true;
    parameters["audioPort"] = "SPDIF0";

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDucking", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetAudioDuckingAttenuateSucceeds)
{
    JsonObject parameters;
    JsonObject result;
    parameters["mode"] = "attenuate";
    parameters["enable"] = true;
    parameters["relative"] = true;
    parameters["volume"] = 0.37;
    parameters["audioPort"] = "SPDIF0";

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDucking", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetAudioDuckingRawRelativeStartSucceeds)
{
    JsonObject parameters;
    JsonObject result;
    parameters["mode"] = "raw";
    parameters["action"] = "start";
    parameters["duckingType"] = "relative";
    parameters["level"] = 56;
    parameters["audioPort"] = "SPDIF0";

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDucking", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetAudioDuckingRawAbsoluteStopSucceeds)
{
    JsonObject parameters;
    JsonObject result;
    parameters["mode"] = "raw";
    parameters["action"] = "stop";
    parameters["duckingType"] = "absolute";
    parameters["level"] = 100;
    parameters["audioPort"] = "SPDIF0";

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDucking", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetAudioDuckingRejectsInvalidAttenuateVolume)
{
    JsonObject parameters;
    JsonObject result;
    parameters["mode"] = "attenuate";
    parameters["enable"] = true;
    parameters["relative"] = false;
    parameters["volume"] = 1.5;

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDucking", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetAudioDuckingRejectsInvalidRawLevel)
{
    JsonObject parameters;
    JsonObject result;
    parameters["mode"] = "raw";
    parameters["action"] = "start";
    parameters["duckingType"] = "absolute";
    parameters["level"] = 101;

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDucking", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetAudioDuckingRejectsInvalidRawAction)
{
    JsonObject parameters;
    JsonObject result;
    parameters["mode"] = "raw";
    parameters["action"] = "pause";
    parameters["duckingType"] = "relative";
    parameters["level"] = 40;

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDucking", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetAudioDuckingRejectsInvalidRawType)
{
    JsonObject parameters;
    JsonObject result;
    parameters["mode"] = "raw";
    parameters["action"] = "start";
    parameters["duckingType"] = "bad_type";
    parameters["level"] = 40;

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDucking", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetEnableVideoPortDisablesConnectedDisplay)
{
    JsonObject parameters;
    JsonObject result;
    parameters["videoDisplay"] = "Internal0";
    parameters["enable"] = false;

    EXPECT_CALL(*p_dsVideoPortHalMock, dsEnableVideoPort(::testing::_, false))
        .WillOnce(::testing::Return(dsERR_NONE));

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setEnableVideoPort", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetEnableVideoPortRejectsDisconnectedDisplay)
{
    JsonObject parameters;
    JsonObject result;
    parameters["videoDisplay"] = "Internal0";
    parameters["enable"] = false;

    ON_CALL(*p_dsVideoPortHalMock, dsIsDisplayConnected(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](intptr_t, bool* connected) {
            *connected = false;
            return dsERR_NONE;
        }));
    EXPECT_CALL(*p_dsVideoPortHalMock, dsEnableVideoPort(::testing::_, ::testing::_))
        .Times(0);

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setEnableVideoPort", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, SetEnableVideoPortReportsHalFailure)
{
    JsonObject parameters;
    JsonObject result;
    parameters["videoDisplay"] = "Internal0";
    parameters["enable"] = true;

    EXPECT_CALL(*p_dsVideoPortHalMock, dsEnableVideoPort(::testing::_, true))
        .WillOnce(::testing::Return(dsERR_GENERAL));

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "setEnableVideoPort", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, GetEnableVideoPortReturnsHalState)
{
    JsonObject parameters;
    JsonObject result;
    parameters["videoDisplay"] = "Internal0";

    ON_CALL(*p_dsVideoPortHalMock, dsIsVideoPortEnabled(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](intptr_t, bool* enabled) {
            *enabled = false;
            return dsERR_NONE;
        }));

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "getEnableVideoPort", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
    ASSERT_TRUE(result.HasLabel("enable"));
    EXPECT_FALSE(result["enable"].Boolean());
}

TEST_F(DisplaySettingsL2Test, GetSupportedVideoCodingFormatsReturnsHalCapabilities)
{
    JsonObject parameters;
    JsonObject result;

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "getSupportedVideoCodingFormats", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
    ASSERT_TRUE(result.HasLabel("supportedFormats"));
    EXPECT_EQ(3u, result["supportedFormats"].Array().Length());
}

TEST_F(DisplaySettingsL2Test, GetSupportedVideoCodingFormatsReportsMissingVideoDevice)
{
    JsonObject parameters;
    JsonObject result;

    ON_CALL(*p_dsVideoDeviceHalMock, dsGetVideoDevice(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(dsERR_GENERAL));

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "getSupportedVideoCodingFormats", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, GetVideoCodecInfoRejectsUnsupportedCodec)
{
    JsonObject parameters;
    JsonObject result;
    parameters["codec"] = "VP9";

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "getVideoCodecInfo", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_FALSE(result["success"].Boolean());
}

TEST_F(DisplaySettingsL2Test, GetVideoCodecInfoMapsH264)
{
    JsonObject parameters;
    JsonObject result;
    parameters["codec"] = "H264";

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "getVideoCodecInfo", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
    ASSERT_TRUE(result.HasLabel("entries"));
    EXPECT_EQ(1u, result["entries"].Array().Length());
}

TEST_F(DisplaySettingsL2Test, GetVideoCodecInfoMapsMpeg2)
{
    JsonObject parameters;
    JsonObject result;
    parameters["codec"] = "MPEG2";

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "getVideoCodecInfo", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
    ASSERT_TRUE(result.HasLabel("entries"));
    EXPECT_EQ(1u, result["entries"].Array().Length());
}

TEST_F(DisplaySettingsL2Test, GetSupportedResolutionsReturnsEmptyWhenDisplayDisconnected)
{
    JsonObject parameters;
    JsonObject result;

    ON_CALL(*p_dsVideoPortHalMock, dsIsDisplayConnected(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](intptr_t, bool* connected) {
            *connected = false;
            return dsERR_NONE;
        }));

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "getSupportedResolutions", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
    ASSERT_TRUE(result.HasLabel("supportedResolutions"));
    EXPECT_EQ(0u, result["supportedResolutions"].Array().Length());
}

TEST_F(DisplaySettingsL2Test, GetSupportedResolutionsReturnsCapabilitiesWhenConnected)
{
    JsonObject parameters;
    JsonObject result;
    parameters["videoDisplay"] = "INTERNAL";

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "getSupportedResolutions", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
    ASSERT_TRUE(result.HasLabel("supportedResolutions"));
}

TEST_F(DisplaySettingsL2Test, GetSupportedResolutionsUsesDefaultPort)
{
    JsonObject parameters;
    JsonObject result;

    const uint32_t status = InvokeServiceMethod(
        kDisplaySettingsCallsign, "getSupportedResolutions", parameters, result);

    EXPECT_EQ(Core::ERROR_NONE, status);
    ASSERT_TRUE(result.HasLabel("success"));
    EXPECT_TRUE(result["success"].Boolean());
    ASSERT_TRUE(result.HasLabel("supportedResolutions"));
}

TEST_F(DisplaySettingsL2Test, CoversDisplayInformationEndpoints)
{
    const std::vector<const char*> methods = {
        "getConnectedVideoDisplays",
        "getConnectedAudioPorts",
        "getSupportedVideoDisplays",
        "getSupportedTvResolutions",
        "getSupportedSettopResolutions",
        "getSupportedAudioPorts",
        "getSupportedAudioModes",
        "getCurrentResolution",
        "getAudioFormat",
        "getVideoFormat",
        "getColorDepthCapabilities",
        "getCurrentOutputSettings",
        "getZoomSetting",
        "getTVHDRCapabilities",
        "getSinkAtmosCapability",
    };

    for (const char* method : methods) {
        JsonObject parameters;
        JsonObject result;
        const uint32_t status = InvokeServiceMethod(
            kDisplaySettingsCallsign, method, parameters, result);

        SCOPED_TRACE(method);
        EXPECT_EQ(Core::ERROR_NONE, status);
        EXPECT_TRUE(result.HasLabel("success"));
    }
}

TEST_F(DisplaySettingsL2Test, CoversAudioAndPictureEndpointDispatch)
{
    const std::vector<const char*> methods = {
        "getDRCMode",
        "setDRCMode",
        "getSoundMode",
        "setSoundMode",
        "getActiveInput",
        "getVolumeLevel",
        "setVolumeLevel",
        "getBassEnhancer",
        "setBassEnhancer",
        "getFaderControl",
        "setFaderControl",
        "getTvHDRSupport",
        "setForceHDRMode",
        "getAudioEncoding",
        "setAudioEncoding",
        "getVolumeLeveller",
        "setVolumeLeveller",
        "resetBassEnhancer",
        "getDolbyVolumeMode",
        "setDolbyVolumeMode",
        "getEnableAudioPort",
        "setEnableAudioPort",
        "getPrimaryLanguage",
        "setPrimaryLanguage",
        "getSecondaryLanguage",
        "setSecondaryLanguage",
        "getDefaultResolution",
        "getDisplayAspectRatio",
        "getPreferredColorDepth",
        "setPreferredColorDepth",
        "getSupportedMS12Config",
        "getSurroundVirtualizer",
        "isSurroundDecoderEnabled",
        "enableSurroundDecoder",
        "setMISteering",
        "getMISteering",
        "resetDialogEnhancement",
    };

    for (const char* method : methods) {
        JsonObject parameters;
        JsonObject result;
        const uint32_t status = InvokeServiceMethod(
            kDisplaySettingsCallsign, method, parameters, result);

        SCOPED_TRACE(method);
        EXPECT_NE(Core::ERROR_UNAVAILABLE, status);
    }
}

TEST_F(DisplaySettingsL2Test, CoversValidDisplaySettingsEndpointPaths)
{
    const std::vector<const char*> methods = {
        "readEDID",
        "readHostEDID",
        "setZoomSetting",
        "setCurrentResolution",
        "setForceHDRMode",
        "setMS12AudioCompression",
        "getMS12AudioCompression",
        "setDialogEnhancement",
        "getDialogEnhancement",
        "setIntelligentEqualizerMode",
        "getIntelligentEqualizerMode",
        "setGraphicEqualizerMode",
        "getGraphicEqualizerMode",
        "setMS12AudioProfile",
        "getMS12AudioProfile",
        "getSupportedMS12AudioProfiles",
        "getAudioDelay",
        "setAudioDelay",
        "setAudioAtmosOutputMode",
        "getSettopHDRSupport",
        "setScartParameter",
        "isConnectedDeviceRepeater",
        "setGain",
        "getGain",
        "setMuted",
        "getMuted",
        "setAssociatedAudioMixing",
        "getAssociatedAudioMixing",
        "setSecondaryLanguage",
        "getSecondaryLanguage",
        "resetVolumeLeveller",
        "resetSurroundVirtualizer",
        "getSettopMS12Capabilities",
        "getSettopAudioCapabilities",
        "setMS12ProfileSettingsOverride",
    };

    for (const char* method : methods) {
        JsonObject parameters;
        JsonObject result;
        parameters["videoDisplay"] = "HDMI0";
        parameters["audioPort"] = "HDMI0";
        parameters["enable"] = true;
        parameters["value"] = 1;
        parameters["level"] = 50;
        parameters["mode"] = "on";
        parameters["resolution"] = "1080p60";
        parameters["persist"] = true;
        parameters["ignoreEdid"] = false;
        parameters["hdr_mode"] = "DV";
        parameters["encoding"] = "AC3";
        parameters["lang"] = "en-US";
        parameters["MISteeringEnable"] = true;

        const uint32_t status = InvokeServiceMethod(
            kDisplaySettingsCallsign, method, parameters, result);

        SCOPED_TRACE(method);
        EXPECT_NE(Core::ERROR_UNAVAILABLE, status);
    }
}

TEST_F(DisplaySettingsL2Test, CoversAudioNumericParameterBranches)
{
    const auto invoke = [this](const char* method, const char* name, const char* value) {
        JsonObject parameters;
        JsonObject result;
        parameters[name] = value;
        parameters["audioPort"] = "HDMI0";
        return InvokeServiceMethod(kDisplaySettingsCallsign, method, parameters, result);
    };

    EXPECT_EQ(Core::ERROR_NONE, invoke("setVolumeLeveller", "level", "5"));
    EXPECT_EQ(Core::ERROR_NONE, invoke("setVolumeLeveller", "level", "invalid"));
    EXPECT_EQ(Core::ERROR_NONE, invoke("setBassEnhancer", "bassBoost", "25"));
    EXPECT_EQ(Core::ERROR_NONE, invoke("setBassEnhancer", "bassBoost", "invalid"));
    EXPECT_EQ(Core::ERROR_NONE, invoke("setSurroundVirtualizer", "boost", "20"));
    EXPECT_EQ(Core::ERROR_NONE, invoke("setSurroundVirtualizer", "boost", "invalid"));

    JsonObject volumeModeParameters;
    JsonObject volumeModeResult;
    volumeModeParameters["mode"] = "2";
    volumeModeParameters["level"] = "5";
    volumeModeParameters["audioPort"] = "HDMI0";
    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(
        kDisplaySettingsCallsign, "setVolumeLeveller2", volumeModeParameters, volumeModeResult));

    volumeModeParameters["mode"] = "0";
    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(
        kDisplaySettingsCallsign, "setVolumeLeveller2", volumeModeParameters, volumeModeResult));

    volumeModeParameters["mode"] = "1";
    volumeModeParameters["level"] = "5";
    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(
        kDisplaySettingsCallsign, "setVolumeLeveller2", volumeModeParameters, volumeModeResult));

    volumeModeParameters["mode"] = "invalid";
    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(
        kDisplaySettingsCallsign, "setVolumeLeveller2", volumeModeParameters, volumeModeResult));

    JsonObject surroundModeParameters;
    JsonObject surroundModeResult;
    surroundModeParameters["mode"] = "2";
    surroundModeParameters["audioPort"] = "HDMI0";
    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(
        kDisplaySettingsCallsign, "setSurroundVirtualizer2", surroundModeParameters, surroundModeResult));

    surroundModeParameters["mode"] = "0";
    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(
        kDisplaySettingsCallsign, "setSurroundVirtualizer2", surroundModeParameters, surroundModeResult));

    surroundModeParameters["mode"] = "1";
    surroundModeParameters["boost"] = "20";
    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(
        kDisplaySettingsCallsign, "setSurroundVirtualizer2", surroundModeParameters, surroundModeResult));

    surroundModeParameters["mode"] = "invalid";
    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(
        kDisplaySettingsCallsign, "setSurroundVirtualizer2", surroundModeParameters, surroundModeResult));

    JsonObject delayParameters;
    JsonObject delayResult;
    delayParameters["audioDelay"] = "100";
    delayParameters["audioPort"] = "HDMI0";
    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDelay", delayParameters, delayResult));

    delayParameters["audioDelay"] = "invalid";
    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDelay", delayParameters, delayResult));

    delayParameters["audioDelay"] = "-1";
    EXPECT_EQ(Core::ERROR_NONE, InvokeServiceMethod(
        kDisplaySettingsCallsign, "setAudioDelay", delayParameters, delayResult));
}
