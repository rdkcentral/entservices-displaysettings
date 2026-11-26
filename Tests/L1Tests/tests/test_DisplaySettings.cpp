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
 */

#include <gtest/gtest.h>

#include "DisplaySettings.h"

using namespace WPEFramework;

namespace {

class DisplaySettingsRegistrationTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::DisplaySettings> plugin =
        Core::ProxyType<Plugin::DisplaySettings>::Create();
    Core::JSONRPC::Handler& handler = *plugin;
};

TEST_F(DisplaySettingsRegistrationTest, RegistersAllPublicApis)
{
    const char* methods[] = {
        "getConnectedVideoDisplays", "getConnectedAudioPorts",
        "setEnableAudioPort", "getSupportedResolutions",
        "getSupportedVideoDisplays", "getSupportedTvResolutions",
        "getSupportedSettopResolutions", "getSupportedAudioPorts",
        "getSupportedAudioModes", "getZoomSetting", "setZoomSetting",
        "getCurrentResolution", "setCurrentResolution", "getSoundMode",
        "setSoundMode", "readEDID", "readHostEDID", "getActiveInput",
        "getTvHDRSupport", "getSettopHDRSupport", "getCurrentOutputSettings",
        "setForceHDRMode", "getVolumeLeveller", "getBassEnhancer",
        "isSurroundDecoderEnabled", "getDRCMode", "getSurroundVirtualizer",
        "setVolumeLeveller", "setBassEnhancer", "enableSurroundDecoder",
        "setSurroundVirtualizer", "setMISteering", "setGain", "getGain",
        "setMuted", "getMuted", "setVolumeLevel", "getVolumeLevel",
        "setDRCMode", "getMISteering", "setMS12AudioCompression",
        "getMS12AudioCompression", "setDolbyVolumeMode", "getDolbyVolumeMode",
        "setDialogEnhancement", "getDialogEnhancement",
        "setIntelligentEqualizerMode", "getIntelligentEqualizerMode",
        "setGraphicEqualizerMode", "getGraphicEqualizerMode",
        "setMS12AudioProfile", "getMS12AudioProfile",
        "getSupportedMS12AudioProfiles", "resetDialogEnhancement",
        "resetBassEnhancer", "resetSurroundVirtualizer", "resetVolumeLeveller",
        "setAssociatedAudioMixing", "getAssociatedAudioMixing",
        "setFaderControl", "getFaderControl", "setPrimaryLanguage",
        "getPrimaryLanguage", "setSecondaryLanguage", "getSecondaryLanguage",
        "getAudioDelay", "setAudioDelay", "getSinkAtmosCapability",
        "setAudioAtmosOutputMode", "getTVHDRCapabilities",
        "isConnectedDeviceRepeater", "getDefaultResolution", "setScartParameter",
        "getSettopMS12Capabilities", "getSettopAudioCapabilities",
        "setMS12ProfileSettingsOverride", "getVideoFormat", "setPreferredColorDepth",
        "getPreferredColorDepth", "getColorDepthCapabilities", "getSupportedMS12Config",
        "setAudioDucking", "setEnableVideoPort", "getEnableVideoPort",
        "getSupportedVideoCodingFormats", "getVideoCodecInfo", "getAudioEncoding",
        "setAudioEncoding", "getDisplayAspectRatio"
    };

    for (const char* method : methods) {
        SCOPED_TRACE(method);
        EXPECT_TRUE(handler.Exists(method));
    }
}

TEST_F(DisplaySettingsRegistrationTest, ExposesPluginInformation)
{
    EXPECT_TRUE(plugin->Information().empty());
}

} // namespace