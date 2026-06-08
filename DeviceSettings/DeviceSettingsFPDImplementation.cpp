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

#include "DeviceSettingsHALConfig.h"

using namespace std;

namespace WPEFramework {
namespace Plugin {

    //SERVICE_REGISTRATION(DeviceSettingsFPDImpl, 1, 0);

    DeviceSettingsFPDImpl::DeviceSettingsFPDImpl()
        : _fpd(FPD::Create(*this))
    {
        InitializeFrontPanelConfigCache();
        LOGINFO("DeviceSettingsFPDImpl Constructor - Instance Address: %p", this);
    }

    DeviceSettingsFPDImpl::~DeviceSettingsFPDImpl() {
        LOGINFO("DeviceSettingsFPDImpl Destructor - Instance Address: %p", this);
    }

    void DeviceSettingsFPDImpl::InitializeFrontPanelConfigCache()
    {
        _apiLock.Lock();
        DeviceSettingsHAL::PopulateFPDConfig(_cachedColorConfigs, _cachedIndicatorConfigs, _cachedTextDisplayConfigs, _cachedColorBindingConfigs);
        DeviceSettingsHAL::DumpFPDConfig(_cachedColorConfigs, _cachedIndicatorConfigs, _cachedTextDisplayConfigs, _cachedColorBindingConfigs);
        _apiLock.Unlock();

        LOGINFO("InitializeFrontPanelConfigCache: colors=%zu indicators=%zu textDisplays=%zu colorBindings=%zu",
            _cachedColorConfigs.size(), _cachedIndicatorConfigs.size(), _cachedTextDisplayConfigs.size(), _cachedColorBindingConfigs.size());
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

        _apiLock.Lock();
        colorConfigs = _cachedColorConfigs;
        indicatorConfigs = _cachedIndicatorConfigs;
        textDisplayConfigs = _cachedTextDisplayConfigs;
        colorBindingConfigs = _cachedColorBindingConfigs;
        _apiLock.Unlock();

        DeviceSettingsHAL::DumpFPDConfig(colorConfigs, indicatorConfigs, textDisplayConfigs, colorBindingConfigs);

        using ColorIterator = RPC::IteratorType<IFPDColorConfigIterator>;
        using IndicatorIterator = RPC::IteratorType<IFPDIndicatorConfigIterator>;
        using TextDisplayIterator = RPC::IteratorType<IFPDTextDisplayConfigIterator>;
        using ColorBindingIterator = RPC::IteratorType<IFPDColorBindingIterator>;

        colors = Core::Service<ColorIterator>::Create<IFPDColorConfigIterator>(colorConfigs);
        indicators = Core::Service<IndicatorIterator>::Create<IFPDIndicatorConfigIterator>(indicatorConfigs);
        textDisplays = Core::Service<TextDisplayIterator>::Create<IFPDTextDisplayConfigIterator>(textDisplayConfigs);
        colorBindings = Core::Service<ColorBindingIterator>::Create<IFPDColorBindingIterator>(colorBindingConfigs);

        LOGINFO("GetFrontPanelConfig: returning cached config colors=%zu indicators=%zu textDisplays=%zu colorBindings=%zu", colorConfigs.size(), indicatorConfigs.size(), textDisplayConfigs.size(), colorBindingConfigs.size());
        return Core::ERROR_NONE;
    }

} // namespace Plugin
} // namespace WPEFramework
