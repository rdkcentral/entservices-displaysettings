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
        LOGINFO("SetFPDTextBrightness: SUCCESS - stub implementation completed");
        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::GetFPDTextBrightness(const FPDTextDisplay textDisplay, uint32_t &brightNess) {
        brightNess = 50; // Example value
        LOGINFO("GetFPDTextBrightness: SUCCESS - textDisplay=%d, brightNess=%d", textDisplay, brightNess);
        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::EnableFPDClockDisplay(const bool enable) {
        LOGINFO("EnableFPDClockDisplay: enable=%s", enable ? "true" : "false");
        LOGINFO("EnableFPDClockDisplay: SUCCESS - stub implementation completed");
        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::GetFPDTimeFormat(FPDTimeFormat &fpdTimeFormat) {
        fpdTimeFormat = FPDTimeFormat::DS_FPD_TIMEFORMAT_24_HOUR; // Example value
        LOGINFO("GetFPDTimeFormat: SUCCESS - fpdTimeFormat=%d", fpdTimeFormat);
        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::SetFPDTimeFormat(const FPDTimeFormat fpdTimeFormat) {
        LOGINFO("SetFPDTimeFormat: fpdTimeFormat=%d", fpdTimeFormat);
        LOGINFO("SetFPDTimeFormat: SUCCESS - stub implementation completed");
        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::SetFPDBlink(const FPDIndicator indicator, const uint32_t blinkDuration, const uint32_t blinkIterations) {
        LOGINFO("SetFPDBlink: indicator=%d, blinkDuration=%u, blinkIterations=%u", indicator, blinkDuration, blinkIterations);
        LOGINFO("SetFPDBlink: SUCCESS - stub implementation completed");
        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::SetFPDMode(const FPDMode fpdMode) {
        LOGINFO("SetFPDMode: fpdMode=%d", fpdMode);
        LOGINFO("SetFPDMode: SUCCESS - stub implementation completed");
        return Core::ERROR_NONE;
    }
    //Depricated

    Core::hresult DeviceSettingsFPDImpl::SetFPDBrightness(const FPDIndicator indicator, const uint32_t brightNess, const bool persist) {
        LOGINFO("SetFPDBrightness: indicator=%d, brightNess=%u, persist=%s", indicator, brightNess, persist ? "true" : "false");

        _apiLock.Lock();
        _fpd.SetFPDBrightness(indicator, brightNess, persist);
        _apiLock.Unlock();
        
        LOGINFO("SetFPDBrightness: SUCCESS - platform call completed");

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::GetFPDBrightness(const FPDIndicator indicator, uint32_t &brightNess) {

        _apiLock.Lock();
        _fpd.GetFPDBrightness(indicator, brightNess);
        _apiLock.Unlock();

        LOGINFO("GetFPDBrightness: SUCCESS - indicator=%d, brightNess=%d", indicator, brightNess);
        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::SetFPDState(const FPDIndicator indicator, const FPDState state) {
        LOGINFO("SetFPDState: indicator=%d, state=%d", indicator, state);

        _apiLock.Lock();
        _fpd.SetFPDState(indicator, state);
        _apiLock.Unlock();
        
        LOGINFO("SetFPDState: SUCCESS - platform call completed");

        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::GetFPDState(const FPDIndicator indicator, FPDState &state) {

        _apiLock.Lock();
        _fpd.GetFPDState(indicator, state);
        _apiLock.Unlock();

        LOGINFO("GetFPDState: SUCCESS - indicator=%d, state=%d", indicator, state);
        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::GetFPDColor(const FPDIndicator indicator, uint32_t &color) {

        _apiLock.Lock();
        _fpd.GetFPDColor(indicator, color);
        _apiLock.Unlock();

        LOGINFO("GetFPDColor: SUCCESS - indicator=%d, color=0x%X", indicator, color);
        return Core::ERROR_NONE;
    }

    Core::hresult DeviceSettingsFPDImpl::SetFPDColor(const FPDIndicator indicator, const uint32_t color) {
        LOGINFO("SetFPDColor: indicator=%d, color=0x%X", indicator, color);

        _apiLock.Lock();
        _fpd.SetFPDColor(indicator, color);
        _apiLock.Unlock();
        
        LOGINFO("SetFPDColor: SUCCESS - platform call completed");

        return Core::ERROR_NONE;
    }

} // namespace Plugin
} // namespace WPEFramework
