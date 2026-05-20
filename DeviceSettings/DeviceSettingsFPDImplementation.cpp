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
#include <com/IteratorType.h>
#include <cstdlib>
#include <syscall.h>
#include <time.h>

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

    Core::hresult DeviceSettingsFPDImpl::GetFPDDeviceConfig(DeviceSettingsFPD::IFPDDeviceConfigIterator*& deviceConfig)
    {
        using DeviceConfigInfo = DeviceSettingsFPD::FPDDeviceConfigInfo;
        using DeviceConfigIterator = DeviceSettingsFPD::IFPDDeviceConfigIterator;
        using DeviceConfigIteratorImpl = RPC::IteratorType<DeviceConfigIterator>;

        auto randomU32 = [](const uint32_t minValue, const uint32_t maxValue) -> uint32_t {
            if (maxValue <= minValue) {
                return minValue;
            }
            const uint32_t span = maxValue - minValue + 1;
            return minValue + (static_cast<uint32_t>(std::rand()) % span);
        };

        auto randomLong = [](const long minValue, const long maxValue) -> long {
            if (maxValue <= minValue) {
                return minValue;
            }
            const long span = maxValue - minValue + 1;
            return minValue + (static_cast<long>(std::rand()) % span);
        };

        auto randomFloat = [](const float minValue, const float maxValue) -> float {
            if (maxValue <= minValue) {
                return minValue;
            }
            const float ratio = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
            return minValue + ((maxValue - minValue) * ratio);
        };

        auto randomDouble = [](const double minValue, const double maxValue) -> double {
            if (maxValue <= minValue) {
                return minValue;
            }
            const double ratio = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
            return minValue + ((maxValue - minValue) * ratio);
        };

        _apiLock.Lock();

        static bool isSeeded = false;
        if (isSeeded == false) {
            std::srand(static_cast<unsigned int>(time(nullptr)));
            isSeeded = true;
        }

        std::list<DeviceConfigInfo> configEntries;
        for (uint32_t i = 0; i < 3; ++i) {
            DeviceConfigInfo info;

            info.deviceId = 1000 + i;
            info.hardwareSku = randomU32(1, 128);
            info.firmwareBuild = randomU32(10000, 99999);
            info.capabilitiesMask = randomU32(1, 0xFFFF);
            info.panelBrightness = randomFloat(10.0f, 100.0f);
            info.ambientLightLevel = randomFloat(0.0f, 1000.0f);
            info.powerLimitPercent = randomFloat(50.0f, 100.0f);
            info.refreshRateHz = randomFloat(30.0f, 120.0f);
            info.boardTemperature = randomDouble(20.0, 85.0);
            info.inputVoltage = randomDouble(3.0, 12.0);
            info.totalPowerWatt = randomDouble(1.0, 15.0);
            info.uptimeSeconds = randomDouble(1000.0, 999999.0);
            info.bootEpoch = randomLong(1700000000L, 1900000000L);
            info.provisionEpoch = randomLong(1700000000L, 1900000000L);
            info.lastSyncEpoch = randomLong(1700000000L, 1900000000L);
            info.serialNumeric = randomLong(10000000L, 99999999L);
            info.deviceName = string("FPD_Device_") + std::to_string(i);
            info.version = string("v1.") + std::to_string(i);
            info.serialNumber = string("SN-") + std::to_string(randomU32(100000, 999999));
            info.manufacturer = "RDK";

            info.primaryGroup.groupId = i + 1;
            info.primaryGroup.elementCount = randomU32(1, 8);
            info.primaryGroup.activeCount = randomU32(1, 8);
            info.primaryGroup.priority = randomU32(1, 10);
            info.primaryGroup.groupBrightnessScale = randomFloat(0.1f, 2.0f);
            info.primaryGroup.powerBudgetPercent = randomFloat(10.0f, 100.0f);
            info.primaryGroup.heartbeatSec = randomFloat(0.1f, 5.0f);
            info.primaryGroup.failoverDelaySec = randomFloat(0.0f, 30.0f);
            info.primaryGroup.maxGroupPowerWatt = randomDouble(1.0, 20.0);
            info.primaryGroup.avgCurrentAmp = randomDouble(0.1, 2.5);
            info.primaryGroup.uptimeHours = randomDouble(1.0, 50000.0);
            info.primaryGroup.temperatureAvg = randomDouble(20.0, 90.0);
            info.primaryGroup.createdEpoch = randomLong(1700000000L, 1900000000L);
            info.primaryGroup.modifiedEpoch = randomLong(1700000000L, 1900000000L);
            info.primaryGroup.scheduleId = randomLong(1L, 1000L);
            info.primaryGroup.policyVersion = randomLong(1L, 100L);
            info.primaryGroup.groupName = string("Group_") + std::to_string(i);
            info.primaryGroup.policyName = "DefaultPolicy";
            info.primaryGroup.owner = "DeviceSettings";
            info.primaryGroup.comment = "Auto-generated sample group";

            info.primaryGroup.representativeElement.elementId = 200 + i;
            info.primaryGroup.representativeElement.gpioPin = randomU32(1, 64);
            info.primaryGroup.representativeElement.pwmChannel = randomU32(0, 8);
            info.primaryGroup.representativeElement.maxBrightness = randomU32(80, 100);
            info.primaryGroup.representativeElement.minBrightness = randomU32(0, 20);
            info.primaryGroup.representativeElement.defaultBrightness = randomFloat(1.0f, 100.0f);
            info.primaryGroup.representativeElement.blinkRateHz = randomFloat(0.5f, 10.0f);
            info.primaryGroup.representativeElement.thermalLimitC = randomFloat(50.0f, 95.0f);
            info.primaryGroup.representativeElement.updateIntervalSec = randomFloat(0.01f, 1.0f);
            info.primaryGroup.representativeElement.maxCurrentAmp = randomDouble(0.01, 1.50);
            info.primaryGroup.representativeElement.operatingVoltage = randomDouble(1.8, 5.0);
            info.primaryGroup.representativeElement.calibrationFactor = randomDouble(0.8, 1.2);
            info.primaryGroup.representativeElement.lifetimeHours = randomDouble(1000.0, 100000.0);
            info.primaryGroup.representativeElement.bootSequenceOrder = randomLong(1L, 10L);
            info.primaryGroup.representativeElement.lastFailureEpoch = randomLong(1700000000L, 1900000000L);
            info.primaryGroup.representativeElement.retryCount = randomLong(0L, 10L);
            info.primaryGroup.representativeElement.lockToken = randomLong(1L, 100000L);
            info.primaryGroup.representativeElement.elementName = string("Element_") + std::to_string(i);
            info.primaryGroup.representativeElement.elementType = "INDICATOR";
            info.primaryGroup.representativeElement.ownerModule = "DeviceSettingsFPD";
            info.primaryGroup.representativeElement.diagnostics = "Generated default diagnostics";

            info.primaryGroup.representativeElement.colorConfig.colorMode = randomU32(0, 1);
            info.primaryGroup.representativeElement.colorConfig.defaultColorId = randomU32(0, 8);
            info.primaryGroup.representativeElement.colorConfig.maxColorSlots = randomU32(1, 16);
            info.primaryGroup.representativeElement.colorConfig.transitionMode = randomU32(0, 4);
            info.primaryGroup.representativeElement.colorConfig.fadeInSec = randomFloat(0.0f, 3.0f);
            info.primaryGroup.representativeElement.colorConfig.fadeOutSec = randomFloat(0.0f, 3.0f);
            info.primaryGroup.representativeElement.colorConfig.blinkDutyCycle = randomFloat(0.1f, 1.0f);
            info.primaryGroup.representativeElement.colorConfig.brightnessScale = randomFloat(0.1f, 2.0f);
            info.primaryGroup.representativeElement.colorConfig.transitionCurveA = randomDouble(0.0, 10.0);
            info.primaryGroup.representativeElement.colorConfig.transitionCurveB = randomDouble(0.0, 10.0);
            info.primaryGroup.representativeElement.colorConfig.maxPowerWatt = randomDouble(0.1, 5.0);
            info.primaryGroup.representativeElement.colorConfig.voltageDrop = randomDouble(0.0, 1.0);
            info.primaryGroup.representativeElement.colorConfig.lastPersistEpoch = randomLong(1700000000L, 1900000000L);
            info.primaryGroup.representativeElement.colorConfig.configVersion = randomLong(1L, 1000L);
            info.primaryGroup.representativeElement.colorConfig.checksum = randomLong(1000L, 999999L);
            info.primaryGroup.representativeElement.colorConfig.migrationId = randomLong(1L, 10000L);
            info.primaryGroup.representativeElement.colorConfig.modeName = "DefaultMode";
            info.primaryGroup.representativeElement.colorConfig.fallbackPolicy = "KeepLastKnown";
            info.primaryGroup.representativeElement.colorConfig.colorSpace = "RGB";
            info.primaryGroup.representativeElement.colorConfig.transitionProfile = "Smooth";

            info.primaryGroup.representativeElement.colorConfig.primaryColor.colorId = randomU32(0, 8);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.paletteIndex = randomU32(0, 32);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.rgbPacked = randomU32(0x000000, 0xFFFFFF);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.hsvHue = randomU32(0, 360);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.hue = randomFloat(0.0f, 360.0f);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.saturation = randomFloat(0.0f, 1.0f);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.value = randomFloat(0.0f, 1.0f);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.alpha = randomFloat(0.0f, 1.0f);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.cieX = randomDouble(0.0, 1.0);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.cieY = randomDouble(0.0, 1.0);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.cieZ = randomDouble(0.0, 1.0);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.wavelengthNm = randomDouble(380.0, 780.0);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.creationEpoch = randomLong(1700000000L, 1900000000L);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.lastUpdateEpoch = randomLong(1700000000L, 1900000000L);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.usageCounter = randomLong(0L, 50000L);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.reservedLong = randomLong(0L, 1000L);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.colorName = "Blue";
            info.primaryGroup.representativeElement.colorConfig.primaryColor.colorFamily = "Primary";
            info.primaryGroup.representativeElement.colorConfig.primaryColor.vendorTag = "RDK";
            info.primaryGroup.representativeElement.colorConfig.primaryColor.metadata = "Generated color metadata";

            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.minValue = randomU32(0, 20);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.maxValue = randomU32(200, 255);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.stepSize = randomU32(1, 10);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.precision = randomU32(8, 16);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.gain = randomFloat(0.1f, 4.0f);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.offset = randomFloat(-10.0f, 10.0f);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.gamma = randomFloat(0.5f, 3.0f);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.noiseFloor = randomFloat(0.0f, 0.1f);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.calibrationSlope = randomDouble(0.5, 2.0);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.calibrationIntercept = randomDouble(-5.0, 5.0);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.referenceLux = randomDouble(10.0, 1000.0);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.thermalDrift = randomDouble(0.0, 0.2);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.sampleCount = randomLong(10L, 1000L);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.stableWindowMs = randomLong(10L, 5000L);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.warmupMs = randomLong(0L, 3000L);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.expiryEpoch = randomLong(1700000000L, 1900000000L);
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.profileName = "DefaultRangeProfile";
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.unit = "nits";
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.revision = "r1";
            info.primaryGroup.representativeElement.colorConfig.primaryColor.range.notes = "Generated range notes";

            configEntries.push_back(info);
        }

        deviceConfig = Core::Service<DeviceConfigIteratorImpl>::Create<DeviceConfigIterator>(std::move(configEntries));
        _apiLock.Unlock();

        if (deviceConfig == nullptr) {
            LOGERR("GetFPDDeviceConfig: iterator allocation failed");
            return Core::ERROR_GENERAL;
        }

        LOGINFO("GetFPDDeviceConfig: generated default/random sample data for iterator");
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
