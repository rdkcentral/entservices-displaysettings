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

#include "dsHdmiIn.h"
#include "dsError.h"
#include "dsHdmiInTypes.h"
#include "dsUtl.h"
#include "dsTypes.h"

#include "rfcapi.h"

#include <core/Portability.h>
#include <WPEFramework/interfaces/IDeviceSettingsFPD.h>
#include "DeviceSettingsTypes.h"

#include "UtilsLogging.h"
#include <cstdint>

namespace hal {
namespace dFPD {

    class IPlatform {

    public:
        virtual ~IPlatform() {}
        void InitialiseHAL();
        void DeInitialiseHAL();

        // FPD Platform interface methods - all pure virtual
        virtual uint32_t SetFPDTime(const FPDTimeFormat timeFormat, const uint32_t minutes, const uint32_t seconds) = 0;
        virtual uint32_t SetFPDScroll(const uint32_t scrollHoldDuration, const uint32_t nHorizontalScrollIterations, const uint32_t nVerticalScrollIterations) = 0;
        virtual uint32_t SetFPDBlink(const FPDIndicator indicator, const uint32_t blinkDuration, const uint32_t blinkIterations) = 0;
        virtual uint32_t SetFPDBrightness(const FPDIndicator indicator , const uint32_t brightNess , const bool persist ) = 0;
        virtual uint32_t GetFPDBrightness(const FPDIndicator indicator, uint32_t &brightNess) = 0;
        virtual uint32_t SetFPDState(const FPDIndicator indicator, const FPDState state) = 0;
        virtual uint32_t GetFPDState(const FPDIndicator indicator, FPDState &state) = 0;
        virtual uint32_t GetFPDColor(const FPDIndicator indicator, uint32_t &color) = 0;
        virtual uint32_t SetFPDColor(const FPDIndicator indicator, const uint32_t color) = 0;
        virtual uint32_t SetFPDTextBrightness(const FPDTextDisplay textDisplay, const uint32_t brightNess) = 0;
        virtual uint32_t GetFPDTextBrightness(const FPDTextDisplay textDisplay, uint32_t &brightNess) = 0;
        virtual uint32_t EnableFPDClockDisplay(const bool enable) = 0;
        virtual uint32_t GetFPDTimeFormat(FPDTimeFormat &fpdTimeFormat) = 0;
        virtual uint32_t SetFPDTimeFormat(const FPDTimeFormat fpdTimeFormat) = 0;
        virtual uint32_t SetFPDMode(const FPDMode fpdMode) = 0;

    };
} // namespace dFPD
} // namespace hal

