# DisplaySettings Plugin — Specification

## Overview

The `DisplaySettings` plugin manages video and audio output configuration for RDK-based set-top boxes and TV devices. It provides a JSON-RPC interface for querying and controlling display resolutions, audio ports, audio processing features (MS12), HDR modes, ARC/eARC routing, and EDID inspection. The plugin bridges the WPEFramework middleware layer with the Device Settings (DS) HAL through the `device::Host` abstraction.

| Property | Value |
|----------|-------|
| Callsign | `org.rdk.DisplaySettings` |
| API Version | 2.0.5 |
| Framework | WPEFramework Thunder JSONRPC |
| Precondition | `Platform` |
| Transport | JSON-RPC over HTTP at `127.0.0.1:9998` |

```
┌──────────────┐    JSON-RPC     ┌───────────────────────┐
│  Clients /   │◄───────────────►│  DisplaySettings       │
│  Apps / UI   │                 │  (org.rdk.DisplaySettings) │
└──────────────┘                 └──────────┬────────────┘
                                            │
                    ┌───────────────────────┼──────────────────────┐
                    │                       │                      │
          ┌─────────▼───────┐   ┌──────────▼──────┐   ┌──────────▼──────┐
          │  DS HAL          │   │  HdmiCecSink     │   │  PowerManager    │
          │  (device::Host)  │   │  Plugin (ARC)    │   │  Plugin          │
          └──────────────────┘   └─────────────────┘   └─────────────────┘
```

---

## Description

The `DisplaySettings` plugin is a WPEFramework Thunder plugin that exposes over 80 JSON-RPC methods covering four functional domains:

1. **Video output** — resolution management (get/set current and supported resolutions, TV vs. STB capabilities), zoom/DFC, HDR mode control, EDID inspection, color depth, and active-input detection.
2. **Audio output** — port enumeration, sound mode selection, port enable/disable, gain, volume, mute, audio delay (lip-sync), Atmos passthrough, and audio format reporting.
3. **MS12 audio processing** — Dolby MS12 pipeline controls including compression, dialog enhancement, equalizers, volume leveller, bass enhancer, surround virtualizer, DRC mode, MI steering, and audio profiles.
4. **Associated audio (AD)** — mixing enable, fader control, and primary/secondary language selection for accessibility audio streams.

Additionally, the plugin manages an **ARC/eARC routing state machine** that integrates with the `org.rdk.HdmiCecSink` plugin to automatically route audio from a connected AV receiver over the HDMI ARC port, including Short Audio Descriptor (SAD) negotiation and AVR power state management.

The plugin also integrates with `org.rdk.PowerManager` to properly handle audio port re-initialization and ARC state recovery across power transitions (ON/standby/sleep cycles).

---

## Requirements

### Functional Requirements

- **REQ-DS-001**: The plugin SHALL expose all video and audio output port enumeration methods via JSON-RPC.
- **REQ-DS-002**: The plugin SHALL allow clients to get and set the current video resolution, with optional EDID validation bypass (`ignoreEdid`).
- **REQ-DS-003**: Resolution changes SHALL emit a `resolutionPreChange` notification before applying and a `resolutionChanged` notification after completion.
- **REQ-DS-004**: The plugin SHALL persist zoom settings to `/opt/persistent/rdkservices/zoomSettings.json`.
- **REQ-DS-005**: The plugin SHALL expose MS12 audio processing controls (compression, dialog, EQ, volume leveller, bass enhancer, surround virtualizer, DRC, MI steering) when the DS HAL supports MS12.
- **REQ-DS-006**: The plugin SHALL manage ARC/eARC audio routing in coordination with `org.rdk.HdmiCecSink`, including SAD negotiation and AVR power-on sequencing.
- **REQ-DS-007**: On power state transitions received from `org.rdk.PowerManager`, the plugin SHALL re-initialize audio ports and restart ARC detection as appropriate.
- **REQ-DS-008**: The plugin SHALL emit events for all state changes (volume, mute, audio format, video format, Atmos capability, port connect/disconnect, language changes).
- **REQ-DS-009**: The plugin SHALL support API version 2 for `getVolumeLeveller`, `setVolumeLeveller`, `getSurroundVirtualizer`, and `setSurroundVirtualizer` with extended object response formats.
- **REQ-DS-010**: The plugin SHALL cache the current resolution and display-connected state, invalidating the cache on corresponding hardware events.
- **REQ-DS-011**: Audio port enable/disable state SHALL be persisted via the DS HAL persistence mechanism.
- **REQ-DS-012**: The plugin SHALL provide EDID read capability for both the connected display (`readEDID`) and the host STB (`readHostEDID`).
- **REQ-DS-013**: The plugin SHALL support associated audio (AD) mixing, fader control, and primary/secondary language selection per ISO 639-2.
- **REQ-DS-014**: The plugin SHALL precondition on `Platform` and SHALL start only after the platform is available.
- **REQ-DS-015**: `getSupportedResolutions` SHALL return the list of resolutions sourced from the connected display's EDID data, obtained via the `getSupportedTvResolutions` HAL API.
- **REQ-DS-016**: `getSupportedResolutions` SHALL default to the platform's default video output port (as returned by `getDefaultVideoPortName()`) when the `videoDisplay` parameter is omitted.
- **REQ-DS-017**: `getSupportedResolutions` SHALL return an empty `supportedResolutions` array when no display is connected on the requested port.
- **REQ-DS-018**: `getSupportedResolutions` SHALL reflect the newly connected display's EDID capabilities after a display hot-plug event; stale data from the previous display SHALL NOT be returned.

### Non-Functional Requirements

- **REQ-DS-NF-001**: Audio port initialization SHALL run in a background worker thread to avoid blocking plugin activation.
- **REQ-DS-NF-002**: All JSON-RPC methods SHALL be registered using locked-API wrappers (`registerMethodLockedApi`) to ensure thread safety.
- **REQ-DS-NF-003**: ARC routing timer intervals SHALL be: reconnection 5500ms, audio device detection 3000ms, SAD detection 3000ms, ARC detection 1000ms, AVR power transition 1000ms.

---

## Architecture / Design

### Component Diagram

```
┌────────────────────────────────────────────────────────────────┐
│                      DisplaySettings Plugin                    │
│                                                                │
│  ┌──────────────────┐  ┌──────────────────┐  ┌─────────────┐  │
│  │  Video Methods   │  │  Audio Methods   │  │    ARC/eARC │  │
│  │  Resolution, DFC │  │  MS12, Volume,   │  │  Routing    │  │
│  │  HDR, EDID       │  │  Mute, Gain, AD  │  │  State Mach │  │
│  └────────┬─────────┘  └────────┬─────────┘  └──────┬──────┘  │
│           │                     │                   │          │
└───────────┼─────────────────────┼───────────────────┼──────────┘
            │                     │                   │
   ┌────────▼─────────┐  ┌────────▼──────┐   ┌───────▼────────┐
   │   DS HAL          │  │  DS HAL       │   │ HdmiCecSink    │
   │   VideoOutputPort │  │  AudioOutput  │   │ Plugin         │
   │   VideoDevice     │  │  Port / MS12  │   │ (ARC events)   │
   └───────────────────┘  └───────────────┘   └────────────────┘
```

### Plugin Lifecycle

**Initialization** (`Initialize()`):
1. Registers all JSON-RPC methods on handler versions 1 and 2.
2. Initializes audio ports via `InitAudioPorts()` (runs asynchronously in a worker thread).
3. Registers DS HAL event listeners: `IDisplayEvents`, `IAudioOutputPortEvents`, `IDisplayDeviceEvents`, `IHdmiInEvents`, `IVideoDeviceEvents`, `IVideoOutputPortEvents`.
4. Registers with PowerManager plugin via `IPowerManager::IModeChangedNotification`.
5. Subscribes to HdmiCecSink events for ARC/eARC management (deferred until plugin activates).

**Deinitialization** (`Deinitialize()`):
1. Unregisters all DS HAL event listeners.
2. Releases PowerManager notification interface.
3. Stops ARC routing thread and associated timers.
4. Clears all cached state.

**Power State Handling**:
- On transition to ON: re-initializes audio ports, restarts ARC detection, sends pending AVR power-on messages.
- On transition to standby/sleep: suspends ARC routing, clears audio device connection state.

### ARC/eARC Routing State Machine

```
        ┌──────────────┐
        │  CEC Enabled │ ──── HdmiCecSink plugin activated
        └──────┬───────┘
               │ CEC ping detects audio device
               ▼
    ┌─────────────────────┐
    │ Audio Device        │ ──── m_hdmiCecAudioDeviceDetected = true
    │ Detected            │
    └──────┬──────────────┘
           │ Audio device confirms ARC capability
           ▼
    ┌──────────────────────┐
    │ ARC Initiation       │ ◄── arcInitiationEvent from HdmiCecSink
    │ (ARC_STATE_ARC_INIT) │
    └──────┬───────────────┘
           │ SAD exchanged successfully
           ▼
    ┌──────────────────────┐
    │ ARC Active           │ ──── m_arcEarcAudioEnabled = true
    │ (Audio routing via   │      connectedAudioPortUpdated(CONNECTED)
    │  HDMI_ARC0)          │
    └──────┬───────────────┘
           │ arcTerminationEvent / HPD disconnect
           ▼
    ┌──────────────────────┐
    │ ARC Terminated       │ ──── m_arcEarcAudioEnabled = false
    │ (ARC_STATE_ARC_TERM) │      connectedAudioPortUpdated(DISCONNECTED)
    └──────────────────────┘
```

### Interfaces Implemented

| Interface | Purpose |
|-----------|---------|
| `PluginHost::IPlugin` | Thunder plugin lifecycle |
| `PluginHost::JSONRPC` | JSON-RPC method registration and dispatch |
| `Exchange::IDeviceOptimizeStateActivator` | System mode change requests (`Request(newState)`) |
| `device::Host::IDisplayEvents` | Resolution change, active input events |
| `device::Host::IAudioOutputPortEvents` | Audio port connect/disconnect, format change |
| `device::Host::IDisplayDeviceEvents` | Display device connect/disconnect |
| `device::Host::IHdmiInEvents` | HDMI input events |
| `device::Host::IVideoDeviceEvents` | HDR capability change events |
| `device::Host::IVideoOutputPortEvents` | Video output port events |

### Caching

| Cache Variable | Invalidation Trigger |
|----------------|---------------------|
| `currentResolutionCache` | `resolutionChanged` event |
| `isDisplayConnectedCacheUpdated` | HDMI hot-plug event |
| `stbHDRcapabilitiesCache` | HDR capability change |
| `audioPortEnableStatusMap` | `setEnableAudioPort` call or `audioPortEnableStatusChanged` event |

### Persistent Storage

| Data | Location |
|------|----------|
| Zoom setting | `/opt/persistent/rdkservices/zoomSettings.json` |
| Audio port enable state | DS HAL (`getEnablePersist` / `setEnablePersist`) |
| Audio mode / MS12 settings | DS HAL persistence |
| Color depth | DS HAL persistence |

### ARC/eARC Timers

| Timer | Timeout | Purpose |
|-------|---------|---------|
| `m_timer` | 5500ms | HdmiCecSink reconnection |
| `m_AudioDeviceDetectTimer` | 3000ms | Verify audio device connectivity |
| `m_SADDetectionTimer` | 3000ms | Short Audio Descriptor update check |
| `m_ArcDetectionTimer` | 1000ms | ARC device presence polling |
| `m_AudioDevicePowerOnStatusTimer` | 1000ms | AVR power state transition delay |

---

## External Interfaces

### JSON-RPC API — Video Display Methods

All methods follow the JSON-RPC 2.0 pattern. Every response includes `"success": true|false`.

#### `getConnectedVideoDisplays`
Returns a list of currently connected video display port names.
- **Parameters**: none
- **Response**: `{ "connectedVideoDisplays": ["HDMI0"], "success": true }`

#### `getSupportedVideoDisplays`
Returns all video output ports available on the device.
- **Parameters**: none
- **Response**: `{ "supportedVideoDisplays": ["HDMI0"], "success": true }`

#### `getSupportedResolutions`
Returns supported resolutions for a given video output port.
- **Parameters**: `videoDisplay` (string, optional — defaults to the platform default port, typically `"HDMI0"`)
- **Response**: `{ "supportedResolutions": ["720p", "1080i", "1080p60"], "success": true }`
- **Data source**: Resolutions are sourced from the connected display's EDID data via `getSupportedTvResolutions` HAL API (same bitmask approach as the `getSupportedTvResolutions` JSON-RPC method).
- **No-display behavior**: When no display is connected on the requested port, returns `{ "supportedResolutions": [], "success": true }`.
- **Refresh on reconnect**: The supported resolution list SHALL reflect the newly connected display's capabilities after a display hot-plug event; stale data from the previous display SHALL NOT be returned.

#### `getSupportedTvResolutions`
Returns TV-reported supported resolutions from EDID/HDCP negotiation.
- **Parameters**: `videoDisplay` (string, optional)
- **Response**: `{ "supportedTvResolutions": ["480i", "720p", "1080p", "2160p60"], "success": true }`
- **Notes**: Returns `["none"]` when the TV reports no resolutions.

#### `getSupportedSettopResolutions`
Returns resolutions supported by the STB hardware.
- **Parameters**: none
- **Response**: `{ "supportedSettopResolutions": ["720p", "1080i", "1080p60"], "success": true }`

#### `getCurrentResolution`
Returns the currently active resolution with pixel dimensions.
- **Parameters**: `videoDisplay` (string, optional)
- **Response**: `{ "resolution": "1080p60", "w": 1920, "h": 1080, "progressive": true, "success": true }`
- **Resolution→Pixel mapping**: 480→720×480, 576→720×576, 720→1280×720, 768→1366×768, 1080→1920×1080, 2160→3840×2160, 4096x2160→4096×2160
- **Notes**: Result is cached; cache invalidated by `resolutionChanged` event.

#### `setCurrentResolution`
Sets the active resolution for a video port.
- **Parameters**: `videoDisplay` (string, required), `resolution` (string, required), `persist` (boolean, default `true`), `ignoreEdid` (boolean, default `false`)
- **Response**: `{ "success": true }`
- **Events emitted**: `resolutionPreChange`, `resolutionChanged`

#### `getDefaultResolution`
Returns the factory/fallback resolution for the default video port.
- **Parameters**: none
- **Response**: `{ "defaultResolution": "720p", "success": true }`

#### `getZoomSetting` / `setZoomSetting`
Gets or sets the zoom/DFC mode.
- **Set Parameters**: `zoomSetting` (string, e.g. `"FULL"`, `"NONE"`)
- **Get Response**: `{ "zoomSetting": "FULL", "success": true }`
- **Events emitted (set)**: `zoomSettingUpdated`

#### `getActiveInput`
Returns whether the connected display reports the STB as its active input.
- **Parameters**: none
- **Response**: `{ "activeInput": true, "success": true }`

#### `getCurrentOutputSettings`
Returns current video signal parameters.
- **Parameters**: none
- **Response**: `{ "videoEOTF": 1, "matrixCoefficients": 0, "colorSpace": 3, "colorDepth": 10, "quantizationRange": 4, "success": true }`

#### `setForceHDRMode`
Forces or releases a specific HDR mode on video output.
- **Parameters**: `hdrMode` (boolean)
- **Response**: `{ "success": true }`

#### `getTvHDRSupport`
Returns HDR standards supported by the connected TV.
- **Parameters**: none
- **Response**: `{ "standards": ["HDR10", "HLG"], "supportsHDR": true, "success": true }`

#### `getSettopHDRSupport`
Returns HDR standards supported by the STB hardware.
- **Parameters**: none
- **Response**: `{ "standards": ["HDR10", "DolbyVision"], "supportsHDR": true, "success": true }`

#### `getTVHDRCapabilities`
Returns a bitmask of HDR capabilities reported by the TV (`dsHDRStandard_t` enum).
- **Parameters**: none
- **Response**: `{ "capabilities": 3, "success": true }`

#### `getVideoFormat`
Returns the current incoming video/HDR format.
- **Parameters**: none
- **Response**: `{ "videoFormat": "HDR10", "success": true }`

#### `isConnectedDeviceRepeater`
Returns whether the downstream HDMI device is an HDMI repeater.
- **Parameters**: none
- **Response**: `{ "connected": false, "success": true }`

#### `readEDID` / `readHostEDID`
Returns EDID data from the connected display or from the host STB.
- **Parameters**: none
- **Response**: `{ "EDID": "<encoded-EDID>", "success": true }`

#### `setPreferredColorDepth` / `getPreferredColorDepth`
Gets or sets preferred color depth for video output.
- **Set Parameters**: `videoDisplay` (string), `colorDepth` (string: `"8bit"`, `"10bit"`, `"12bit"`, `"auto"`)
- **Get Response**: `{ "colorDepth": "10bit", "success": true }`

#### `getColorDepthCapabilities`
Returns color depth values supported by the video output port.
- **Parameters**: none
- **Response**: `{ "capabilities": ["8bit", "10bit"], "success": true }`

#### `setScartParameter`
Configures SCART output parameters on devices with a SCART port.
- **Parameters**: Platform-specific SCART parameter object (schema not yet defined — see Open Queries).
- **Response**: `{ "success": true }`

---

### JSON-RPC API — Audio Port Methods

#### `getConnectedAudioPorts`
Returns currently connected audio output ports. `HDMI_ARC0` is included only when an ARC/eARC device is actively routed.
- **Parameters**: none
- **Response**: `{ "connectedAudioPorts": ["HDMI0"], "success": true }`

#### `getSupportedAudioPorts`
Returns all audio output ports available on the device.
- **Parameters**: none
- **Response**: `{ "supportedAudioPorts": ["HDMI0", "SPDIF0", "HDMI_ARC0", "HEADPHONE0"], "success": true }`

#### `getSupportedAudioModes`
Returns audio modes supported by an audio port.
- **Parameters**: `audioPort` (string, required)
- **Response**: `{ "supportedAudioModes": ["STEREO", "SURROUND", "DOLBYDIGITAL"], "success": true }`

#### `getSoundMode` / `setSoundMode`
Gets or sets the active audio mode for a port.
- **Get Parameters**: `audioPort` (string, optional — defaults to first connected port)
- **Get Response**: `{ "soundMode": "AUTO (Dolby Digital 5.1)", "success": true }`
- **Set Parameters**: `audioPort` (string), `soundMode` (string), `persist` (boolean, default `true`)

#### `setEnableAudioPort` / `getEnableAudioPort`
Enables or disables an audio output port. Supported ports: `IDLR0`, `HDMI0`, `SPDIF0`, `SPEAKER0`, `HDMI_ARC0`, `HEADPHONE0`.
- **Set Parameters**: `audioPort` (string), `enable` (boolean)
- **Get Parameters**: `audioPort` (string)
- **Events emitted**: `audioPortEnableStatusChanged`

#### `getAudioFormat`
Returns the current audio format being decoded/passed through (`dsAudioFormat_t` enum values).
- **Parameters**: none
- **Response**: `{ "audioFormat": "DOLBY AC-3", "success": true }`

#### `getAudioDelay` / `setAudioDelay`
Gets or sets audio output delay in milliseconds for lip-sync adjustment.
- **Parameters**: `audioPort` (string), `audioDelay` (integer ms, set only)
- **Response**: `{ "audioDelay": 0, "success": true }`

#### `getSinkAtmosCapability`
Returns Atmos capability of the sink device (`dsATMOSCapability_t` enum).
- **Parameters**: none
- **Response**: `{ "atmos_capability": 2, "success": true }`

#### `setAudioAtmosOutputMode`
Enables or disables Dolby Atmos output passthrough.
- **Parameters**: `enable` (boolean)
- **Response**: `{ "success": true }`

#### `setGain` / `getGain`
Gets or sets audio output gain (dB) on a port.
- **Parameters**: `audioPort` (string), `gain` (number dB, set only)
- **Response**: `{ "gain": -2.0, "success": true }`

#### `setMuted` / `getMuted`
Gets or sets the mute state of an audio port.
- **Parameters**: `audioPort` (string), `muted` (boolean, set only)
- **Response**: `{ "muted": false, "success": true }`
- **Events emitted (set)**: `muteStatusChanged`

#### `setVolumeLevel` / `getVolumeLevel`
Gets or sets the volume level (0–100) of an audio port.
- **Parameters**: `audioPort` (string), `volumeLevel` (number 0–100, set only)
- **Response**: `{ "volumeLevel": 50.0, "success": true }`
- **Events emitted (set)**: `volumeLevelChanged`

#### `getSettopAudioCapabilities`
Returns audio capability bitmask of the STB for a given port.
- **Parameters**: `audioPort` (string)
- **Response**: `{ "AudioCapabilities": 3, "success": true }`

#### `getSettopMS12Capabilities`
Returns MS12 audio processing capability flags of the STB.
- **Parameters**: `audioPort` (string)
- **Response**: `{ "MS12Capabilities": 7, "success": true }`

#### `getSupportedMS12Config`
Returns supported MS12 configuration modes.
- **Parameters**: none
- **Response**: `{ "ms12config": [...], "success": true }`

---

### JSON-RPC API — MS12 Audio Processing Methods

> Only effective when the DS HAL supports Dolby MS12 audio processing.

#### `setMS12AudioCompression` / `getMS12AudioCompression`
- **Parameters**: `audioPort` (string), `compresionLevel` (integer 0–10, set only)
- **Response**: `{ "compressionLevel": 5, "success": true }`

#### `setDolbyVolumeMode` / `getDolbyVolumeMode`
- **Parameters**: `audioPort` (string), `dolbyVolumeMode` (boolean, set only)
- **Response**: `{ "dolbyVolumeMode": true, "success": true }`

#### `setDialogEnhancement` / `getDialogEnhancement` / `resetDialogEnhancement`
- **Set Parameters**: `audioPort` (string), `enhancementLevel` (integer 0–16)
- **Get Response**: `{ "enhancementLevel": 0, "success": true }`

#### `setIntelligentEqualizerMode` / `getIntelligentEqualizerMode`
- **Parameters**: `audioPort` (string), `intelligentEqualizerMode` (integer, set only)
- **Response**: `{ "intelligentEqualizerMode": 0, "success": true }`

#### `setGraphicEqualizerMode` / `getGraphicEqualizerMode`
- **Parameters**: `audioPort` (string), `graphicEqualizerMode` (integer, set only)
- **Response**: `{ "graphicEqualizerMode": 0, "success": true }`

#### `setMS12AudioProfile` / `getMS12AudioProfile` / `getSupportedMS12AudioProfiles`
Manages MS12 audio profiles (e.g. Movie, Music, Sports).
- **Set Parameters**: `audioPort` (string), `ms12AudioProfile` (string)
- **Get Response**: `{ "ms12AudioProfile": "Movie", "success": true }`
- **Supported Profiles Response**: `{ "supportedMS12AudioProfiles": ["Movie", "Music", "Sports", "OFF"], "success": true }`

#### `getVolumeLeveller` / `setVolumeLeveller` / `resetVolumeLeveller`
Controls MS12 volume leveller. v1 and v2 differ in response format.
- **v1 Set Parameters**: `audioPort` (string), `mode` (integer 0=off, 1=on, 2=auto)
- **v2 Set Parameters**: `audioPort` (string), `enable` (boolean), `level` (integer 0–10)
- **v1 Get Response**: `{ "enable": true, "level": 5, "success": true }`
- **v2 Get Response**: `{ "volumeLeveller": { "enable": true, "level": 5 }, "success": true }`

#### `getBassEnhancer` / `setBassEnhancer` / `resetBassEnhancer`
- **Parameters**: `audioPort` (string), `bassBoost` (integer 0–100, set only)
- **Response**: `{ "bassBoost": 50, "success": true }`

#### `isSurroundDecoderEnabled` / `enableSurroundDecoder`
- **Enable Parameters**: `audioPort` (string), `surroundDecoderEnabled` (boolean)
- **Query Response**: `{ "surroundDecoderEnabled": true, "success": true }`

#### `getDRCMode` / `setDRCMode`
- **Parameters**: `audioPort` (string), `DRCMode` (integer 0=Line, 1=RF, set only)
- **Response**: `{ "DRCMode": 0, "success": true }`

#### `getSurroundVirtualizer` / `setSurroundVirtualizer` / `resetSurroundVirtualizer`
Controls surround sound virtualizer. v1 and v2 differ in response format.
- **v1 Set Parameters**: `audioPort` (string), `enable` (boolean), `boost` (integer 0–96)
- **v2 Get Response**: `{ "surroundVirtualizer": { "enable": true, "boost": 50 }, "success": true }`

#### `getMISteering` / `setMISteering`
- **Parameters**: `audioPort` (string), `MISteering` (boolean, set only)
- **Response**: `{ "MISteering": false, "success": true }`

#### `setMS12ProfileSettingsOverride`
Overrides MS12 profile settings at runtime.
- **Parameters**: Profile key-value overrides (platform-specific — see Open Queries).
- **Response**: `{ "success": true }`

---

### JSON-RPC API — Associated Audio (AD) Methods

#### `setAssociatedAudioMixing` / `getAssociatedAudioMixing`
Controls mixing of secondary (AD) audio with primary audio.
- **Parameters**: `audioPort` (string), `mixing` (boolean, set only)
- **Response**: `{ "mixing": true, "success": true }`
- **Events emitted (set)**: `associatedAudioMixingChanged`

#### `setFaderControl` / `getFaderControl`
Sets the fader balance between primary and secondary audio (-32 to 32).
- **Parameters**: `audioPort` (string), `mixerBalance` (integer, set only)
- **Response**: `{ "mixerBalance": 0, "success": true }`
- **Events emitted (set)**: `faderControlChanged`

#### `setPrimaryLanguage` / `getPrimaryLanguage`
Gets or sets preferred primary audio language (ISO 639-2 three-character code).
- **Parameters**: `primaryLanguage` (string, set only)
- **Response**: `{ "lang": "eng", "success": true }`
- **Events emitted (set)**: `primaryLanguageChanged`

#### `setSecondaryLanguage` / `getSecondaryLanguage`
Gets or sets preferred secondary audio language for AD streams (ISO 639-2).
- **Parameters**: `secondaryLanguage` (string, set only)
- **Response**: `{ "lang": "fra", "success": true }`
- **Events emitted (set)**: `secondaryLanguageChanged`

---

### Events (JSON-RPC Notifications)

| Event | Payload | Trigger |
|-------|---------|---------|
| `resolutionPreChange` | `{}` | Before resolution change begins |
| `resolutionChanged` | `{ "width": int, "height": int }` | Resolution change completed |
| `zoomSettingUpdated` | `{ "zoomSetting": string }` | Zoom/DFC mode changed |
| `activeInputChanged` | `{ "activeInput": bool }` | Display active-input state changed |
| `connectedVideoDisplaysUpdated` | `{ "connectedVideoDisplays": string[] }` | HDMI hot-plug event |
| `connectedAudioPortUpdated` | `{ "HotpluggedAudioPort": string, "isConnected": bool }` | Audio port connect/disconnect |
| `audioFormatChanged` | `{ "audioFormat": string }` | Incoming audio format changed |
| `AtmosCapabilityChanged` | `{ "atmos_capability": int }` | Sink Atmos capability changed |
| `videoFormatChanged` | `{ "videoFormat": string }` | Incoming HDR/video format changed |
| `associatedAudioMixingChanged` | `{ "mixing": bool }` | AD mixing state changed |
| `faderControlChanged` | `{ "mixerBalance": int }` | Fader control changed |
| `primaryLanguageChanged` | `{ "primaryLanguage": string }` | Primary audio language changed |
| `secondaryLanguageChanged` | `{ "secondaryLanguage": string }` | Secondary audio language changed |
| `muteStatusChanged` | `{ "muted": bool, "audioPort": string }` | Port mute state changed |
| `volumeLevelChanged` | `{ "volumeLevel": float, "audioPort": string }` | Port volume level changed |
| `audioPortEnableStatusChanged` | `{ "audioPort": string, "enabled": bool }` | Audio port enable state changed |

### Configuration

| Key | Default | Description |
|-----|---------|-------------|
| `callsign` | `org.rdk.DisplaySettings` | Plugin registration name |
| `autostart` | `PLUGIN_DISPLAYSETTINGS_AUTOSTART` (build-time) | Auto-activate on framework start |
| `startuporder` | `PLUGIN_DISPLAYSETTINGS_STARTUPORDER` (build-time) | Startup ordering |
| `preconditions` | `Platform` | Required precondition |

### API Version Compatibility

The plugin registers handlers for version 1 (default) and version 2 (`CreateHandler({2})`). Version 2 overrides the following methods:

| Method | v1 Format | v2 Format |
|--------|-----------|-----------|
| `getVolumeLeveller` | flat `enable`/`level` fields | `volumeLeveller: { enable, level }` object |
| `setVolumeLeveller` | flat fields | object parameter |
| `getSurroundVirtualizer` | flat `enable`/`boost` fields | `surroundVirtualizer: { enable, boost }` object |
| `setSurroundVirtualizer` | flat fields | object parameter |

Clients should use callsign `org.rdk.DisplaySettings.2` for v2 methods.

---

## Performance

- Audio port initialization runs in a dedicated background thread (`initAudioPortsWorker`) to avoid blocking plugin activation.
- Display-connected status and resolution are cached to avoid repeated DS HAL calls on frequent reads; caches are invalidated by hardware events.
- All JSON-RPC methods use locked-API wrappers to serialize concurrent calls without busy-waiting.

---

## Security

- The plugin does not expose any authentication mechanism; access control is expected to be enforced at the WPEFramework layer (token-based access or firewall rules on port 9998).
- No user-provided data is passed to shell commands or file-system paths without sanitization; the zoom-settings file path is a hardcoded constant.
- RFC/TR181 feature flags are read-only from this plugin's perspective; no sensitive data is written via RFC API.

---

## Versioning & Compatibility

- Plugin API version is `2.0.5` (Major=2, Minor=0, Patch=5).
- The `SERVICE_REGISTRATION` macro registers the plugin with the Thunder framework under this version.
- v1 clients using callsign `org.rdk.DisplaySettings.1` receive the v1 response format for all methods.
- v2 clients using callsign `org.rdk.DisplaySettings.2` receive the extended object format for the four overridden methods.
- The plugin is backward-compatible: v1 clients are unaffected by the v2 handler registration.

---

## Conformance Testing & Validation

L2 integration tests exist under `Tests/L2Tests/tests/DisplaySettings_L2Test.cpp`. The test fixture (`DisplaySettings_L2test`):
- Activates `org.rdk.PowerManager` and `org.rdk.DisplaySettings` via WPEFramework plugin activation.
- Registers mock DS HAL delegates for all `device::Host` event listener interfaces.
- Invokes JSON-RPC methods via `InvokeServiceMethod` and validates responses.
- Verifies event notifications are received with the expected payloads.

Covered test scenarios (partial — see L2 test file for full list):
- `getCurrentResolution` — verifies cached and uncached paths.
- `getAudioFormat` — verifies `dsAudioFormat_t` format string mapping.
- `getVideoFormat` — verifies `dsHDRStandard_t` format string mapping.
- `setCurrentResolution` — verifies DS HAL `setResolution` is called with correct parameters.
- `getColorDepthCapabilities`, `setForceHDRMode`, `getCurrentOutputSettings`, `getTVHDRCapabilities`.

L1 unit test stubs exist under `Tests/L1Tests/`.

---

## Covered Code

- plugin/DisplaySettings.cpp:
    - `DisplaySettings::DisplaySettings`
    - `DisplaySettings::~DisplaySettings`
    - `DisplaySettings::Initialize`
    - `DisplaySettings::Deinitialize`
    - `DisplaySettings::InitAudioPorts`
    - `DisplaySettings::AudioPortsReInitialize`
    - `DisplaySettings::initAudioPortsWorker`
    - `DisplaySettings::InitializePowerManager`
    - `DisplaySettings::registerEventHandlers`
    - `DisplaySettings::registerDsEventHandlers`
    - `DisplaySettings::onPowerModeChanged`
    - `DisplaySettings::Request`
    - `DisplaySettings::getConnectedVideoDisplays`
    - `DisplaySettings::getConnectedVideoDisplaysHelper`
    - `DisplaySettings::getConnectedAudioPorts`
    - `DisplaySettings::getSupportedResolutions`
    - `DisplaySettings::getSupportedVideoDisplays`
    - `DisplaySettings::getSupportedTvResolutions`
    - `DisplaySettings::getSupportedSettopResolutions`
    - `DisplaySettings::getSupportedAudioPorts`
    - `DisplaySettings::getSupportedAudioModes`
    - `DisplaySettings::getZoomSetting`
    - `DisplaySettings::setZoomSetting`
    - `DisplaySettings::getCurrentResolution`
    - `DisplaySettings::setCurrentResolution`
    - `DisplaySettings::getDefaultResolution`
    - `DisplaySettings::getSoundMode`
    - `DisplaySettings::setSoundMode`
    - `DisplaySettings::readEDID`
    - `DisplaySettings::readHostEDID`
    - `DisplaySettings::getActiveInput`
    - `DisplaySettings::getTvHDRSupport`
    - `DisplaySettings::getSettopHDRSupport`
    - `DisplaySettings::getCurrentOutputSettings`
    - `DisplaySettings::setForceHDRMode`
    - `DisplaySettings::setMS12AudioCompression`
    - `DisplaySettings::getMS12AudioCompression`
    - `DisplaySettings::setDolbyVolumeMode`
    - `DisplaySettings::getDolbyVolumeMode`
    - `DisplaySettings::setDialogEnhancement`
    - `DisplaySettings::getDialogEnhancement`
    - `DisplaySettings::resetDialogEnhancement`
    - `DisplaySettings::setIntelligentEqualizerMode`
    - `DisplaySettings::getIntelligentEqualizerMode`
    - `DisplaySettings::setGraphicEqualizerMode`
    - `DisplaySettings::getGraphicEqualizerMode`
    - `DisplaySettings::setMS12AudioProfile`
    - `DisplaySettings::getMS12AudioProfile`
    - `DisplaySettings::getSupportedMS12AudioProfiles`
    - `DisplaySettings::getVolumeLeveller`
    - `DisplaySettings::getVolumeLeveller2`
    - `DisplaySettings::setVolumeLeveller`
    - `DisplaySettings::setVolumeLeveller2`
    - `DisplaySettings::resetVolumeLeveller`
    - `DisplaySettings::getBassEnhancer`
    - `DisplaySettings::setBassEnhancer`
    - `DisplaySettings::resetBassEnhancer`
    - `DisplaySettings::isSurroundDecoderEnabled`
    - `DisplaySettings::enableSurroundDecoder`
    - `DisplaySettings::getSurroundVirtualizer`
    - `DisplaySettings::getSurroundVirtualizer2`
    - `DisplaySettings::setSurroundVirtualizer`
    - `DisplaySettings::setSurroundVirtualizer2`
    - `DisplaySettings::resetSurroundVirtualizer`
    - `DisplaySettings::getMISteering`
    - `DisplaySettings::setMISteering`
    - `DisplaySettings::getDRCMode`
    - `DisplaySettings::setDRCMode`
    - `DisplaySettings::getAudioDelay`
    - `DisplaySettings::setAudioDelay`
    - `DisplaySettings::getSinkAtmosCapability`
    - `DisplaySettings::setAudioAtmosOutputMode`
    - `DisplaySettings::getTVHDRCapabilities`
    - `DisplaySettings::isConnectedDeviceRepeater`
    - `DisplaySettings::setScartParameter`
    - `DisplaySettings::getGain`
    - `DisplaySettings::setGain`
    - `DisplaySettings::getMuted`
    - `DisplaySettings::setMuted`
    - `DisplaySettings::getVolumeLevel`
    - `DisplaySettings::setVolumeLevel`
    - `DisplaySettings::getSettopMS12Capabilities`
    - `DisplaySettings::getSettopAudioCapabilities`
    - `DisplaySettings::setEnableAudioPort`
    - `DisplaySettings::getEnableAudioPort`
    - `DisplaySettings::setAssociatedAudioMixing`
    - `DisplaySettings::getAssociatedAudioMixing`
    - `DisplaySettings::setFaderControl`
    - `DisplaySettings::getFaderControl`
    - `DisplaySettings::setPrimaryLanguage`
    - `DisplaySettings::getPrimaryLanguage`
    - `DisplaySettings::setSecondaryLanguage`
    - `DisplaySettings::getSecondaryLanguage`
    - `DisplaySettings::getAudioFormat`
    - `DisplaySettings::getVideoFormat`
    - `DisplaySettings::setMS12ProfileSettingsOverride`
    - `DisplaySettings::setPreferredColorDepth`
    - `DisplaySettings::getPreferredColorDepth`
    - `DisplaySettings::getColorDepthCapabilities`
    - `DisplaySettings::getSupportedMS12Config`
    - `DisplaySettings::audioFormatToString`
    - `DisplaySettings::getVideoFormatTypeToString`
    - `DisplaySettings::getVideoFormatTypeFromString`
    - `DisplaySettings::getSupportedVideoFormats`
    - `DisplaySettings::checkPortName`
    - `DisplaySettings::getSystemPowerState`
    - `DisplaySettings::isDisplayConnected`
    - `DisplaySettings::resolutionPreChange`
    - `DisplaySettings::resolutionChanged`
    - `DisplaySettings::zoomSettingUpdated`
    - `DisplaySettings::activeInputChanged`
    - `DisplaySettings::connectedVideoDisplaysUpdated`
    - `DisplaySettings::connectedAudioPortUpdated`
    - `DisplaySettings::notifyAudioFormatChange`
    - `DisplaySettings::notifyAtmosCapabilityChange`
    - `DisplaySettings::notifyAssociatedAudioMixingChange`
    - `DisplaySettings::notifyFaderControlChange`
    - `DisplaySettings::notifyPrimaryLanguageChange`
    - `DisplaySettings::notifySecondaryLanguageChange`
    - `DisplaySettings::notifyVideoFormatChange`
    - `DisplaySettings::onARCInitiationEventHandler`
    - `DisplaySettings::onARCTerminationEventHandler`
    - `DisplaySettings::onShortAudioDescriptorEventHandler`
    - `DisplaySettings::onSystemAudioModeEventHandler`
    - `DisplaySettings::onArcAudioStatusEventHandler`
    - `DisplaySettings::onAudioDeviceConnectedStatusEventHandler`
    - `DisplaySettings::onCecEnabledEventHandler`
    - `DisplaySettings::onAudioDevicePowerStatusEventHandler`
    - `DisplaySettings::OnResolutionPreChange`
    - `DisplaySettings::OnResolutionPostChange`
    - `DisplaySettings::OnDisplayHDMIHotPlug`
    - `DisplaySettings::OnDisplayRxSense`
    - `DisplaySettings::OnDolbyAtmosCapabilitiesChanged`
    - `DisplaySettings::OnAudioFaderControlChanged`
    - `DisplaySettings::OnAudioFormatUpdate`
    - `DisplaySettings::OnAudioOutHotPlug`
    - `DisplaySettings::OnAudioPortStateChanged`
    - `DisplaySettings::OnAudioPrimaryLanguageChanged`
    - `DisplaySettings::OnAudioSecondaryLanguageChanged`
    - `DisplaySettings::OnAssociatedAudioMixingChanged`
    - `DisplaySettings::OnHdmiInEventHotPlug`
    - `DisplaySettings::OnVideoFormatUpdate`
    - `DisplaySettings::OnZoomSettingsChanged`
    - `DisplaySettings::getHdmiCecSinkPlugin`
    - `DisplaySettings::subscribeForHdmiCecSinkEvent`
    - `DisplaySettings::setUpHdmiCecSinkArcRouting`
    - `DisplaySettings::requestShortAudioDescriptor`
    - `DisplaySettings::requestAudioDevicePowerStatus`
    - `DisplaySettings::requestDeviceAudioStatus`
    - `DisplaySettings::sendUserControlPressCommand`
    - `DisplaySettings::sendHdmiCecSinkAudioDevicePowerOn`
    - `DisplaySettings::getHdmiCecSinkCecEnableStatus`
    - `DisplaySettings::getHdmiCecSinkAudioDeviceConnectedStatus`
    - `DisplaySettings::getAudioDeviceSADState`
    - `DisplaySettings::setAudioDeviceSADState`
    - `DisplaySettings::getCurrentArcRoutingState`
    - `DisplaySettings::onTimer`
    - `DisplaySettings::stopCecTimeAndUnsubscribeEvent`
    - `DisplaySettings::checkAudioDeviceDetectionTimer`
    - `DisplaySettings::checkArcDeviceConnected`
    - `DisplaySettings::checkSADUpdate`
    - `DisplaySettings::checkAudioDevicePowerStatusTimer`
    - `DisplaySettings::sendMsgToQueue`
    - `DisplaySettings::sendMsgThread`
- plugin/DisplaySettings.h:
    - `class DisplaySettings`
    - `class DisplaySettings::PowerManagerNotification`
- plugin/Module.cpp:
    - Module registration
- plugin/Module.h:
    - Module declaration
- Tests/L2Tests/tests/DisplaySettings_L2Test.cpp:
    - `DisplaySettings_L2test` (test fixture)
    - `DisplaySettings_L2test::DisplaySettings_L2test`
    - `DisplaySettings_L2test::~DisplaySettings_L2test`
    - `TEST_F(DisplaySettings_L2test, DisplaySettings_L2_MethodTest)`

---

## Open Queries

- **Zoom setting enumeration**: What is the complete list of valid `zoomSetting` string values beyond `"FULL"` and `"NONE"`? Are these defined by the DS HAL `dsVideoZoom_t` enum and are they platform-dependent?
- **`setScartParameter` schema**: The parameter schema for SCART configuration is not documented in the code. What fields are required/optional, and which platforms is this applicable to?
- **`setMS12ProfileSettingsOverride` schema**: What key-value pairs are valid overrides? Is this entirely platform-specific or is there a common set?
- **ARC/eARC volume forwarding**: When `m_arcEarcAudioEnabled` is true, `setMuted`/`setVolumeLevel` appear to forward commands to the AV receiver via CEC. Is this behaviour fully specified and intentional? Is there a fallback if CEC is unavailable?
- **`getSupportedMS12Config` response format**: The exact structure of the `ms12config` array elements is not documented in the code. Needs confirmation from DS HAL spec.
- **`IDeviceOptimizeStateActivator::Request` state values**: What state strings are valid inputs and what is the observable effect on audio/video output?
- **`audioPortEnableStatusMap` hardcoded ports**: Ports `IDLR0`, `HDMI0`, `SPDIF0`, `SPEAKER0`, `HDMI_ARC0`, `HEADPHONE0` are hardcoded in the constructor. How are platforms with subsets or supersets of these ports handled?
- **EDID encoding format**: Is the EDID data returned by `readEDID` / `readHostEDID` raw binary encoded as base64, as a hex string, or in another format?

---

## References

- [RDK Central — entservices-displaysettings repository](https://github.com/rdkcentral/entservices-displaysettings)
- [WPEFramework Thunder Plugin documentation](https://rdkcentral.github.io/Thunder/)
- [Dolby MS12 Audio Processing documentation](https://professional.dolby.com/product/dolby-ms12/)
- [HDMI ARC/eARC specification — HDMI Forum](https://www.hdmi.org/spec/earc)
- [ISO 639-2 Language Codes](https://www.loc.gov/standards/iso639-2/php/code_list.php)
- DS HAL API — `dsTypes.h`, `dsError.h`, `dsDisplay.h`
- [plugin/CHANGELOG.md](plugin/CHANGELOG.md) — DisplaySettings plugin version history

---

## Change History

- [2026-04-28] - openspec-templater - Restructured to match spec template.
