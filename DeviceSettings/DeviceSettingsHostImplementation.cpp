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

#include "DeviceSettingsHostImplementation.h"

#include "UtilsLogging.h"
#include <syscall.h>
#include <chrono>

using namespace std;

namespace WPEFramework {
namespace Plugin {

    //SERVICE_REGISTRATION(DeviceSettingsHostImpl, 1, 0);

    DeviceSettingsHostImpl::DeviceSettingsHostImpl() : 
        _HostNotifications(),
        _apiLock(),
        _callbackLock(),
        _host(Host::Create(*this))
    {
        LOGINFO("DeviceSettingsHostImpl Constructor - Instance Address: %p", this);
    }

    DeviceSettingsHostImpl::~DeviceSettingsHostImpl() {
        LOGINFO("DeviceSettingsHostImpl Destructor - Instance Address: %p", this);
    }

    template<typename Func, typename... Args>
    void DeviceSettingsHostImpl::dispatchHostEvent(Func notifyFunc, Args&&... args) {
        LOGINFO(">>");
        _callbackLock.Lock();
        for (auto& notification : _HostNotifications) {
            auto start = std::chrono::steady_clock::now();
            (notification->*notifyFunc)(std::forward<Args>(args)...);
            auto elapsed = std::chrono::steady_clock::now() - start;
            LOGINFO("client %p took %" PRId64 "ms to process IHost event", notification, std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        }
        _callbackLock.Unlock();
        LOGINFO("<<");
    }

    template <typename T>
    Core::hresult DeviceSettingsHostImpl::Register(std::list<T*>& list, T* notification)
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
    Core::hresult DeviceSettingsHostImpl::Unregister(std::list<T*>& list, const T* notification)
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

    Core::hresult DeviceSettingsHostImpl::Register(Exchange::IDeviceSettingsHost::INotification* notification)
    {
        Core::hresult errorCode = Register(_HostNotifications, notification);
        if (errorCode != Core::ERROR_NONE) {
            LOGERR("IHost %p, errorCode: %u", notification, errorCode);
        } else {
            LOGINFO("IHost %p registered successfully", notification);
        }
        return errorCode;
    }

    Core::hresult DeviceSettingsHostImpl::Unregister(Exchange::IDeviceSettingsHost::INotification* notification)
    {
        Core::hresult errorCode = Unregister(_HostNotifications, notification);
        if (errorCode != Core::ERROR_NONE) {
            LOGERR("IHost %p, errorcode: %u", notification, errorCode);
        } else {
            LOGINFO("IHost %p unregistered successfully", notification);
        }
        return errorCode;
    }

// Host::INotification interface implementations (called by DS HAL)
    void DeviceSettingsHostImpl::OnSleepModeChanged(const HostSleepMode sleepMode)
    {
        LOGINFO("DS HAL OnSleepModeChanged event: sleepMode=%d", static_cast<int>(sleepMode));
        dispatchHostEvent(&Exchange::IDeviceSettingsHost::INotification::OnSleepModeChanged, static_cast<Exchange::IDeviceSettingsHost::SleepMode>(sleepMode));
    }

// Host interface method implementations called by DeviceSettingsImp 
    Core::hresult DeviceSettingsHostImpl::GetPreferredSleepMode(HostSleepMode &mode)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _host.GetPreferredSleepMode(mode);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetPreferredSleepMode succeeded: mode=%d", static_cast<int>(mode));
        } else {
            LOGERR("GetPreferredSleepMode failed: error=%u", result);
        }
        return result;
    }

    Core::hresult DeviceSettingsHostImpl::SetPreferredSleepMode(const HostSleepMode mode)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _host.SetPreferredSleepMode(mode);
        if (result == Core::ERROR_NONE) {
            LOGINFO("SetPreferredSleepMode succeeded for mode: %d", static_cast<int>(mode));
        } else {
            LOGERR("SetPreferredSleepMode failed for mode: %d, error: %u", static_cast<int>(mode), result);
        }
        return result;
    }

    Core::hresult DeviceSettingsHostImpl::GetCPUTemperature(float &temperature)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _host.GetCPUTemperature(temperature);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetCPUTemperature succeeded: temperature=%.2fC", temperature);
        } else {
            LOGERR("GetCPUTemperature failed: error=%u", result);
        }
        return result;
    }

    Core::hresult DeviceSettingsHostImpl::GetHALVersion(uint32_t &versionNo)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _host.GetHALVersion(versionNo);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetHALVersion succeeded: version=0x%x", versionNo);
        } else {
            LOGERR("GetHALVersion failed: error=%u", result);
        }
        return result;
    }

    Core::hresult DeviceSettingsHostImpl::GetSoCID(string &socID)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _host.GetSoCID(socID);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetSoCID succeeded: socID='%s'", socID.c_str());
        } else {
            LOGERR("GetSoCID failed: error=%u", result);
        }
        return result;
    }

    Core::hresult DeviceSettingsHostImpl::GetEDID(uint8_t edId[], const uint16_t edIdLength)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _host.GetEDID(edId, edIdLength);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetEDID succeeded: edIdLength=%u", edIdLength);
        } else {
            LOGERR("GetEDID failed: edIdLength=%u, error=%u", edIdLength, result);
        }
        return result;
    }

    Core::hresult DeviceSettingsHostImpl::GetMS12ConfigType(string &ms12Config)
    {
        uint32_t result = Core::ERROR_GENERAL;
        result = _host.GetMS12ConfigType(ms12Config);
        if (result == Core::ERROR_NONE) {
            LOGINFO("GetMS12ConfigType succeeded: ms12Config='%s'", ms12Config.c_str());
        } else {
            LOGERR("GetMS12ConfigType failed: error=%u", result);
        }
        return result;
    }

} // namespace Plugin
} // namespace WPEFramework