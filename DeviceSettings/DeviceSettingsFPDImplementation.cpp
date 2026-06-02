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

#include "DeviceSettingsFPDImplementation.h"

#include "UtilsLogging.h"
#include <syscall.h>
#include <vector>
#include <dlfcn.h>
#include <cstring>

namespace {
    static const int32_t kMaxBrightness = 100;
    static const int32_t kMinBrightness = 0;
    static const int32_t kDefaultLevels = 10;
    static const int32_t kMaxCycleRate = 2;
    static const int32_t kMaxHorizontalColumns = 0;
    static const int32_t kMaxVerticalRows = 0;
    static const int32_t kMaxHorizontalIterations = 0;
    static const int32_t kMaxVerticalIterations = 0;
    static const int32_t kDefaultColorMode = 0;
    static const char* kDefaultSupportedCharacters = "ABCEDFG";

    typedef struct _dlSymbolLookup {
        const char* name;
        void** dataptr;
    } dlSymbolLookup;

    typedef struct _fpdConfigs {
        const dsFPDColorConfig_t* pKFPDIndicatorColors;
        const dsFPDIndicatorConfig_t* pKIndicators;
        const dsFPDTextDisplayConfig_t* pKTextDisplays;
        int* pKFPDIndicatorColors_size;
        int* pKIndicators_size;
        int* pKTextDisplays_size;
    } fpdConfigs_t;

    static bool LoadDLSymbols(void* pDLHandle, const dlSymbolLookup* symbols, const int numberOfSymbols)
    {
        int currentSymbols = 0;
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

    static bool LoadFrontPanelConfigFromHAL(fpdConfigs_t& config)
    {
        void* pDLHandle = NULL;
        bool isSymbolsLoaded = false;

        memset(&config, 0, sizeof(config));

        dlerror();
        pDLHandle = dlopen(RDK_DSHAL_NAME, RTLD_LAZY);
        if (pDLHandle == NULL) {
            const char* dlErr = dlerror();
            LOGWARN("LoadFrontPanelConfigFromHAL: dlopen failed for %s: %s", RDK_DSHAL_NAME, (dlErr ? dlErr : "unknown"));
            return false;
        }

        dlSymbolLookup fpdConfigSymbols[] = {
            {"kFPDIndicatorColors", (void**)&config.pKFPDIndicatorColors},
            {"kFPDIndicatorColors_size", (void**)&config.pKFPDIndicatorColors_size},
            {"kIndicators", (void**)&config.pKIndicators},
            {"kIndicators_size", (void**)&config.pKIndicators_size},
            {"kFPDTextDisplays", (void**)&config.pKTextDisplays},
            {"kFPDTextDisplays_size", (void**)&config.pKTextDisplays_size}
        };

        isSymbolsLoaded = LoadDLSymbols(pDLHandle, fpdConfigSymbols, sizeof(fpdConfigSymbols) / sizeof(dlSymbolLookup));
        dlclose(pDLHandle);

        if (!isSymbolsLoaded) {
            LOGWARN("LoadFrontPanelConfigFromHAL: Failed to load all front panel symbols from HAL");
            return false;
        }

        if ((config.pKFPDIndicatorColors == NULL) || (config.pKIndicators == NULL) || (config.pKTextDisplays == NULL) ||
            (config.pKFPDIndicatorColors_size == NULL) || (config.pKIndicators_size == NULL) || (config.pKTextDisplays_size == NULL)) {
            LOGWARN("LoadFrontPanelConfigFromHAL: HAL symbols loaded but one or more pointers are null");
            return false;
        }

        return true;
    }

    static void PopulateFrontPanelConfig(std::vector<FPDColorConfig>& colors,
                                         std::vector<FPDIndicatorConfig>& indicators,
                                         std::vector<FPDTextDisplayConfig>& textDisplays,
                                         std::vector<FPDColorBinding>& colorBindings)
    {
        fpdConfigs_t halConfig;
        memset(&halConfig, 0, sizeof(halConfig));
        bool loadedFromHAL = LoadFrontPanelConfigFromHAL(halConfig);

        colors.clear();
        indicators.clear();
        textDisplays.clear();
        colorBindings.clear();

        if (loadedFromHAL) {
            const int colorCount = *(halConfig.pKFPDIndicatorColors_size);
            const int indicatorCount = *(halConfig.pKIndicators_size);
            const int textDisplayCount = *(halConfig.pKTextDisplays_size);

            for (int i = 0; i < colorCount; i++) {
                const dsFPDColorConfig_t& cfg = halConfig.pKFPDIndicatorColors[i];
                FPDColorConfig colorCfg;
                colorCfg.id = cfg.id;
                colorCfg.color = cfg.color;
                colors.push_back(colorCfg);
            }

            for (int i = 0; i < indicatorCount; i++) {
                const dsFPDIndicatorConfig_t& cfg = halConfig.pKIndicators[i];
                FPDIndicatorConfig indicatorCfg;
                indicatorCfg.id = cfg.id;
                indicatorCfg.maxBrightness = cfg.maxBrightness;
                indicatorCfg.maxCycleRate = cfg.maxCycleRate;
                indicatorCfg.minBrightness = cfg.minBrightness;
                indicatorCfg.levels = cfg.levels;
                indicatorCfg.colorMode = cfg.colorMode;
                indicators.push_back(indicatorCfg);

                if (cfg.supportedColors != nullptr) {
                    for (int colorIndex = 0; colorIndex < colorCount; ++colorIndex) {
                        const dsFPDColorConfig_t& colorCfg = halConfig.pKFPDIndicatorColors[colorIndex];
                        FPDColorBinding mapEntry;
                        mapEntry.targetType = DeviceSettingsFPD::DS_FPD_COLOR_TARGET_INDICATOR;
                        mapEntry.targetId = cfg.id;
                        mapEntry.colorId = colorCfg.id;
                        colorBindings.push_back(mapEntry);
                    }
                }
            }

            for (int i = 0; i < textDisplayCount; i++) {
                const dsFPDTextDisplayConfig_t& cfg = halConfig.pKTextDisplays[i];
                FPDTextDisplayConfig textDisplayCfg;
                textDisplayCfg.id = cfg.id;
                textDisplayCfg.name = (cfg.name ? cfg.name : "");
                textDisplayCfg.maxBrightness = cfg.maxBrightness;
                textDisplayCfg.maxCycleRate = cfg.maxCycleRate;
                textDisplayCfg.supportedCharacters = (cfg.supportedCharacters ? cfg.supportedCharacters : kDefaultSupportedCharacters);
                textDisplayCfg.columns = cfg.columns;
                textDisplayCfg.rows = cfg.rows;
                textDisplayCfg.maxHorizontalIterations = cfg.maxHorizontalIterations;
                textDisplayCfg.maxVerticalIterations = cfg.maxVerticalIterations;
                textDisplayCfg.levels = cfg.levels;
                textDisplayCfg.colorMode = cfg.colorMode;
                textDisplays.push_back(textDisplayCfg);

                if (cfg.supportedColors != nullptr) {
                    for (int colorIndex = 0; colorIndex < colorCount; ++colorIndex) {
                        const dsFPDColorConfig_t& colorCfg = halConfig.pKFPDIndicatorColors[colorIndex];
                        FPDColorBinding mapEntry;
                        mapEntry.targetType = DeviceSettingsFPD::DS_FPD_COLOR_TARGET_TEXTDISPLAY;
                        mapEntry.targetId = cfg.id;
                        mapEntry.colorId = colorCfg.id;
                        colorBindings.push_back(mapEntry);
                    }
                }
            }

            LOGINFO("PopulateFrontPanelConfig: Loaded config from HAL (colors=%d indicators=%d textDisplays=%d)", colorCount, indicatorCount, textDisplayCount);
            return;
        }

        LOGWARN("PopulateFrontPanelConfig: HAL config not available, using fallback defaults");

        {
            FPDColorConfig cfg;
            cfg.id = 0; cfg.color = dsFPD_COLOR_BLUE; colors.push_back(cfg);
            cfg.id = 1; cfg.color = dsFPD_COLOR_GREEN; colors.push_back(cfg);
            cfg.id = 2; cfg.color = dsFPD_COLOR_RED; colors.push_back(cfg);
            cfg.id = 3; cfg.color = dsFPD_COLOR_YELLOW; colors.push_back(cfg);
            cfg.id = 4; cfg.color = dsFPD_COLOR_ORANGE; colors.push_back(cfg);
            cfg.id = 5; cfg.color = dsFPD_COLOR_WHITE; colors.push_back(cfg);
        }

        {
            FPDIndicatorConfig cfg;
            cfg.maxBrightness = kMaxBrightness;
            cfg.maxCycleRate = kMaxCycleRate;
            cfg.minBrightness = kMinBrightness;
            cfg.levels = kDefaultLevels;
            cfg.colorMode = kDefaultColorMode;

            cfg.id = dsFPD_INDICATOR_MESSAGE; indicators.push_back(cfg);
            cfg.id = dsFPD_INDICATOR_POWER; indicators.push_back(cfg);
            cfg.id = dsFPD_INDICATOR_RECORD; indicators.push_back(cfg);
            cfg.id = dsFPD_INDICATOR_REMOTE; indicators.push_back(cfg);
        }

        for (const auto& indicatorCfg : indicators) {
            for (const auto& colorCfg : colors) {
                FPDColorBinding mapEntry;
                mapEntry.targetType = DeviceSettingsFPD::DS_FPD_COLOR_TARGET_INDICATOR;
                mapEntry.targetId = indicatorCfg.id;
                mapEntry.colorId = colorCfg.id;
                colorBindings.push_back(mapEntry);
            }
        }

        {
            FPDTextDisplayConfig cfg;
            cfg.id = dsFPD_TEXTDISP_TEXT;
            cfg.name = "Text";
            cfg.maxBrightness = kMaxBrightness;
            cfg.maxCycleRate = kMaxCycleRate;
            cfg.supportedCharacters = kDefaultSupportedCharacters;
            cfg.columns = kMaxHorizontalColumns;
            cfg.rows = kMaxVerticalRows;
            cfg.maxHorizontalIterations = kMaxHorizontalIterations;
            cfg.maxVerticalIterations = kMaxVerticalIterations;
            cfg.levels = kDefaultLevels;
            cfg.colorMode = kDefaultColorMode;
            textDisplays.push_back(cfg);

            for (const auto& colorCfg : colors) {
                FPDColorBinding mapEntry;
                mapEntry.targetType = DeviceSettingsFPD::DS_FPD_COLOR_TARGET_TEXTDISPLAY;
                mapEntry.targetId = cfg.id;
                mapEntry.colorId = colorCfg.id;
                colorBindings.push_back(mapEntry);
            }
        }
    }
}

using namespace std;

namespace WPEFramework {
namespace Plugin {

    //SERVICE_REGISTRATION(DeviceSettingsFPDImpl, 1, 0);

    DeviceSettingsFPDImpl::DeviceSettingsFPDImpl()
        : _fpd(FPD::Create(*this))
    {
        LOGINFO("DeviceSettingsFPDImpl Constructor - Instance Address: %p", this);
    }

    DeviceSettingsFPDImpl::~DeviceSettingsFPDImpl() {
        LOGINFO("DeviceSettingsFPDImpl Destructor - Instance Address: %p", this);
    }


    template<typename Func, typename... Args>
    void DeviceSettingsFPDImpl::dispatchFPDEvent(Func notifyFunc, Args&&... args) {
        LOGINFO(">>");
        _callbackLock.Lock();
        for (auto& notification : _FPDNotifications) {
            auto start = std::chrono::steady_clock::now();
            (notification->*notifyFunc)(std::forward<Args>(args)...);
            auto elapsed = std::chrono::steady_clock::now() - start;
            LOGINFO("client %p took %" PRId64 "ms to process IFPD event", notification, std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        }
        _callbackLock.Unlock();
        LOGINFO("<<");
    }

    template <typename T>
    Core::hresult DeviceSettingsFPDImpl::Register(std::list<T*>& list, T* notification)
    {
        uint32_t status = Core::ERROR_GENERAL;
        ASSERT(nullptr != notification);

        _callbackLock.Lock();
        // Make sure we can't register the same notification callback multiple times
        if (std::find(list.begin(), list.end(), notification) == list.end()) {
            list.push_back(notification);
            notification->AddRef();
            status = Core::ERROR_NONE;
        } else {
            LOGWARN("Notification %p already registered - skipping", notification);
        }
        _callbackLock.Unlock();

        return status;
    }

    template <typename T>
    Core::hresult DeviceSettingsFPDImpl::Unregister(std::list<T*>& list, const T* notification)
    {
        uint32_t status = Core::ERROR_GENERAL;
        ASSERT(nullptr != notification);
        _callbackLock.Lock();

        // Make sure we can't unregister the same notification callback multiple times
        auto itr = std::find(list.begin(), list.end(), notification);
        if (itr != list.end()) {
            (*itr)->Release();
            list.erase(itr);
            status = Core::ERROR_NONE;
        }

        _callbackLock.Unlock();
        return status;
    }

    Core::hresult DeviceSettingsFPDImpl::Register(DeviceSettingsFPD::INotification* notification)
    {
        Core::hresult errorCode = Register(_FPDNotifications, notification);
        if (errorCode != Core::ERROR_NONE) {
            LOGERR("IFPD %p, errorCode: %u", notification, errorCode);
        } else {
            LOGINFO("IFPD %p registered successfully", notification);
        }
        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::Unregister(DeviceSettingsFPD::INotification* notification)
    {
        Core::hresult errorCode = Unregister(_FPDNotifications, notification);
        if (errorCode != Core::ERROR_NONE) {
            LOGERR("IFPD %p, errorcode: %u", notification, errorCode);
        } else {
            LOGINFO("IFPD %p unregistered successfully", notification);
        }
        return errorCode;
    }

    // FPD notification implementation
    void DeviceSettingsFPDImpl::OnFPDTimeFormatChanged(const FPDTimeFormat timeFormat)
    {
        LOGINFO("OnFPDTimeFormatChanged event Received: timeFormat=%d", timeFormat);
        dispatchFPDEvent(&DeviceSettingsFPD::INotification::OnFPDTimeFormatChanged, timeFormat);
    }

    //Depricated
    Core::hresult DeviceSettingsFPDImpl::SetFPDTime(const FPDTimeFormat timeFormat, const uint32_t minutes, const uint32_t seconds) {
        LOGINFO("SetFPDTime: timeFormat=%d, minutes=%u, seconds=%u", timeFormat, minutes, seconds);
        LOGINFO("SetFPDTime: SUCCESS - stub implementation completed");
        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::SetFPDScroll(const uint32_t scrollHoldDuration, const uint32_t nHorizontalScrollIterations, const uint32_t nVerticalScrollIterations) {
        LOGINFO("SetFPDScroll: scrollHoldDuration=%u, horizontal=%u, vertical=%u", scrollHoldDuration, nHorizontalScrollIterations, nVerticalScrollIterations);
        LOGINFO("SetFPDScroll: SUCCESS - stub implementation completed");
        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::SetFPDTextBrightness(const FPDTextDisplay textDisplay, const uint32_t brightNess) {
        LOGINFO("SetFPDTextBrightness: textDisplay=%d, brightNess=%u", textDisplay, brightNess);
        Core::hresult errorCode = Core::ERROR_GENERAL;
        if (_fpd.SetFPDTextBrightness(textDisplay, brightNess) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("SetFPDTextBrightness: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("SetFPDTextBrightness: SUCCESS - platform call completed");
        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::GetFPDTextBrightness(const FPDTextDisplay textDisplay, uint32_t &brightNess) {
        Core::hresult errorCode = Core::ERROR_GENERAL;
        if (_fpd.GetFPDTextBrightness(textDisplay, brightNess) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetFPDTextBrightness: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetFPDTextBrightness: SUCCESS - textDisplay=%d, brightNess=%d", textDisplay, brightNess);
        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::EnableFPDClockDisplay(const bool enable) {
        Core::hresult errorCode = Core::ERROR_GENERAL;
        if (_fpd.EnableFPDClockDisplay(enable) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("EnableFPDClockDisplay: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("EnableFPDClockDisplay: enable=%s", enable ? "true" : "false");
        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::GetFPDTimeFormat(FPDTimeFormat &fpdTimeFormat) {
        Core::hresult errorCode = Core::ERROR_GENERAL;
        if (_fpd.GetFPDTimeFormat(fpdTimeFormat) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetFPDTimeFormat: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetFPDTimeFormat: SUCCESS - fpdTimeFormat=%d", fpdTimeFormat);
        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::SetFPDTimeFormat(const FPDTimeFormat fpdTimeFormat) {
        Core::hresult errorCode = Core::ERROR_GENERAL;
        if (_fpd.SetFPDTimeFormat(fpdTimeFormat) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("SetFPDTimeFormat: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("SetFPDTimeFormat: fpdTimeFormat=%d", fpdTimeFormat);
        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::SetFPDBlink(const FPDIndicator indicator, const uint32_t blinkDuration, const uint32_t blinkIterations) {
        Core::hresult errorCode = Core::ERROR_GENERAL;
        if (_fpd.SetFPDBlink(indicator, blinkDuration, blinkIterations) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("SetFPDBlink: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("SetFPDBlink: indicator=%d, blinkDuration=%u, blinkIterations=%u", indicator, blinkDuration, blinkIterations);
        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::SetFPDMode(const FPDMode fpdMode) {
        Core::hresult errorCode = Core::ERROR_GENERAL;
        if (_fpd.SetFPDMode(fpdMode) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("SetFPDMode: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("SetFPDMode: fpdMode=%d", fpdMode);
        return errorCode;
    }
    //Depricated

    Core::hresult DeviceSettingsFPDImpl::SetFPDBrightness(const FPDIndicator indicator, const uint32_t brightNess, const bool persist) {
        LOGINFO("SetFPDBrightness: indicator=%d, brightNess=%u, persist=%s", indicator, brightNess, persist ? "true" : "false");

        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_fpd.SetFPDBrightness(indicator, brightNess, persist) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("SetFPDBrightness: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("SetFPDBrightness: SUCCESS - platform call completed");

        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::GetFPDBrightness(const FPDIndicator indicator, uint32_t &brightNess) {

        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_fpd.GetFPDBrightness(indicator, brightNess) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetFPDBrightness: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetFPDBrightness: SUCCESS - indicator=%d, brightNess=%d", indicator, brightNess);
        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::SetFPDState(const FPDIndicator indicator, const FPDState state) {
        LOGINFO("SetFPDState: indicator=%d, state=%d", indicator, state);

        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_fpd.SetFPDState(indicator, state) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("SetFPDState: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("SetFPDState: SUCCESS - platform call completed");

        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::GetFPDState(const FPDIndicator indicator, FPDState &state) {

        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_fpd.GetFPDState(indicator, state) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetFPDState: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetFPDState: SUCCESS - indicator=%d, state=%d", indicator, state);
        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::GetFPDColor(const FPDIndicator indicator, uint32_t &color) {

        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_fpd.GetFPDColor(indicator, color) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("GetFPDColor: failed with errorCode=%u", errorCode);
            return errorCode;
        }

        LOGINFO("GetFPDColor: SUCCESS - indicator=%d, color=0x%X", indicator, color);
        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::SetFPDColor(const FPDIndicator indicator, const uint32_t color) {
        LOGINFO("SetFPDColor: indicator=%d, color=0x%X", indicator, color);

        Core::hresult errorCode = Core::ERROR_GENERAL;
        _apiLock.Lock();
        if (_fpd.SetFPDColor(indicator, color) == dsERR_NONE) {
            errorCode = Core::ERROR_NONE;
        } else {
            errorCode = Core::ERROR_GENERAL;
        }
        _apiLock.Unlock();

        if (errorCode != Core::ERROR_NONE) {
            LOGWARN("SetFPDColor: failed with errorCode=%u", errorCode);
            return errorCode;
        }
        
        LOGINFO("SetFPDColor: SUCCESS - platform call completed");

        return errorCode;
    }

    Core::hresult DeviceSettingsFPDImpl::GetFrontPanelConfig(IFPDTextDisplayConfigIterator*& textDisplays, IFPDIndicatorConfigIterator*& indicators, IFPDColorConfigIterator*& colors, IFPDColorBindingIterator*& colorBindings)
    {
        std::vector<FPDColorConfig> colorConfigs;
        std::vector<FPDIndicatorConfig> indicatorConfigs;
        std::vector<FPDTextDisplayConfig> textDisplayConfigs;
        std::vector<FPDColorBinding> colorBindingConfigs;

        PopulateFrontPanelConfig(colorConfigs, indicatorConfigs, textDisplayConfigs, colorBindingConfigs);

        using ColorIterator = RPC::IteratorType<IFPDColorConfigIterator>;
        using IndicatorIterator = RPC::IteratorType<IFPDIndicatorConfigIterator>;
        using TextDisplayIterator = RPC::IteratorType<IFPDTextDisplayConfigIterator>;
        using ColorBindingIterator = RPC::IteratorType<IFPDColorBindingIterator>;

        colors = Core::Service<ColorIterator>::Create<IFPDColorConfigIterator>(colorConfigs);
        indicators = Core::Service<IndicatorIterator>::Create<IFPDIndicatorConfigIterator>(indicatorConfigs);
        textDisplays = Core::Service<TextDisplayIterator>::Create<IFPDTextDisplayConfigIterator>(textDisplayConfigs);
        colorBindings = Core::Service<ColorBindingIterator>::Create<IFPDColorBindingIterator>(colorBindingConfigs);

        LOGINFO("GetFrontPanelConfig: colors=%zu indicators=%zu textDisplays=%zu colorBindings=%zu", colorConfigs.size(), indicatorConfigs.size(), textDisplayConfigs.size(), colorBindingConfigs.size());
        return Core::ERROR_NONE;
    }

} // namespace Plugin
} // namespace WPEFramework
