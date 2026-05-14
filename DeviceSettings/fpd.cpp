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

#include <core/IAction.h>
#include <core/Time.h>
#include <core/WorkerPool.h>

#include "UtilsLogging.h"
#include "secure_wrapper.h"
#include "fpd.h"

FPD::FPD(INotification& parent, std::shared_ptr<IPlatform> platform)
    : _platform(std::move(platform))
    , _parent(parent)
{
    LOGINFO("FPD Constructor");
    Platform_init();
}

void FPD::Platform_init()
{
    // Initialize FPD platform
    LOGINFO("FPD Init");
}

//Depricated
uint32_t FPD::SetFPDTime(const FPDTimeFormat timeFormat, const uint32_t minutes, const uint32_t seconds) {
    LOGINFO("SetFPDTime: timeFormat=%d, minutes=%u, seconds=%u", timeFormat, minutes, seconds);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetFPDTime(timeFormat, minutes, seconds);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetFPDTime: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetFPDTime: FAILED - result=%u", result);
    }
    return result;
}

uint32_t FPD::SetFPDScroll(const uint32_t scrollHoldDuration, const uint32_t nHorizontalScrollIterations, const uint32_t nVerticalScrollIterations) {
    LOGINFO("SetFPDScroll: scrollHoldDuration=%u, horizontal=%u, vertical=%u", scrollHoldDuration, nHorizontalScrollIterations, nVerticalScrollIterations);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetFPDScroll(scrollHoldDuration, nHorizontalScrollIterations, nVerticalScrollIterations);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetFPDScroll: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetFPDScroll: FAILED - result=%u", result);
    }
    return result;
}

uint32_t FPD::SetFPDTextBrightness(const FPDTextDisplay textDisplay, const uint32_t brightNess) {
    LOGINFO("SetFPDTextBrightness: textDisplay=%d, brightNess=%u", textDisplay, brightNess);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetFPDTextBrightness(textDisplay, brightNess);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetFPDTextBrightness: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetFPDTextBrightness: FAILED - result=%u", result);
    }
    return result;
}

uint32_t FPD::GetFPDTextBrightness(const FPDTextDisplay textDisplay, uint32_t &brightNess) {
    LOGINFO("GetFPDTextBrightness: textDisplay=%d", textDisplay);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetFPDTextBrightness(textDisplay, brightNess);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetFPDTextBrightness: SUCCESS - textDisplay=%d, brightNess=%u", textDisplay, brightNess);
    } else {
        LOGERR("GetFPDTextBrightness: FAILED - result=%u", result);
    }
    return result;
}

uint32_t FPD::EnableFPDClockDisplay(const bool enable) {
    LOGINFO("EnableFPDClockDisplay: enable=%s", enable ? "true" : "false");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().EnableFPDClockDisplay(enable);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("EnableFPDClockDisplay: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("EnableFPDClockDisplay: FAILED - result=%u", result);
    }
    return result;
}

uint32_t FPD::GetFPDTimeFormat(FPDTimeFormat &fpdTimeFormat) {
    LOGINFO("GetFPDTimeFormat");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetFPDTimeFormat(fpdTimeFormat);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetFPDTimeFormat: SUCCESS - fpdTimeFormat=%d", fpdTimeFormat);
    } else {
        LOGERR("GetFPDTimeFormat: FAILED - result=%u", result);
    }
    return result;
}

uint32_t FPD::SetFPDTimeFormat(const FPDTimeFormat fpdTimeFormat) {
    LOGINFO("SetFPDTimeFormat: fpdTimeFormat=%d", fpdTimeFormat);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetFPDTimeFormat(fpdTimeFormat);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetFPDTimeFormat: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetFPDTimeFormat: FAILED - result=%u", result);
    }
    return result;
}
//Depricated

uint32_t FPD::SetFPDBlink(const FPDIndicator indicator, const uint32_t blinkDuration, const uint32_t blinkIterations) {

    LOGINFO("SetFPDBlink: indicator=%d, blinkDuration=%u, blinkIterations:%u", indicator, blinkDuration, blinkIterations);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetFPDBlink(indicator, blinkDuration, blinkIterations);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetFPDBlink: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetFPDBlink: FAILED - result=%u", result);
    }

    return result;
}

uint32_t FPD::GetFPDBrightness(const FPDIndicator indicator, uint32_t &brightNess) {

    LOGINFO("GetFPDBrightness: indicator=%d", indicator);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetFPDBrightness(indicator, brightNess);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetFPDBrightness: SUCCESS - indicator=%d, brightNess=%d", indicator, brightNess);
    } else {
        LOGERR("GetFPDBrightness: FAILED - result=%u", result);
    }

    return result;
}

uint32_t FPD::SetFPDBrightness(const FPDIndicator indicator, const uint32_t brightNess, const bool persist) {

    LOGINFO("SetFPDBrightness: indicator=%d, brightNess=%u, persist=%s", indicator, brightNess, persist ? "true" : "false");
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetFPDBrightness(indicator, brightNess, persist);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetFPDBrightness: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetFPDBrightness: FAILED - result=%u", result);
    }

    return result;
}

uint32_t FPD::GetFPDState(const FPDIndicator indicator, FPDState &state) {

    LOGINFO("GetFPDState: indicator=%d", indicator);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetFPDState(indicator, state);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetFPDState: SUCCESS - indicator=%d, state=%d", indicator, state);
    } else {
        LOGERR("GetFPDState: FAILED - result=%u", result);
    }

    return result;
}

uint32_t FPD::SetFPDState(const FPDIndicator indicator, const FPDState state) {

    LOGINFO("SetFPDState: indicator=%d, state=%d", indicator, state);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetFPDState(indicator, state);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetFPDState: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetFPDState: FAILED - result=%u", result);
    }

    return result;
}

uint32_t FPD::GetFPDColor(const FPDIndicator indicator, uint32_t &color) {

    LOGINFO("GetFPDColor: indicator=%d", indicator);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().GetFPDColor(indicator, color);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("GetFPDColor: SUCCESS - indicator=%d, colour=%d", indicator, color);
    } else {
        LOGERR("GetFPDColor: FAILED - result=%u", result);
    }
    return result;
}

uint32_t FPD::SetFPDColor(const FPDIndicator indicator, const uint32_t color) {

    LOGINFO("SetFPDColor: indicator=%d, colour=%d", indicator, color);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetFPDColor(indicator, color);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetFPDColor: SUCCESS - indicator=%d, colour=%d", indicator, color);
    } else {
        LOGERR("SetFPDColor: FAILED - indicator=%d, colour=%d, result=%u", indicator, color, result);
    }

    return result;
}

uint32_t FPD::SetFPDMode(const FPDMode fpdMode) {
    LOGINFO("SetFPDMode: fpdMode=%d", fpdMode);
    uint32_t result = WPEFramework::Core::ERROR_GENERAL;
    if (_platform) {
        result = this->platform().SetFPDMode(fpdMode);
    }
    if (result == WPEFramework::Core::ERROR_NONE) {
        LOGINFO("SetFPDMode: SUCCESS - platform call completed successfully");
    } else {
        LOGERR("SetFPDMode: FAILED - result=%u", result);
    }
    return result;
}
