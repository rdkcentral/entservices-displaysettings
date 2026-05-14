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

#pragma once

#include <string>
#include <mutex>
#include <interfaces/IPowerManager.h>
#include "PowerManagerInterface.h"
#include "mfrMgr.h"
#include "DeviceSettingsImplementation.h"

// C headers with built-in C++ protection
#include "mfrTypes.h"
#include "libIBus.h"
#include "libIBusDaemon.h"
//Need to remove this once fix the issue while add DevisettingsTypes.h file
#ifdef DEBUG_LOGGING
#define ENTRY_LOG do { LOGINFO("%d: Enter %s", __LINE__, __func__); } while(0);
#define EXIT_LOG do { LOGINFO("%d: Exit %s", __LINE__, __func__); } while(0);
#else
#define ENTRY_LOG do { } while(0)
#define EXIT_LOG do { } while(0)
#endif


using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;

namespace WPEFramework {
namespace Plugin {
namespace DSProductTraits {

typedef enum {
    DEFAULT_STB_PROFILE = 0,
    DEFAULT_TV_PROFILE,
    DEFAULT_STB_PROFILE_EUROPE,
    DEFAULT_TV_PROFILE_EUROPE,
    PROFILE_MAX
} productProfileId_t;

typedef enum {
    DEVICE_TYPE_STB = 0,
    DEVICE_TYPE_TV,
    DEVICE_TYPE_MAX
} deviceType_t;

typedef enum {
    POWER_MODE_ON = 0,
    POWER_MODE_LIGHT_SLEEP,
    POWER_MODE_LAST_KNOWN,
    POWER_MODE_UNSPECIFIED,
    POWER_MODE_MAX
} powerModeTrait_t;

enum class reboot_type_t { HARD, SOFT, UNAVAILABLE };

/*
 * UX Controller - User Experience Controller
 * Maintains and applies user experience attributes owned by power manager
 */
class UXController {
protected:
    unsigned int _id;
    std::string _name;
    deviceType_t _deviceType;
    bool _invalidateAsyncBootloaderPattern;
    bool _firstPowerTransitionComplete;
    mutable std::mutex _mutex;

    // DeviceSettings implementation for component access
    DeviceSettingsImp* _deviceSettings;

    bool _enableMultiColourLedSupport;
    bool _ledEnabledInStandby;
    int _ledColorInStandby;
    bool _ledEnabledInOnState;
    int _ledColorInOnState;

    powerModeTrait_t _preferedPowerModeOnReboot;
    bool _enableSilentRebootSupport;

    static UXController* _singleton;

    void InitializeSafeDefaults();
    void SyncPowerLedWithPowerState(PowerState state) const;
    void SyncDisplayPortsWithPowerState(PowerState state) const;
    bool SetBootloaderPattern(mfrBlPattern_t pattern) const;
    void SetBootloaderPatternAsync(mfrBlPattern_t pattern) const;
    bool SetBootloaderPatternInternal(mfrBlPattern_t pattern);
    bool SetBootloaderPatternFaultTolerant(mfrBlPattern_t pattern);

public:
    static bool Initialize(unsigned int profile_id);
    static UXController* GetInstance();
    
    UXController(unsigned int id, const std::string& name, deviceType_t deviceType);
    virtual ~UXController() {}

    virtual bool ApplyPowerStateChangeConfig(PowerState newState, 
                                            PowerState prevState) {
        return false;
    }
    
    virtual bool ApplyPreRebootConfig(PowerState currentState) const {
        return false;
    }
    
    virtual bool ApplyPreMaintenanceRebootConfig(PowerState currentState) {
        return false;
    }
    
    virtual bool ApplyPostRebootConfig(PowerState newState, 
                                      PowerState prevState) {
        return false;
    }
    
    virtual PowerState GetPreferredPostRebootPowerState(
        PowerState prevState) const {
        return prevState;
    }
    
    virtual void SyncDisplayPortsWithRebootReason(reboot_type_t type) {}
};

// TV Europe Profile
class UXControllerTvEu : public UXController {
public:
    UXControllerTvEu(unsigned int id, const std::string& name);
    virtual bool ApplyPowerStateChangeConfig(PowerState newState, 
                                            PowerState prevState) override;
    virtual bool ApplyPreRebootConfig(PowerState currentState) const override;
    virtual bool ApplyPreMaintenanceRebootConfig(PowerState currentState) override;
    virtual bool ApplyPostRebootConfig(PowerState newState, 
                                      PowerState prevState) override;
    virtual PowerState GetPreferredPostRebootPowerState(
        PowerState prevState) const override;
};

// STB Europe Profile
class UXControllerStbEu : public UXController {
public:
    UXControllerStbEu(unsigned int id, const std::string& name);
    virtual bool ApplyPowerStateChangeConfig(PowerState newState, 
                                            PowerState prevState) override;
    virtual bool ApplyPreRebootConfig(PowerState currentState) const override;
    virtual bool ApplyPreMaintenanceRebootConfig(PowerState currentState) override;
    virtual bool ApplyPostRebootConfig(PowerState newState, 
                                      PowerState prevState) override;
    virtual PowerState GetPreferredPostRebootPowerState(
        PowerState prevState) const override;
};

// TV Profile
class UXControllerTv : public UXController {
public:
    UXControllerTv(unsigned int id, const std::string& name);
    virtual bool ApplyPowerStateChangeConfig(PowerState newState, 
                                            PowerState prevState) override;
    virtual bool ApplyPreRebootConfig(PowerState currentState) const override;
    virtual bool ApplyPreMaintenanceRebootConfig(PowerState currentState) override;
    virtual bool ApplyPostRebootConfig(PowerState newState, 
                                      PowerState prevState) override;
    virtual PowerState GetPreferredPostRebootPowerState(
        PowerState prevState) const override;
    virtual void SyncDisplayPortsWithRebootReason(reboot_type_t type) override;
};

// STB Profile
class UXControllerStb : public UXController {
public:
    UXControllerStb(unsigned int id, const std::string& name);
    virtual bool ApplyPowerStateChangeConfig(PowerState newState, 
                                            PowerState prevState) override;
    virtual bool ApplyPreRebootConfig(PowerState currentState) const override;
    virtual bool ApplyPreMaintenanceRebootConfig(PowerState currentState) override;
    virtual bool ApplyPostRebootConfig(PowerState newState, 
                                      PowerState prevState) override;
    virtual PowerState GetPreferredPostRebootPowerState(
        PowerState prevState) const override;
};

} // namespace DSProductTraits
} // namespace Plugin
} // namespace WPEFramework
