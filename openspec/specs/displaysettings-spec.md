# DisplaySettings Plugin Specification

## 1. Overview

The DisplaySettings plugin provides comprehensive APIs for video display and audio output management on RDK-based devices. It exposes Thunder/JSON-RPC methods for:
- Video display configuration (resolutions, EDID, HDR)
- Audio port management (routing, enable/disable)
- Audio processing (MS12/Dolby enhancements, volume control)
- Multi-language audio and accessibility features

**Plugin Version:** 2.0.5

### Architecture (ASCII Diagram)

    +------------------------+
    |   Client (App/UI)      |
    +-----------+------------+
                |
      JSON-RPC (HTTP/WS)
                |
    +-----------v------------+
    |   Thunder Framework    |
    +-----------+------------+
                |
         Plugin Call
                |
    +-----------v-------------------+
    |   DisplaySettings Plugin      |
    |   ┌───────────┬──────────┐    |
    |   │  Video    │  Audio   │    |
    |   │  Control  │  Control │    |
    |   └─────┬─────┴────┬─────┘    |
    +---------│──────────│----------+
              │          │
        EDID/HDMI    IARM/CEC
              │          │
    +---------v----------v----------+
    | Device Host Abstraction       |
    |  • Video Output Ports         |
    |  • Audio Output Ports         |
    |  • MS12 Audio Decoder         |
    +---------+----------+----------+
              │          │
       Display HW   Audio HW
              │          │
    +---------v----+ +---v-----------+
    | HDMI/Panel   | | HDMI ARC/SPDIF|
    | TV Display   | | Speakers/Audio|
    +--------------+ +---------------+


## 2. Capabilities

### Video Display Control
- Query connected and supported video displays (HDMI, internal panels)
- Query resolutions from EDID or platform capabilities
- Get/set current resolution with persistence
- Read EDID data (raw and host)
- Manage zoom/aspect ratio settings
- Query and configure HDR support (HDR10, Dolby Vision, HLG)
- Control color depth (8/10/12-bit)
- Query video format details

### Audio Output Management
- Enumerate and control audio output ports (HDMI, ARC/eARC, SPDIF, Speakers, Headphones)
- Enable/disable audio ports with persistence
- Query supported audio modes and formats
- Manage HDMI ARC/eARC routing with CEC integration
- Query Atmos and audio decoder capabilities

### Audio Processing (MS12/Dolby)
- Volume control (level, mute, gain)
- Dynamic range compression (DRC)
- Bass enhancement and surround virtualization
- Dialog enhancement for speech clarity
- Intelligent and graphic equalizer modes
- Volume leveller for consistent loudness
- MS12 audio profiles and configuration
- MI steering for audio mixing

### Audio Routing & Accessibility
- Audio delay compensation
- Associated audio mixing for visually impaired users
- Fader control for audio mixing
- Primary and secondary language selection
- Atmos output mode configuration

---

## 3. APIs

### 3.1 Video Display APIs

| Method Name                   | Description                                      | Params                          | Returns                |
|-------------------------------|--------------------------------------------------|---------------------------------|------------------------|
| getConnectedVideoDisplays     | List connected video displays                    | —                               | Array of display names |
| getSupportedVideoDisplays     | List all supported video displays                | —                               | Array of display names |
| getSupportedResolutions       | List supported resolutions from platform capabilities | videoDisplay (opt)              | Array of resolutions   |
| getSupportedTvResolutions     | List TV-supported resolutions from platform      | videoDisplay (opt)              | Array of resolutions   |
| getSupportedSettopResolutions | List set-top box supported resolutions           | —                               | Array of resolutions   |
| getCurrentResolution          | Get current active resolution                    | videoDisplay (opt)              | Resolution object      |
| setCurrentResolution          | Set current resolution                           | videoDisplay, resolution, persist | Success status       |
| getDefaultResolution          | Get default resolution for a display             | videoDisplay (opt)              | Resolution string      |
| getZoomSetting                | Get current zoom/aspect ratio setting            | videoDisplay (opt)              | Zoom setting           |
| setZoomSetting                | Set zoom/aspect ratio                            | videoDisplay, zoomSetting       | Success status         |
| readEDID                      | Read EDID data from connected display            | videoDisplay (opt)              | EDID bytes             |
| readHostEDID                  | Read host EDID data                              | videoDisplay (opt)              | EDID bytes             |
| getActiveInput                | Get active video input                           | —                               | Active input name      |
| getVideoFormat                | Get current video format details                 | videoDisplay (opt)              | Format info            |

### 3.2 Audio Port Management APIs

| Method Name                   | Description                                      | Params                          | Returns                |
|-------------------------------|--------------------------------------------------|---------------------------------|------------------------|
| getConnectedAudioPorts        | List currently connected audio ports             | —                               | Array of port names    |
| getSupportedAudioPorts        | List all supported audio ports                   | —                               | Array of port names    |
| setEnableAudioPort            | Enable or disable an audio output port           | audioPort, enable               | Success status         |
| getEnableAudioPort            | Get enable/disable status of audio port          | audioPort                       | Enable status          |
| getSupportedAudioModes        | List supported audio modes                       | audioPort                       | Array of modes         |
| getAudioFormat                | Get current audio format                         | audioPort                       | Format info            |
| getSoundMode                  | Get current sound mode                           | audioPort (opt)                 | Sound mode             |
| setSoundMode                  | Set sound mode                                   | audioPort, soundMode, persist   | Success status         |

### 3.3 Audio Processing APIs (MS12/Dolby)

#### Volume & Dynamics Control

| Method Name                   | Description                                      | Params                          | Returns                |
|-------------------------------|--------------------------------------------------|---------------------------------|------------------------|
| setVolumeLevel                | Set volume level                                 | audioPort, volumeLevel          | Success status         |
| getVolumeLevel                | Get current volume level                         | audioPort                       | Volume level (0-100)   |
| setMuted                      | Mute or unmute audio                             | audioPort, muted                | Success status         |
| getMuted                      | Get mute status                                  | audioPort                       | Mute status (bool)     |
| setGain                       | Set audio gain                                   | audioPort, gain                 | Success status         |
| getGain                       | Get current audio gain                           | audioPort                       | Gain value (dB)        |
| setVolumeLeveller             | Set volume leveller (v1)                         | audioPort, level, mode          | Success status         |
| getVolumeLeveller             | Get volume leveller settings (v1)                | audioPort                       | Level and mode         |
| setVolumeLeveller2            | Set volume leveller (v2)                         | audioPort, level                | Success status         |
| getVolumeLeveller2            | Get volume leveller settings (v2)                | audioPort                       | Level value            |
| resetVolumeLeveller           | Reset volume leveller to default                 | audioPort                       | Success status         |
| setDRCMode                    | Set dynamic range compression mode               | audioPort, mode                 | Success status         |
| getDRCMode                    | Get DRC mode                                     | audioPort                       | DRC mode (line/RF)     |
| setMS12AudioCompression       | Set MS12 audio compression                       | audioPort, compressionlevel     | Success status         |
| getMS12AudioCompression       | Get MS12 audio compression level                 | audioPort                       | Compression level      |
| setDolbyVolumeMode            | Set Dolby volume mode                            | audioPort, mode                 | Success status         |
| getDolbyVolumeMode            | Get Dolby volume mode                            | audioPort                       | Volume mode            |

#### Audio Enhancement

| Method Name                   | Description                                      | Params                          | Returns                |
|-------------------------------|--------------------------------------------------|---------------------------------|------------------------|
| setBassEnhancer               | Set bass enhancement level                       | audioPort, boost                | Success status         |
| getBassEnhancer               | Get bass enhancement level                       | audioPort                       | Boost level (0-100)    |
| resetBassEnhancer             | Reset bass enhancer to default                   | audioPort                       | Success status         |
| setSurroundVirtualizer        | Set surround virtualization (v1)                 | audioPort, boost, mode          | Success status         |
| getSurroundVirtualizer        | Get surround virtualization (v1)                 | audioPort                       | Boost and mode         |
| setSurroundVirtualizer2       | Set surround virtualization (v2)                 | audioPort, boost                | Success status         |
| getSurroundVirtualizer2       | Get surround virtualization (v2)                 | audioPort                       | Boost value            |
| resetSurroundVirtualizer      | Reset surround virtualizer to default            | audioPort                       | Success status         |
| enableSurroundDecoder         | Enable/disable surround decoder                  | audioPort, surroundDecoderEnable| Success status         |
| isSurroundDecoderEnabled      | Check if surround decoder is enabled             | audioPort                       | Enable status (bool)   |
| setDialogEnhancement          | Set dialog enhancement level                     | audioPort, enhancerlevel        | Success status         |
| getDialogEnhancement          | Get dialog enhancement level                     | audioPort                       | Enhancement level      |
| resetDialogEnhancement        | Reset dialog enhancement to default              | audioPort                       | Success status         |
| setMISteering                 | Set MI steering                                  | audioPort, MISteeringenable     | Success status         |
| getMISteering                 | Get MI steering status                           | audioPort                       | MI steering status     |

#### Equalizer & Sound Profiles

| Method Name                   | Description                                      | Params                          | Returns                |
|-------------------------------|--------------------------------------------------|---------------------------------|------------------------|
| setIntelligentEqualizerMode   | Set intelligent equalizer mode                   | audioPort, mode                 | Success status         |
| getIntelligentEqualizerMode   | Get intelligent equalizer mode                   | audioPort                       | Equalizer mode         |
| setGraphicEqualizerMode       | Set graphic equalizer mode                       | audioPort, mode                 | Success status         |
| getGraphicEqualizerMode       | Get graphic equalizer mode                       | audioPort                       | Equalizer mode         |

#### MS12 Profiles & Configuration

| Method Name                   | Description                                      | Params                          | Returns                |
|-------------------------------|--------------------------------------------------|---------------------------------|------------------------|
| setMS12AudioProfile           | Set MS12 audio profile                           | audioPort, profile              | Success status         |
| getMS12AudioProfile           | Get current MS12 audio profile                   | audioPort                       | Profile name           |
| getSupportedMS12AudioProfiles | List supported MS12 audio profiles               | audioPort                       | Array of profiles      |
| setMS12ProfileSettingsOverride| Override MS12 profile settings                   | audioPort, profileState         | Success status         |
| getSupportedMS12Config        | Get supported MS12 configuration                 | audioPort                       | Config details         |
| getSettopMS12Capabilities     | Get set-top MS12 capabilities                    | audioPort                       | MS12 capabilities      |
| getSettopAudioCapabilities    | Get set-top audio capabilities                   | —                               | Audio capabilities     |

### 3.4 Audio Routing & Accessibility APIs

| Method Name                   | Description                                      | Params                          | Returns                |
|-------------------------------|--------------------------------------------------|---------------------------------|------------------------|
| setAudioDelay                 | Set audio delay compensation                     | audioPort, audioDelay           | Success status         |
| getAudioDelay                 | Get audio delay setting                          | audioPort                       | Delay in ms            |
| getSinkAtmosCapability        | Get sink Atmos capability                        | audioPort                       | Atmos support (bool)   |
| setAudioAtmosOutputMode       | Set Atmos output mode                            | audioPort, enable               | Success status         |
| setAssociatedAudioMixing      | Set associated audio mixing (visual impaired)    | audioPort, mixing               | Success status         |
| getAssociatedAudioMixing      | Get associated audio mixing status               | audioPort                       | Mixing status          |
| setFaderControl               | Set fader control for audio mixing               | audioPort, mixerLevel           | Success status         |
| getFaderControl               | Get fader control setting                        | audioPort                       | Mixer level            |
| setPrimaryLanguage            | Set primary audio language                       | audioPort, primaryLanguage      | Success status         |
| getPrimaryLanguage            | Get primary audio language                       | audioPort                       | Language code          |
| setSecondaryLanguage          | Set secondary audio language                     | audioPort, secondaryLanguage    | Success status         |
| getSecondaryLanguage          | Get secondary audio language                     | audioPort                       | Language code          |

### 3.5 Advanced Video & Color APIs

| Method Name                   | Description                                      | Params                          | Returns                |
|-------------------------------|--------------------------------------------------|---------------------------------|------------------------|
| setPreferredColorDepth        | Set preferred color depth                        | videoDisplay, colorDepth        | Success status         |
| getPreferredColorDepth        | Get preferred color depth                        | videoDisplay (opt)              | Color depth (bits)     |
| getColorDepthCapabilities     | Get supported color depth capabilities           | videoDisplay (opt)              | Array of depths        |
| setForceHDRMode               | Force specific HDR mode                          | videoDisplay, mode              | Success status         |
| getTvHDRSupport               | Get TV HDR support (legacy)                      | —                               | HDR support info       |
| getSettopHDRSupport           | Get set-top HDR support                          | —                               | HDR capabilities       |
| getTVHDRCapabilities          | Get TV HDR capabilities (detailed)               | videoDisplay (opt)              | HDR capabilities       |
| isConnectedDeviceRepeater     | Check if connected device is a repeater          | —                               | Repeater status (bool) |
| setScartParameter             | Set SCART parameter (legacy video)               | videoDisplay, parameter, value  | Success status         |

### 3.6 System & Output Settings APIs

| Method Name                   | Description                                      | Params                          | Returns                |
|-------------------------------|--------------------------------------------------|---------------------------------|------------------------|
| getCurrentOutputSettings      | Get all current output settings                  | —                               | Comprehensive settings |

---

## 4. Data Models

### Video Display Types
- **Resolution:** String format: "480i", "480p", "576i", "576p", "576p50", "720p", "720p50", "1080i", "1080p", "1080p24", "1080p25", "1080p30", "1080p50", "1080p60", "2160p24", "2160p25", "2160p30", "2160p50", "2160p60"
- **VideoDisplay:** Port names: "HDMI0", "HDMI1", "INTERNAL" (platform-specific)
- **ZoomSetting:** Values: "None", "Full", "Normal", "Zoom", "Stretch"
- **ColorDepth:** Values: "8", "10", "12", "auto" (bits per channel)
- **HDRFormat:** Values: "none", "HDR10", "HDR10PLUS", "DolbyVision", "HLG"

### Audio Port Types
- **AudioPort:** Port names: "HDMI0", "HDMI_ARC0", "SPDIF0", "SPEAKER0", "HEADPHONE0", "IDLR0"
- **AudioMode:** Values: "STEREO", "SURROUND", "PASSTHRU", "AUTO"
- **SoundMode:** Values: "STEREO", "MONO", "SURROUND", "AUTO"
- **AudioFormat:** Values: "PCM", "AC3", "EAC3", "AAC", "Dolby Digital Plus", "Dolby TrueHD", "Dolby Atmos"

### Audio Processing Values
- **DRCMode:** Values: "line" (light compression), "RF" (heavy compression)
- **VolumeLevel:** Integer 0-100
- **Gain:** Float in dB (typically -2080.0 to +480.0)
- **EnhancementLevel:** Integer 0-16 (for bass, dialog, etc.)
- **BoostLevel:** Integer 0-100 (for virtualizer, bass)
- **MixerLevel:** Integer 0-100 (for fader control)

### MS12 Data Types
- **MS12Profile:** Values: "Movie", "Music", "Game", "Sports", "Night", "User" (platform-dependent)
- **EqualizerMode:** Integer 0-N (platform-dependent mode IDs)
- **LanguageCode:** ISO 639-2 codes: "eng", "spa", "fra", etc.

### EDID Data
- **EDIDBytes:** Hexadecimal string representation of raw EDID data (128 or 256 bytes)
- **EDIDInfo:** Parsed structure containing manufacturer, model, capabilities

### HDR Capabilities
```json
{
  "supportsHDR": true,
  "capabilities": ["HDR10", "DolbyVision", "HLG"],
  "standards": {
    "HDR10": { "supported": true },
    "DolbyVision": { "supported": true },
    "HLG": { "supported": false }
  }
}
```

---

## 5. Behavior & Constraints

### Video Display Behavior
- **Resolution Query:**
  - Returns platform capabilities via device layer abstraction
  - For external displays (HDMI): platform capabilities may be influenced by display capabilities, but returns platform-supported resolutions
  - For built-in displays: returns platform-defined capabilities
  - If no display connected: returns empty array
  - Query failure: returns empty array (check logs for details)
  
- **Display Connection:**
  - Hot-plug events trigger resolution cache updates
  - Internal displays always report as "connected"
  - HDMI displays report actual connection status
  
- **Resolution Setting:**
  - Requires display to support the requested resolution
  - `persist` flag saves setting across reboots
  - Invalid resolution fails with `success: false`
  - Platform may apply closest supported resolution

### Audio Port Behavior
- **Port Enable/Disable:**
  - Only one primary audio port can be active simultaneously
  - Enabling a port may auto-disable other ports (platform-dependent)
  - HDMI_ARC0 has special handling (see HDMI ARC section)
  - Settings persist across reboots per port
  
- **Port Types:**
  - **HDMI0:** Standard HDMI audio output
  - **HDMI_ARC0:** HDMI Audio Return Channel (ARC/eARC)
  - **SPDIF0:** Digital optical output
  - **SPEAKER0:** Internal speakers
  - **HEADPHONE0:** Analog headphone jack
  - **IDLR0:** Internal digital audio (platform-specific)

### HDMI ARC/eARC Handling
- **Connection Flow:**
  ```
  1. Plugin detects HDMI ARC port (hdmiArcPortId)
  2. Subscribes to HdmiCecSink events
  3. Sends CEC power-on message to AVR
  4. Waits for AVR response (with timeout/retry)
  5. Detects ARC or eARC capability
  6. Routes audio accordingly
  ```
  
- **CEC Dependencies:**
  - Requires HdmiCecSink plugin to be active
  - CEC must be enabled for ARC/eARC
  - Audio device power state monitored via CEC
  - SADList (Short Audio Descriptor) retrieved via CEC
  
- **State Machine:**
  - `ARC_STATE_ARC_TERMINATED` → Initial/disconnected
  - `ARC_STATE_ARC_INITIATED` → ARC active
  - `ARC_STATE_EARC_CONNECTED` → eARC active (higher bandwidth)

### MS12 Audio Processing
- **Requirements:**
  - Requires Dolby MS12 audio decoder on platform
  - Not all platforms support all MS12 features
  - Feature availability checked via `getSettopMS12Capabilities`
  
- **Profiles:**
  - Profiles are preset combinations of audio settings
  - Override allows custom tuning beyond profiles
  - Profile changes may reset individual enhancement settings
  
- **Version Differences:**
  - Some APIs have v1 and v2 variants (e.g., `getVolumeLeveller` vs `getVolumeLeveller2`)
  - v2 typically has simplified parameter structure
  - Both versions remain for backward compatibility

### Power State Integration
- **Initialization:**
  - Audio ports initialized only when power state is ON
  - Video settings can be queried in standby (EDID cached)
  - Full audio setup deferred until system is active
  
- **Power Transitions:**
  - ON → STANDBY: Audio ports may be disabled
  - STANDBY → ON: Audio ports re-initialized
  - ARC/eARC setup re-triggered after power-on

### Persistence
- Audio port enable/disable states persisted per port
- Video resolution settings persisted when `persist: true`
- Audio enhancement levels persisted per port
- MS12 profile selection persisted
- Zoom settings persisted per video display

### Error Handling
- Invalid port names → `success: false`
- Display not connected → empty arrays (no error)
- Feature not supported → `success: false` (check logs)
- CEC/IARM communication failures → logged, retries attempted
- Device layer exceptions → caught and logged, `success: false` returned

---

## 6. API Examples

### Example 1: Get Supported Resolutions

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "org.rdk.DisplaySettings.1.getSupportedResolutions",
  "params": { "videoDisplay": "HDMI0" }
}
```

**Response (Display Connected):**
```json
{
  "success": true,
  "supportedResolutions": ["720p", "1080i", "1080p60", "2160p60"]
}
```

**Response (No Display):**
```json
{
  "success": true,
  "supportedResolutions": []
}
```

---

### Example 2: Enable HDMI Audio Port

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 5,
  "method": "org.rdk.DisplaySettings.1.setEnableAudioPort",
  "params": {
    "audioPort": "HDMI0",
    "enable": true
  }
}
```

**Response:**
```json
{
  "success": true
}
```

---

### Example 3: Set Dialog Enhancement

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "method": "org.rdk.DisplaySettings.1.setDialogEnhancement",
  "params": {
    "audioPort": "HDMI0",
    "enhancerlevel": 12
  }
}
```

**Response:**
```json
{
  "success": true
}
```

---

### Example 4: Get Color Depth Capabilities

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 9,
  "method": "org.rdk.DisplaySettings.1.getColorDepthCapabilities",
  "params": {
    "videoDisplay": "HDMI0"
  }
}
```

**Response:**
```json
{
  "success": true,
  "capabilities": ["8", "10", "12", "auto"]
}
```

---

### Example 5: Set MS12 Audio Profile

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 11,
  "method": "org.rdk.DisplaySettings.1.setMS12AudioProfile",
  "params": {
    "audioPort": "HDMI0",
    "ms12AudioProfile": "Movie"
  }
}
```

**Response:**
```json
{
  "success": true
}
```

---

### Example 6: Get Connected Audio Ports (ARC Active)

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 13,
  "method": "org.rdk.DisplaySettings.1.getConnectedAudioPorts"
}
```

**Response:**
```json
{
  "success": true,
  "connectedAudioPorts": ["HDMI0", "HDMI_ARC0", "SPEAKER0"]
}
```

---

### Example 7: Set Primary Language

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 15,
  "method": "org.rdk.DisplaySettings.1.setPrimaryLanguage",
  "params": {
    "audioPort": "HDMI0",
    "primaryLanguage": "eng"
  }
}
```

**Response:**
```json
{
  "success": true
}
```

---

## 7. Events & Notifications

The DisplaySettings plugin publishes events for state changes:

### Video Events
- **resolutionChanged:** Triggered when video resolution changes
- **zoomSettingChanged:** Triggered when zoom setting changes
- **connectedVideoDisplaysChanged:** Triggered when display connection state changes

### Audio Events
- **connectedAudioPortUpdated:** Triggered when audio port connection changes (especially HDMI ARC)
- **audioFormatChanged:** Triggered when audio format changes
- **audioPortEnabledChanged:** Triggered when audio port enable state changes

---

## 8. Dependencies

### Required Plugins
- **HdmiCecSink:** Required for HDMI ARC/eARC functionality
- **PowerManager:** Required for power state management

### Platform Requirements
- **Device Services HAL:** Required for all video/audio operations
- **Dolby MS12:** Required for MS12 audio processing APIs
- **IARM Bus:** Required for inter-component communication

### Optional Features
- CEC support (for ARC/eARC)
- Atmos decoder (for Atmos output)
- HDR support (platform-specific)

---

## 9. Notes

### API Versioning
- Some methods have v1 and v2 variants for backward compatibility
- Both versions are maintained and functional
- v2 APIs generally have simplified parameters

### Platform Variance
- Not all audio ports available on all platforms
- MS12 features platform-dependent
- HDR capabilities vary by SoC and display
- Resolution lists vary by platform chipset

### Thread Safety
- All APIs use locked implementations (`registerMethodLockedApi`)
- Concurrent access is safe
- Audio port state protected by mutex
- ARC/eARC state machine uses dedicated mutex

### Performance
- Resolution queries cached when possible
- EDID reads may take 100-500ms (hardware dependent)
- CEC communication adds latency to ARC setup
- Audio enhancement changes are immediate

---
