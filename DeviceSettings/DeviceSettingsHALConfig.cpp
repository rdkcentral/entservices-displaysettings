/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

/**
 * @file DeviceSettingsHALConfig.cpp
 * @brief Shared HAL configuration loading for FPD, Audio, and VideoPort.
 *
 * All three components use the identical dlopen → dlsym → deep-copy → dlclose
 * pattern.  This file consolidates that duplicated code so each component's
 * implementation file only calls the public DeviceSettingsHAL:: functions.
 */

#include "Module.h"
#include "DeviceSettingsHALConfig.h"

/* Component headers — each pulls in the raw HAL type headers it needs:
 *   fpd.h      → dsFPDTypes.h     (dsFPDColorConfig_t etc. at global scope)
 *   Audio.h    → dsAudio.h        (dsAudioTypeConfig_t etc. at global scope)
 *               + "using namespace WPEFramework::Exchange;"
 *   VideoPort.h → dsVideoPort.h   (dsVideoPortTypeConfig_t etc. at global scope)
 */
#include "fpd.h"
#include "Audio.h"
#include "VideoPort.h"

#include <dlfcn.h>
#include <cstring>
#include <unistd.h>
#include <sstream>
#include <type_traits>

// ---------------------------------------------------------------------------
// File-local helpers (anonymous namespace = not visible outside this TU)
// ---------------------------------------------------------------------------
namespace {

// ── Shared DL symbol helper ────────────────────────────────────────────────

typedef struct _dlSymbolLookup {
    const char* name;
    void**      dataptr;
} dlSymbolLookup;

static bool LoadDLSymbols(void* pDLHandle, const dlSymbolLookup* symbols, const int numberOfSymbols)
{
    int  currentSymbols    = 0;
    bool isAllSymbolsLoaded = false;

    if ((pDLHandle == NULL) || (symbols == NULL)) {
        LOGERR("LoadDLSymbols: Invalid handle or symbols");
        return false;
    }

    for (int i = 0; i < numberOfSymbols; i++) {
        if ((symbols[i].dataptr == NULL) || (symbols[i].name == NULL)) {
            LOGERR("LoadDLSymbols: Invalid symbol entry at index %d", i);
            continue;
        }

        *(symbols[i].dataptr) = dlsym(pDLHandle, symbols[i].name);
        if (*(symbols[i].dataptr) == NULL) {
            LOGWARN("LoadDLSymbols: [%s] not found", symbols[i].name);
        } else {
            currentSymbols++;
        }
    }

    isAllSymbolsLoaded = (numberOfSymbols > 0) ? (currentSymbols == numberOfSymbols) : false;
    return isAllSymbolsLoaded;
}

// ── FPD ───────────────────────────────────────────────────────────────────

static const char* kDefaultSupportedCharacters = "ABCEDFG";

typedef struct _fpdConfigs {
    const dsFPDColorConfig_t*       pKFPDIndicatorColors;
    const dsFPDIndicatorConfig_t*   pKIndicators;
    const dsFPDTextDisplayConfig_t* pKTextDisplays;
    int*                            pKFPDIndicatorColors_size;
    int*                            pKIndicators_size;
    int*                            pKTextDisplays_size;
} fpdConfigs_t;

static bool LoadFrontPanelConfigFromHAL(fpdConfigs_t& config, void*& outHandle)
{
    void* pDLHandle       = NULL;
    bool  isSymbolsLoaded = false;

    outHandle = NULL;
    memset(&config, 0, sizeof(config));

    dlerror();
    pDLHandle = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
    if (pDLHandle == NULL) {
        const char* dlErr = dlerror();
        LOGWARN("LoadFrontPanelConfigFromHAL: dlopen failed for %s: %s",
                RDK_DSHAL_NAME, (dlErr ? dlErr : "unknown"));
        return false;
    }

    dlSymbolLookup fpdConfigSymbols[] = {
        {"kFPDIndicatorColors",      (void**)&config.pKFPDIndicatorColors},
        {"kFPDIndicatorColors_size", (void**)&config.pKFPDIndicatorColors_size},
        {"kIndicators",              (void**)&config.pKIndicators},
        {"kIndicators_size",         (void**)&config.pKIndicators_size},
        {"kFPDTextDisplays",         (void**)&config.pKTextDisplays},
        {"kFPDTextDisplays_size",    (void**)&config.pKTextDisplays_size}
    };

    isSymbolsLoaded = LoadDLSymbols(pDLHandle, fpdConfigSymbols,
                                    sizeof(fpdConfigSymbols) / sizeof(dlSymbolLookup));

    if (!isSymbolsLoaded) {
        LOGWARN("LoadFrontPanelConfigFromHAL: Failed to load all front panel symbols from HAL");
        dlclose(pDLHandle);
        return false;
    }

    if ((config.pKFPDIndicatorColors == NULL) || (config.pKIndicators == NULL) ||
        (config.pKTextDisplays == NULL) || (config.pKFPDIndicatorColors_size == NULL) ||
        (config.pKIndicators_size == NULL) || (config.pKTextDisplays_size == NULL)) {
        LOGWARN("LoadFrontPanelConfigFromHAL: HAL symbols loaded but one or more pointers are null");
        dlclose(pDLHandle);
        return false;
    }

    outHandle = pDLHandle;
    return true;
}

// ── Audio ─────────────────────────────────────────────────────────────────

typedef struct _audioConfigs {
    const dsAudioTypeConfig_t* pKConfigs;
    const dsAudioPortConfig_t* pKPorts;
    int*                       pKConfigSize;
    int*                       pKPortSize;
} audioConfigs_t;

template <typename EnumType>
static uint32_t ToEnumMask(const EnumType* values, const size_t count)
{
    static_assert(std::is_enum<EnumType>::value, "EnumType must be an enum");

    uint32_t mask = 0;
    for (size_t index = 0; index < count; ++index) {
        const uint32_t bit = static_cast<uint32_t>(values[index]);
        if (bit < (sizeof(mask) * 8)) {
            mask |= (1u << bit);
        }
    }
    return mask;
}

static bool LoadAudioConfigFromHAL(audioConfigs_t& config, void*& outHandle)
{
    void* pDLHandle       = NULL;
    bool  isSymbolsLoaded = false;

    outHandle = NULL;
    memset(&config, 0, sizeof(config));

    dlerror();
    pDLHandle = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
    if (pDLHandle == NULL) {
        const char* dlErr = dlerror();
        LOGWARN("LoadAudioConfigFromHAL: dlopen failed for %s: %s",
                RDK_DSHAL_NAME, (dlErr ? dlErr : "unknown"));
        return false;
    }

    dlSymbolLookup audioConfigSymbols[] = {
        {"kAudioConfigs",      (void**)&config.pKConfigs},
        {"kAudioPorts",        (void**)&config.pKPorts},
        {"kAudioConfigs_size", (void**)&config.pKConfigSize},
        {"kAudioPorts_size",   (void**)&config.pKPortSize}
    };

    isSymbolsLoaded = LoadDLSymbols(pDLHandle, audioConfigSymbols,
                                    sizeof(audioConfigSymbols) / sizeof(dlSymbolLookup));

    if (!isSymbolsLoaded) {
        LOGWARN("LoadAudioConfigFromHAL: Failed to load all audio symbols from HAL");
        dlclose(pDLHandle);
        return false;
    }

    if ((config.pKConfigs == NULL) || (config.pKPorts == NULL) ||
        (config.pKConfigSize == NULL) || (config.pKPortSize == NULL)) {
        LOGWARN("LoadAudioConfigFromHAL: HAL symbols loaded but one or more pointers are null");
        dlclose(pDLHandle);
        return false;
    }

    outHandle = pDLHandle;
    return true;
}

// ── VideoPort ─────────────────────────────────────────────────────────────

typedef struct _videoPortConfigs {
    const dsVideoPortTypeConfig_t* pKConfigs;
    int*                           pKVideoPortConfigs_size;
    const dsVideoPortPortConfig_t* pKPorts;
    int*                           pKVideoPortPorts_size;
    dsVideoPortResolution_t*       pKResolutionsSettings;
    int*                           pKResolutionsSettings_size;
} videoPortConfigs_t;

static bool LoadVideoPortConfigFromHAL(videoPortConfigs_t& config, void*& outHandle)
{
    void* pDLHandle       = NULL;
    bool  isSymbolsLoaded = false;

    outHandle = NULL;
    memset(&config, 0, sizeof(config));

    dlerror();
    pDLHandle = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
    if (pDLHandle == NULL) {
        const char* dlErr = dlerror();
        LOGWARN("LoadVideoPortConfigFromHAL: dlopen failed for %s: %s",
                RDK_DSHAL_NAME, (dlErr ? dlErr : "unknown"));
        return false;
    }

    dlSymbolLookup videoPortConfigSymbols[] = {
        {"kVideoPortConfigs",       (void**)&config.pKConfigs},
        {"kVideoPortConfigs_size",  (void**)&config.pKVideoPortConfigs_size},
        {"kVideoPortPorts",         (void**)&config.pKPorts},
        {"kVideoPortPorts_size",    (void**)&config.pKVideoPortPorts_size},
        {"kResolutionsSettings",    (void**)&config.pKResolutionsSettings},
        {"kResolutionsSettings_size",(void**)&config.pKResolutionsSettings_size}
    };

    isSymbolsLoaded = LoadDLSymbols(pDLHandle, videoPortConfigSymbols,
                                    sizeof(videoPortConfigSymbols) / sizeof(dlSymbolLookup));

    if (!isSymbolsLoaded) {
        LOGWARN("LoadVideoPortConfigFromHAL: Failed to load all video port symbols from HAL");
        dlclose(pDLHandle);
        return false;
    }

    if ((config.pKConfigs == NULL) || (config.pKPorts == NULL) ||
        (config.pKResolutionsSettings == NULL) || (config.pKVideoPortConfigs_size == NULL) ||
        (config.pKVideoPortPorts_size == NULL) || (config.pKResolutionsSettings_size == NULL)) {
        LOGWARN("LoadVideoPortConfigFromHAL: HAL symbols loaded but one or more pointers are null");
        dlclose(pDLHandle);
        return false;
    }

    outHandle = pDLHandle;
    return true;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public DeviceSettingsHAL namespace
// ---------------------------------------------------------------------------
namespace DeviceSettingsHAL {

// ── FPD ───────────────────────────────────────────────────────────────────

void PopulateFPDConfig(
    std::vector<FPDColorConfig>&     colors,
    std::vector<FPDIndicatorConfig>& indicators,
    std::vector<FPDTextDisplayConfig>& textDisplays,
    std::vector<FPDColorBinding>&    colorBindings)
{
    fpdConfigs_t halConfig;
    void* halHandle = NULL;
    memset(&halConfig, 0, sizeof(halConfig));
    bool loadedFromHAL = LoadFrontPanelConfigFromHAL(halConfig, halHandle);

    colors.clear();
    indicators.clear();
    textDisplays.clear();
    colorBindings.clear();

    if (loadedFromHAL) {
        const int colorCount       = *(halConfig.pKFPDIndicatorColors_size);
        const int indicatorCount   = *(halConfig.pKIndicators_size);
        const int textDisplayCount = *(halConfig.pKTextDisplays_size);

        for (int i = 0; i < colorCount; i++) {
            const dsFPDColorConfig_t& cfg = halConfig.pKFPDIndicatorColors[i];
            FPDColorConfig colorCfg;
            colorCfg.id    = cfg.id;
            colorCfg.color = cfg.color;
            colors.push_back(colorCfg);
        }

        for (int i = 0; i < indicatorCount; i++) {
            const dsFPDIndicatorConfig_t& cfg = halConfig.pKIndicators[i];
            FPDIndicatorConfig indicatorCfg;
            indicatorCfg.id            = cfg.id;
            indicatorCfg.maxBrightness = cfg.maxBrightness;
            indicatorCfg.maxCycleRate  = cfg.maxCycleRate;
            indicatorCfg.minBrightness = cfg.minBrightness;
            indicatorCfg.levels        = cfg.levels;
            indicatorCfg.colorMode     = cfg.colorMode;
            indicators.push_back(indicatorCfg);

            if (cfg.supportedColors != nullptr) {
                for (int colorIndex = 0; colorIndex < colorCount; ++colorIndex) {
                    const dsFPDColorConfig_t& colorCfg = halConfig.pKFPDIndicatorColors[colorIndex];
                    FPDColorBinding mapEntry;
                    mapEntry.targetType = DeviceSettingsFPD::DS_FPD_COLOR_TARGET_INDICATOR;
                    mapEntry.targetId   = cfg.id;
                    mapEntry.colorId    = colorCfg.id;
                    colorBindings.push_back(mapEntry);
                }
            }
        }

        for (int i = 0; i < textDisplayCount; i++) {
            const dsFPDTextDisplayConfig_t& cfg = halConfig.pKTextDisplays[i];
            FPDTextDisplayConfig textDisplayCfg;
            textDisplayCfg.id                    = cfg.id;
            textDisplayCfg.name                  = (cfg.name ? cfg.name : "");
            textDisplayCfg.maxBrightness          = cfg.maxBrightness;
            textDisplayCfg.maxCycleRate           = cfg.maxCycleRate;
            textDisplayCfg.supportedCharacters    = (cfg.supportedCharacters
                                                        ? cfg.supportedCharacters
                                                        : kDefaultSupportedCharacters);
            textDisplayCfg.columns               = cfg.columns;
            textDisplayCfg.rows                  = cfg.rows;
            textDisplayCfg.maxHorizontalIterations = cfg.maxHorizontalIterations;
            textDisplayCfg.maxVerticalIterations   = cfg.maxVerticalIterations;
            textDisplayCfg.levels                = cfg.levels;
            textDisplayCfg.colorMode             = cfg.colorMode;
            textDisplays.push_back(textDisplayCfg);

            if (cfg.supportedColors != nullptr) {
                for (int colorIndex = 0; colorIndex < colorCount; ++colorIndex) {
                    const dsFPDColorConfig_t& colorCfg = halConfig.pKFPDIndicatorColors[colorIndex];
                    FPDColorBinding mapEntry;
                    mapEntry.targetType = DeviceSettingsFPD::DS_FPD_COLOR_TARGET_TEXTDISPLAY;
                    mapEntry.targetId   = cfg.id;
                    mapEntry.colorId    = colorCfg.id;
                    colorBindings.push_back(mapEntry);
                }
            }
        }

        LOGINFO("PopulateFPDConfig: Loaded config from HAL (colors=%d indicators=%d textDisplays=%d)",
                colorCount, indicatorCount, textDisplayCount);
        dlclose(halHandle);
        halHandle = NULL;
        return;
    }

    LOGWARN("PopulateFPDConfig: HAL config not available, returning empty config");
}

void DumpFPDConfig(
    const std::vector<FPDColorConfig>&     colors,
    const std::vector<FPDIndicatorConfig>& indicators,
    const std::vector<FPDTextDisplayConfig>& textDisplays,
    const std::vector<FPDColorBinding>&    colorBindings)
{
    if (-1 == access("/opt/dsMgrDumpDeviceConfigs", F_OK)) {
        LOGINFO("DumpFPDConfig: Dumping of Device configs is disabled");
        return;
    }

    LOGINFO("\n=============== Dump DeviceSettings FPD Cached Config ===============");
    LOGINFO("Colors count=%zu", colors.size());
    for (size_t i = 0; i < colors.size(); ++i) {
        LOGINFO("colors[%zu]: id=%d color=%d", i, colors[i].id, colors[i].color);
    }

    LOGINFO("Indicators count=%zu", indicators.size());
    for (size_t i = 0; i < indicators.size(); ++i) {
        const FPDIndicatorConfig& cfg = indicators[i];
        LOGINFO("indicators[%zu]: id=%d maxBrightness=%d maxCycleRate=%d minBrightness=%d levels=%d colorMode=%d",
                i, cfg.id, cfg.maxBrightness, cfg.maxCycleRate,
                cfg.minBrightness, cfg.levels, cfg.colorMode);
    }

    LOGINFO("TextDisplays count=%zu", textDisplays.size());
    for (size_t i = 0; i < textDisplays.size(); ++i) {
        const FPDTextDisplayConfig& cfg = textDisplays[i];
        LOGINFO("textDisplays[%zu]: id=%d name=%s maxBrightness=%d maxCycleRate=%d columns=%d rows=%d maxHIter=%d maxVIter=%d levels=%d colorMode=%d supportedChars=%s",
                i, cfg.id, cfg.name.c_str(), cfg.maxBrightness, cfg.maxCycleRate,
                cfg.columns, cfg.rows, cfg.maxHorizontalIterations,
                cfg.maxVerticalIterations, cfg.levels, cfg.colorMode,
                cfg.supportedCharacters.c_str());
    }

    LOGINFO("ColorBindings count=%zu", colorBindings.size());
    for (size_t i = 0; i < colorBindings.size(); ++i) {
        const FPDColorBinding& cfg = colorBindings[i];
        LOGINFO("colorBindings[%zu]: targetType=%d targetId=%d colorId=%d",
                i, static_cast<int>(cfg.targetType), cfg.targetId, cfg.colorId);
    }

    LOGINFO("=============== Dump DeviceSettings FPD Cached Config done ===============\n");
}

// ── Audio ─────────────────────────────────────────────────────────────────

void PopulateAudioConfig(
    std::vector<AudioTypeConfigInfo>& audioTypes,
    std::vector<AudioPortConfigInfo>& audioPorts)
{
    audioConfigs_t halConfig;
    void* halHandle = NULL;
    memset(&halConfig, 0, sizeof(halConfig));
    const bool loadedFromHAL = LoadAudioConfigFromHAL(halConfig, halHandle);

    audioTypes.clear();
    audioPorts.clear();

    if (!loadedFromHAL) {
        LOGWARN("PopulateAudioConfig: HAL config not available, returning empty config");
        return;
    }

    const int typeCount = *(halConfig.pKConfigSize);
    const int portCount = *(halConfig.pKPortSize);

    for (int i = 0; i < typeCount; i++) {
        const dsAudioTypeConfig_t& cfg = halConfig.pKConfigs[i];

        AudioTypeConfigInfo typeCfg;
        typeCfg.typeId = cfg.typeId;
        typeCfg.name   = (cfg.name ? cfg.name : "");
        typeCfg.supportedCompressionMask = (cfg.compressions != NULL)
            ? ToEnumMask(cfg.compressions, cfg.numSupportedCompressions)
            : 0;
        typeCfg.supportedEncodingMask = (cfg.encodings != NULL)
            ? ToEnumMask(cfg.encodings, cfg.numSupportedEncodings)
            : 0;
        typeCfg.supportedStereoModeMask = (cfg.stereoModes != NULL)
            ? ToEnumMask(cfg.stereoModes, cfg.numSupportedStereoModes)
            : 0;
        audioTypes.push_back(typeCfg);
    }

    for (int i = 0; i < portCount; i++) {
        const dsAudioPortConfig_t& cfg = halConfig.pKPorts[i];

        AudioPortConfigInfo portCfg;
        portCfg.audioPortType  = static_cast<AudioPortType>(cfg.id.type);
        portCfg.audioPortIndex = cfg.id.index;
        if (cfg.connectedVOPs != NULL) {
            portCfg.connectedVideoPortType  = static_cast<int32_t>(cfg.connectedVOPs->type);
            portCfg.connectedVideoPortIndex = cfg.connectedVOPs->index;
        } else {
            portCfg.connectedVideoPortType  = -1;
            portCfg.connectedVideoPortIndex = -1;
        }
        audioPorts.push_back(portCfg);
    }

    LOGINFO("PopulateAudioConfig: Loaded config from HAL (audioTypes=%zu audioPorts=%zu)",
            audioTypes.size(), audioPorts.size());
    dlclose(halHandle);
    halHandle = NULL;
}

void DumpAudioConfig(
    const std::vector<AudioTypeConfigInfo>& audioTypes,
    const std::vector<AudioPortConfigInfo>& audioPorts)
{
    if (-1 == access("/opt/dsMgrDumpDeviceConfigs", F_OK)) {
        LOGINFO("DumpAudioConfig: Dumping of Device configs is disabled");
        return;
    }

    LOGINFO("\n=============== Dump DeviceSettings Audio Cached Config ===============");
    LOGINFO("AudioTypes count=%zu", audioTypes.size());
    for (size_t i = 0; i < audioTypes.size(); ++i) {
        const AudioTypeConfigInfo& cfg = audioTypes[i];
        LOGINFO("audioTypes[%zu]: typeId=%d name=%s compressionMask=0x%x encodingMask=0x%x stereoModeMask=0x%x",
                i,
                static_cast<int>(cfg.typeId),
                cfg.name.c_str(),
                static_cast<unsigned int>(cfg.supportedCompressionMask),
                static_cast<unsigned int>(cfg.supportedEncodingMask),
                static_cast<unsigned int>(cfg.supportedStereoModeMask));
    }

    LOGINFO("AudioPorts count=%zu", audioPorts.size());
    for (size_t i = 0; i < audioPorts.size(); ++i) {
        const AudioPortConfigInfo& cfg = audioPorts[i];
        LOGINFO("audioPorts[%zu]: portType=%d portIndex=%d connectedVideoPortType=%d connectedVideoPortIndex=%d",
                i,
                static_cast<int>(cfg.audioPortType),
                cfg.audioPortIndex,
                cfg.connectedVideoPortType,
                cfg.connectedVideoPortIndex);
    }

    LOGINFO("=============== Dump DeviceSettings Audio Cached Config done ===============\n");
}

// ── VideoPort ─────────────────────────────────────────────────────────────

void PopulateVideoPortConfig(
    std::vector<VideoPortTypeConfig>& videoPortTypes,
    std::vector<VideoPortPortConfig>& videoPorts,
    std::vector<VideoPortResolution>& resolutions)
{
    videoPortConfigs_t halConfig;
    void* halHandle = NULL;
    memset(&halConfig, 0, sizeof(halConfig));
    const bool loadedFromHAL = LoadVideoPortConfigFromHAL(halConfig, halHandle);

    videoPortTypes.clear();
    videoPorts.clear();
    resolutions.clear();

    if (!loadedFromHAL) {
        LOGWARN("PopulateVideoPortConfig: HAL config not available, returning empty config");
        return;
    }

    const int configCount     = *(halConfig.pKVideoPortConfigs_size);
    const int portCount       = *(halConfig.pKVideoPortPorts_size);
    const int resolutionCount = *(halConfig.pKResolutionsSettings_size);

    for (int i = 0; i < configCount; i++) {
        const dsVideoPortTypeConfig_t& cfg = halConfig.pKConfigs[i];

        VideoPortTypeConfig typeCfg;
        typeCfg.typeId               = static_cast<VideoPortType>(cfg.typeId);
        typeCfg.name                 = (cfg.name ? cfg.name : "");
        typeCfg.dtcpSupported        = cfg.dtcpSupported;
        typeCfg.hdcpSupported        = cfg.hdcpSupported;
        typeCfg.restrictedResollution = cfg.restrictedResollution;
        if ((cfg.supportedResolutions != NULL) && (cfg.numSupportedResolutions > 0)) {
            std::ostringstream supportedResolutions;
            for (size_t j = 0; j < cfg.numSupportedResolutions; ++j) {
                if (j != 0) {
                    supportedResolutions << ',';
                }
                supportedResolutions << cfg.supportedResolutions[j].name;
            }
            typeCfg.supportedResolutionNames = supportedResolutions.str();
        } else {
            typeCfg.supportedResolutionNames.clear();
        }
        videoPortTypes.push_back(typeCfg);
    }

    for (int i = 0; i < portCount; i++) {
        const dsVideoPortPortConfig_t& cfg = halConfig.pKPorts[i];

        VideoPortPortConfig portCfg;
        portCfg.videoPortType            = static_cast<VideoPortType>(cfg.id.type);
        portCfg.videoPortIndex           = cfg.id.index;
        portCfg.connectedAudioPortType   = static_cast<int32_t>(cfg.connectedAOP.type);
        portCfg.connectedAudioPortIndex  = cfg.connectedAOP.index;
        portCfg.defaultResolution        = (cfg.defaultResolution ? cfg.defaultResolution : "");
        videoPorts.push_back(portCfg);
    }

    for (int i = 0; i < resolutionCount; i++) {
        const dsVideoPortResolution_t& cfg = halConfig.pKResolutionsSettings[i];

        VideoPortResolution resCfg;
        resCfg.name              = cfg.name;
        resCfg.pixelResolution   = static_cast<VideoResolution>(cfg.pixelResolution);
        resCfg.aspectRatio       = static_cast<VideoAspectRatio>(cfg.aspectRatio);
        resCfg.stereoScopicMode  = static_cast<VideoStereoScopicMode>(cfg.stereoScopicMode);
        resCfg.frameRate         = static_cast<VideoFrameRate>(cfg.frameRate);
        resCfg.interlaced        = cfg.interlaced;
        resolutions.push_back(resCfg);
    }

    LOGINFO("PopulateVideoPortConfig: Loaded config from HAL (videoPortTypes=%zu videoPorts=%zu resolutions=%zu)",
            videoPortTypes.size(), videoPorts.size(), resolutions.size());
    dlclose(halHandle);
    halHandle = NULL;
}

void DumpVideoPortConfig(
    const std::vector<VideoPortTypeConfig>& videoPortTypes,
    const std::vector<VideoPortPortConfig>& videoPorts,
    const std::vector<VideoPortResolution>& resolutions)
{
    if (-1 == access("/opt/dsMgrDumpDeviceConfigs", F_OK)) {
        LOGINFO("DumpVideoPortConfig: Dumping of Device configs is disabled");
        return;
    }

    LOGINFO("\n=============== Dump DeviceSettings VideoPort Cached Config ===============");
    LOGINFO("VideoPortTypes count=%zu", videoPortTypes.size());
    for (size_t i = 0; i < videoPortTypes.size(); ++i) {
        const VideoPortTypeConfig& cfg = videoPortTypes[i];
        LOGINFO("videoPortTypes[%zu]: typeId=%d name=%s dtcp=%s hdcp=%s restrictedRes=%d supportedResolutions=%s",
                i,
                static_cast<int>(cfg.typeId),
                cfg.name.c_str(),
                cfg.dtcpSupported ? "true" : "false",
                cfg.hdcpSupported ? "true" : "false",
                cfg.restrictedResollution,
                cfg.supportedResolutionNames.c_str());
    }

    LOGINFO("VideoPorts count=%zu", videoPorts.size());
    for (size_t i = 0; i < videoPorts.size(); ++i) {
        const VideoPortPortConfig& cfg = videoPorts[i];
        LOGINFO("videoPorts[%zu]: videoPortType=%d videoPortIndex=%d connectedAudioPortType=%d connectedAudioPortIndex=%d defaultResolution=%s",
                i,
                static_cast<int>(cfg.videoPortType),
                cfg.videoPortIndex,
                cfg.connectedAudioPortType,
                cfg.connectedAudioPortIndex,
                cfg.defaultResolution.c_str());
    }

    LOGINFO("Resolutions count=%zu", resolutions.size());
    for (size_t i = 0; i < resolutions.size(); ++i) {
        const VideoPortResolution& cfg = resolutions[i];
        LOGINFO("resolutions[%zu]: name=%s pixelResolution=%d aspectRatio=%d stereoScopicMode=%d frameRate=%d interlaced=%s",
                i,
                cfg.name.c_str(),
                static_cast<int>(cfg.pixelResolution),
                static_cast<int>(cfg.aspectRatio),
                static_cast<int>(cfg.stereoScopicMode),
                static_cast<int>(cfg.frameRate),
                cfg.interlaced ? "true" : "false");
    }

    LOGINFO("=============== Dump DeviceSettings VideoPort Cached Config done ===============\n");
}

} // namespace DeviceSettingsHAL
