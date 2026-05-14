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

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <core/Portability.h>
#include <core/Proxy.h>
#include <core/Trace.h>

#include "UtilsLogging.h"
#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>

#include <interfaces/IDeviceSettingsFPD.h>

// #include "dsMgr.h" // Removed - dsMgr functionality moved to DSController
#include "dsUtl.h"
#include "dsError.h"
#include "dsDisplay.h"
#include "dsRpc.h"
#include "dsFPDTypes.h"

#include "hal/dFPD.h"
#include "hal/dFPDImpl.h"
#include "DeviceSettingsTypes.h"

class FPD {
    using IPlatform = hal::dFPD::IPlatform;
    using DefaultImpl = dFPDImpl;

    std::shared_ptr<IPlatform> _platform;

public:
    class INotification {

        public:
            virtual ~INotification() = default;
            virtual void OnFPDTimeFormatChanged(const FPDTimeFormat timeFormat) = 0;
    };

public:

    void Platform_init();

    uint32_t SetFPDTime(const FPDTimeFormat timeFormat, const uint32_t minutes, const uint32_t seconds);
    uint32_t SetFPDScroll(const uint32_t scrollHoldDuration, const uint32_t nHorizontalScrollIterations, const uint32_t nVerticalScrollIterations);
    uint32_t SetFPDBlink(const FPDIndicator indicator, const uint32_t blinkDuration, const uint32_t blinkIterations);
    uint32_t SetFPDBrightness(const FPDIndicator indicator, const uint32_t brightNess, const bool persist);
    uint32_t GetFPDBrightness(const FPDIndicator indicator, uint32_t &brightNess);
    uint32_t SetFPDState(const FPDIndicator indicator, const FPDState state);
    uint32_t GetFPDState(const FPDIndicator indicator, FPDState &state);
    uint32_t GetFPDColor(const FPDIndicator indicator, uint32_t &color);
    uint32_t SetFPDColor(const FPDIndicator indicator, const uint32_t color);
    uint32_t SetFPDTextBrightness(const FPDTextDisplay textDisplay, const uint32_t brightNess);
    uint32_t GetFPDTextBrightness(const FPDTextDisplay textDisplay, uint32_t &brightNess);
    uint32_t EnableFPDClockDisplay(const bool enable);
    uint32_t GetFPDTimeFormat(FPDTimeFormat &fpdTimeFormat);
    uint32_t SetFPDTimeFormat(const FPDTimeFormat fpdTimeFormat);
    uint32_t SetFPDMode(const FPDMode fpdMode);

    template <typename IMPL = DefaultImpl, typename... Args>
    static FPD Create(INotification& parent, Args&&... args)
    {
        ENTRY_LOG;
        static_assert(std::is_base_of<IPlatform, IMPL>::value, "Impl must derive from hal::dFPD::IPlatform");
        auto impl = std::shared_ptr<IMPL>(new IMPL(std::forward<Args>(args)...));
        ASSERT(impl != nullptr);
        EXIT_LOG;
        return FPD(parent, std::move(impl));
    }
    
    private:
    FPD(INotification& parent, std::shared_ptr<IPlatform> platform);

    inline IPlatform& platform() const
    {
        return *_platform;
    }

    INotification& _parent;
};
