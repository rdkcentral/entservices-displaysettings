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
#pragma once

#include "dsHost.h"
#include "dsError.h"
#include "dsUtl.h"
#include "dsTypes.h"

#include "rfcapi.h"

#include <core/Portability.h>
#include <WPEFramework/interfaces/IDeviceSettingsHost.h>
#include "DeviceSettingsTypes.h"

#include "UtilsLogging.h"
#include <cstdint>

namespace hal {
namespace dHost {

    class IPlatform {

    public:
        virtual ~IPlatform() {} 
        void InitialiseHAL();
        void DeInitialiseHAL();
        virtual void setAllCallbacks(const CallbackBundle& bundle) = 0;
        virtual void getPersistenceValue() = 0;

        // Host Platform interface methods - all pure virtual
        virtual uint32_t GetPreferredSleepMode(HostSleepMode &mode) = 0;
        virtual uint32_t SetPreferredSleepMode(const HostSleepMode mode) = 0;
        virtual uint32_t GetCPUTemperature(float &temperature) = 0;
        virtual uint32_t GetHALVersion(uint32_t &versionNo) = 0;
        virtual uint32_t GetSoCID(string &socID) = 0;
        virtual uint32_t GetEDID(uint8_t edId[], const uint16_t edIdLength) = 0;
        virtual uint32_t GetMS12ConfigType(string &ms12Config) = 0;

    };
} // namespace dHost
} // namespace hal