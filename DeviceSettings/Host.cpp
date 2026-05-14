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

#include <functional>
#include <unistd.h>
#include <memory>

#include <core/IAction.h>
#include <core/Time.h>
#include <core/WorkerPool.h>

#include "UtilsLogging.h"
#include "secure_wrapper.h"
#include "Host.h"
#include "hal/dHostImpl.h"

Host::Host(INotification& parent, std::shared_ptr<IPlatform> platform)
    : _platform(std::move(platform))
    , _parent(parent)
{
    LOGINFO("Host Constructor");
    Platform_init();
}

Host Host::Create(INotification& parent) {
    return Host(parent, std::make_shared<dHostImpl>());
}

void Host::Platform_init()
{
    LOGINFO("Host Init - Setting up event callbacks");
    
    // Set up callback bundle for Host events - using global CallbackBundle pattern
    CallbackBundle bundle;
    
    bundle.OnSleepModeChanged = [this](const HostSleepMode sleepMode) {
        this->OnSleepModeChanged(sleepMode);
    };
    
    if (_platform) {
        // Use interface method directly - no casting needed
        this->platform().setAllCallbacks(bundle);
        this->platform().getPersistenceValue();
    }
    
}

uint32_t Host::GetPreferredSleepMode(HostSleepMode &mode) {
    LOGINFO("GetPreferredSleepMode");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetPreferredSleepMode(mode);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetPreferredSleepMode: SUCCESS - platform call completed successfully, mode=%d", static_cast<int>(mode));
    } else {
        LOGERR("GetPreferredSleepMode: FAILED - result=%u", result);
    }
    return result;
}

uint32_t Host::SetPreferredSleepMode(const HostSleepMode mode) {
    LOGINFO("SetPreferredSleepMode: mode=%d", static_cast<int>(mode));
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetPreferredSleepMode(mode);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetPreferredSleepMode: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetPreferredSleepMode: FAILED - result=%u", result);
    }
    return result;
}

uint32_t Host::GetCPUTemperature(float &temperature) {
    LOGINFO("GetCPUTemperature");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetCPUTemperature(temperature);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetCPUTemperature: SUCCESS - temperature=%.2fC", temperature);
    } else {
        LOGERR("GetCPUTemperature: FAILED - result=%u", result);
    }
    return result;
}

uint32_t Host::GetHALVersion(uint32_t &versionNo) {
    LOGINFO("GetHALVersion");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetHALVersion(versionNo);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetHALVersion: SUCCESS - version=0x%x", versionNo);
    } else {
        LOGERR("GetHALVersion: FAILED - result=%u", result);
    }
    return result;
}

uint32_t Host::GetSoCID(string &socID) {
    LOGINFO("GetSoCID");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetSoCID(socID);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetSoCID: SUCCESS - socID='%s'", socID.c_str());
    } else {
        LOGERR("GetSoCID: FAILED - result=%u", result);
    }
    return result;
}

uint32_t Host::GetEDID(uint8_t edId[], const uint16_t edIdLength) {
    LOGINFO("GetEDID: edIdLength=%u", edIdLength);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetEDID(edId, edIdLength);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetEDID: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("GetEDID: FAILED - result=%u", result);
    }
    return result;
}

uint32_t Host::GetMS12ConfigType(string &ms12Config) {
    LOGINFO("GetMS12ConfigType");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetMS12ConfigType(ms12Config);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetMS12ConfigType: SUCCESS - ms12Config='%s'", ms12Config.c_str());
    } else {
        LOGERR("GetMS12ConfigType: FAILED - result=%u", result);
    }
    return result;
}

// Host event handlers - called by DS HAL to forward events to parent
void Host::OnSleepModeChanged(const HostSleepMode sleepMode) {
    LOGINFO("DS HAL OnSleepModeChanged event: sleepMode=%d", static_cast<int>(sleepMode));
    _parent.OnSleepModeChanged(sleepMode);
}