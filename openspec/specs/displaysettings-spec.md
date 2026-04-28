# DisplaySettings Plugin — Specification

**Callsign**: `org.rdk.DisplaySettings`  
**API Version**: 2.0.5  
**Framework**: WPEFramework Thunder JSONRPC  
**Precondition**: `Platform`  
**Transport**: JSON-RPC over HTTP at `127.0.0.1:9998`

---

## 1. Overview

The `DisplaySettings` plugin manages video and audio output configuration for RDK-based set-top boxes and TV devices. It provides a JSON-RPC interface for querying and controlling display resolutions, audio ports, audio processing features (MS12), HDR modes, ARC/eARC routing, and EDID inspection. The plugin bridges the WPEFramework middleware layer with the Device Settings (DS) HAL through the `device::Host` abstraction.

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

## 2. Plugin Lifecycle

### 2.1 Initialization

On `Initialize()`:
1. Registers all JSON-RPC methods on handler versions 1 and 2.
2. Initializes audio ports via `InitAudioPorts()` (runs asynchronously in a worker thread).
3. Registers DS HAL event listeners:
   - `device::Host::IDisplayEvents`
   - `device::Host::IAudioOutputPortEvents`
   - `device::Host::IDisplayDeviceEvents`
   - `device::Host::IHdmiInEvents`
   - `device::Host::IVideoDeviceEvents`
   - `device::Host::IVideoOutputPortEvents`
4. Registers with the PowerManager plugin via `IPowerManager::IModeChangedNotification`.
5. Subscribes to HdmiCecSink events for ARC/eARC management (deferred until plugin activates).

### 2.2 Deinitialization

On `Deinitialize()`:
1. Unregisters all DS HAL event listeners.
2. Releases PowerManager notification interface.
3. Stops ARC routing thread and associated timers.
4. Clears all cached state.

### 2.3 Power State Handling

- Receives `OnPowerModeChanged` callbacks from PowerManager plugin.
- On transition to ON: re-initializes audio ports, restarts ARC detection, sends pending audio device power-on messages.
- On transition to standby/sleep: suspends ARC routing, clears audio device connection state.

---

## 3. Interfaces Implemented

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

---

## 4. API Methods

All methods follow the JSON-RPC 2.0 pattern. Every response includes `"success": true|false`.

### 4.1 Video Display Methods

#### `getConnectedVideoDisplays`

Returns a list of currently connected video display port names.

- **Parameters**: none
- **Response**:
  ```json
  { "connectedVideoDisplays": ["HDMI0"], "success": true }
  ```

#### `getSupportedVideoDisplays`

Returns all video output ports available on the device.

- **Parameters**: none
- **Response**:
  ```json
  { "supportedVideoDisplays": ["HDMI0"], "success": true }
  ```

#### `getSupportedResolutions`

Returns supported resolutions for a given video output port.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `videoDisplay` | string | No | Port name; defaults to platform default port |
- **Response**:
  ```json
  { "supportedResolutions": ["720p", "1080i", "1080p60"], "success": true }
  ```

#### `getSupportedTvResolutions`

Returns TV-reported supported resolutions (from EDID/HDCP negotiation) for a video port.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `videoDisplay` | string | No | Port name; defaults to platform default port |
- **Response**:
  ```json
  { "supportedTvResolutions": ["480i", "720p", "1080i", "1080p", "2160p60"], "success": true }
  ```
- **Notes**: Returns `["none"]` when no resolutions are reported by the TV.

#### `getSupportedSettopResolutions`

Returns resolutions supported by the STB hardware.

- **Parameters**: none
- **Response**:
  ```json
  { "supportedSettopResolutions": ["720p", "1080i", "1080p60"], "success": true }
  ```

#### `getCurrentResolution`

Returns the currently active resolution for a video port, with pixel dimensions.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `videoDisplay` | string | No | Port name; defaults to platform default port |
- **Response**:
  ```json
  { "resolution": "1080p60", "w": 1920, "h": 1080, "progressive": true, "success": true }
  ```
- **Notes**: Resolution string prefix determines pixel dimensions (480→720×480, 576→720×576, 720→1280×720, 768→1366×768, 1080→1920×1080, 2160→3840×2160, 4096x2160→4096×2160). Result is cached until next `resolutionChanged` event.

#### `setCurrentResolution`

Sets the active resolution for a video port.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `videoDisplay` | string | Yes | Port name |
  | `resolution` | string | Yes | Resolution string e.g. `"1080p60"` |
  | `persist` | boolean | No | Whether to persist across reboots; default `true` |
  | `ignoreEdid` | boolean | No | Skip EDID validation; default `false` |
- **Response**: `{ "success": true }`
- **Events**: `resolutionPreChange`, then `resolutionChanged`

#### `getDefaultResolution`

Returns the default (factory/fallback) resolution for the default video port.

- **Parameters**: none
- **Response**:
  ```json
  { "defaultResolution": "720p", "success": true }
  ```

#### `getZoomSetting`

Returns the current zoom (DFC) setting.

- **Parameters**: none
- **Response**:
  ```json
  { "zoomSetting": "FULL", "success": true }
  ```
- **Notes**: Zoom setting is persisted to `/opt/persistent/rdkservices/zoomSettings.json`.

#### `setZoomSetting`

Sets the zoom (DFC) mode.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `zoomSetting` | string | Yes | Zoom mode: `"FULL"`, `"NONE"`, etc. |
- **Response**: `{ "success": true }`
- **Events**: `zoomSettingUpdated`

#### `getActiveInput`

Returns whether the connected display is reporting the STB as its active (primary) input.

- **Parameters**: none
- **Response**:
  ```json
  { "activeInput": true, "success": true }
  ```

#### `getCurrentOutputSettings`

Returns the current video output signal parameters.

- **Parameters**: none
- **Response**:
  ```json
  {
    "videoEOTF": 1,
    "matrixCoefficients": 0,
    "colorSpace": 3,
    "colorDepth": 10,
    "quantizationRange": 4,
    "success": true
  }
  ```

#### `setForceHDRMode`

Forces a specific HDR mode on the video output.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `hdrMode` | boolean | Yes | Enable/disable forced HDR mode |
- **Response**: `{ "success": true }`

#### `getTvHDRSupport`

Returns HDR standards supported by the connected TV.

- **Parameters**: none
- **Response**:
  ```json
  { "standards": ["HDR10", "HLG"], "supportsHDR": true, "success": true }
  ```

#### `getSettopHDRSupport`

Returns HDR standards supported by the STB hardware.

- **Parameters**: none
- **Response**:
  ```json
  { "standards": ["HDR10", "DolbyVision"], "supportsHDR": true, "success": true }
  ```

#### `getTVHDRCapabilities`

Returns a bitmask of HDR capabilities reported by the TV.

- **Parameters**: none
- **Response**:
  ```json
  { "capabilities": 3, "success": true }
  ```
- **Notes**: Bitmask values match `dsHDRStandard_t` enum (e.g. `dsHDRSTANDARD_HDR10=1`, `dsHDRSTANDARD_HLG=2`).

#### `getVideoFormat`

Returns the current incoming video format/HDR standard.

- **Parameters**: none
- **Response**:
  ```json
  { "videoFormat": "HDR10", "success": true }
  ```

#### `isConnectedDeviceRepeater`

Returns whether the connected downstream HDMI device is an HDMI repeater.

- **Parameters**: none
- **Response**:
  ```json
  { "connected": false, "success": true }
  ```

#### `readEDID`

Returns the raw EDID data from the connected display.

- **Parameters**: none
- **Response**:
  ```json
  { "EDID": "<base64-encoded-EDID>", "success": true }
  ```

#### `readHostEDID`

Returns the EDID data advertised by the host (STB).

- **Parameters**: none
- **Response**:
  ```json
  { "EDID": "<base64-encoded-EDID>", "success": true }
  ```

#### `setPreferredColorDepth` / `getPreferredColorDepth`

Gets or sets the preferred color depth for the video output.

- **Set Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `videoDisplay` | string | Yes | Port name |
  | `colorDepth` | string | Yes | e.g. `"8bit"`, `"10bit"`, `"12bit"`, `"auto"` |
- **Get Response**:
  ```json
  { "colorDepth": "10bit", "success": true }
  ```

#### `getColorDepthCapabilities`

Returns the color depth values supported by the video output port.

- **Parameters**: none
- **Response**:
  ```json
  { "capabilities": ["8bit", "10bit"], "success": true }
  ```

#### `setScartParameter`

Configures SCART output parameters (applicable to devices with SCART port).

- **Parameters**: Platform-specific SCART parameter object.
- **Response**: `{ "success": true }`

---

### 4.2 Audio Port Methods

#### `getConnectedAudioPorts`

Returns currently connected audio output ports.

- **Parameters**: none
- **Response**:
  ```json
  { "connectedAudioPorts": ["HDMI0"], "success": true }
  ```
- **Notes**: `HDMI_ARC0` is included only when an ARC/eARC audio device is detected as connected and active.

#### `getSupportedAudioPorts`

Returns all audio output ports available on the device.

- **Parameters**: none
- **Response**:
  ```json
  { "supportedAudioPorts": ["HDMI0", "SPDIF0", "HDMI_ARC0", "HEADPHONE0"], "success": true }
  ```

#### `getSupportedAudioModes`

Returns audio modes (stereo, surround, etc.) supported by an audio port.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `audioPort` | string | Yes | Port name e.g. `"HDMI0"` |
- **Response**:
  ```json
  { "supportedAudioModes": ["STEREO", "SURROUND", "DOLBYDIGITAL"], "success": true }
  ```

#### `getSoundMode` / `setSoundMode`

Gets or sets the active audio mode for an audio port.

- **Get Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `audioPort` | string | No | Port name; defaults to `HDMI0` or first connected port |
- **Get Response**:
  ```json
  { "soundMode": "AUTO (Dolby Digital 5.1)", "success": true }
  ```
- **Set Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `audioPort` | string | Yes | Port name |
  | `soundMode` | string | Yes | Mode string |
  | `persist` | boolean | No | Persist setting; default `true` |
- **Set Response**: `{ "success": true }`

#### `setEnableAudioPort` / `getEnableAudioPort`

Enables or disables an audio output port. Persistence is handled via DS HAL.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `audioPort` | string | Yes | Port name |
  | `enable` | boolean | Yes (set only) | Enable or disable |
- **Supported ports**: `IDLR0`, `HDMI0`, `SPDIF0`, `SPEAKER0`, `HDMI_ARC0`, `HEADPHONE0`
- **Events**: `audioPortEnableStatusChanged`

#### `getAudioFormat`

Returns the current audio format being decoded/passed through.

- **Parameters**: none
- **Response**:
  ```json
  { "audioFormat": "DOLBY AC-3", "success": true }
  ```
- **Notes**: Format values correspond to `dsAudioFormat_t` enum values (e.g. `dsAUDIO_FORMAT_NONE`, `dsAUDIO_FORMAT_AC3`, `dsAUDIO_FORMAT_EAC3`, `dsAUDIO_FORMAT_DOLBY_TRUEHD`, etc.).

#### `getAudioDelay` / `setAudioDelay`

Gets or sets audio output delay in milliseconds (lip-sync adjustment).

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `audioPort` | string | Yes | Port name |
  | `audioDelay` | integer | Yes (set only) | Delay in ms |
- **Response**:
  ```json
  { "audioDelay": 0, "success": true }
  ```

#### `getSinkAtmosCapability`

Returns whether the sink (TV/AV receiver) supports Dolby Atmos.

- **Parameters**: none
- **Response**:
  ```json
  { "atmos_capability": 2, "success": true }
  ```
- **Notes**: Value maps to `dsATMOSCapability_t` enum.

#### `setAudioAtmosOutputMode`

Enables or disables Atmos output passthrough.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `enable` | boolean | Yes | Enable Atmos output |
- **Response**: `{ "success": true }`

#### `setGain` / `getGain`

Gets or sets audio output gain on a port.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `audioPort` | string | Yes | Port name |
  | `gain` | number | Yes (set only) | Gain value (dB) |
- **Response**: `{ "gain": -2.0, "success": true }`

#### `setMuted` / `getMuted`

Gets or sets the mute state of an audio port.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `audioPort` | string | Yes | Port name |
  | `muted` | boolean | Yes (set only) | Mute state |
- **Response**: `{ "muted": false, "success": true }`
- **Events**: `muteStatusChanged`

#### `setVolumeLevel` / `getVolumeLevel`

Gets or sets the volume level (0–100) of an audio port.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `audioPort` | string | Yes | Port name |
  | `volumeLevel` | number | Yes (set only) | Volume level 0–100 |
- **Response**: `{ "volumeLevel": 50.0, "success": true }`
- **Events**: `volumeLevelChanged`

#### `getSettopAudioCapabilities`

Returns the audio capability bitmask of the STB for a given port.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `audioPort` | string | Yes | Port name |
- **Response**: `{ "AudioCapabilities": 3, "success": true }`

#### `getSettopMS12Capabilities`

Returns the MS12 audio processing capability flags of the STB.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `audioPort` | string | Yes | Port name |
- **Response**: `{ "MS12Capabilities": 7, "success": true }`

#### `getSupportedMS12Config`

Returns supported MS12 configuration modes.

- **Parameters**: none
- **Response**: `{ "ms12config": [...], "success": true }`

---

### 4.3 MS12 Audio Processing Methods

> These methods are only effective when the DS HAL supports Dolby MS12 audio processing.

#### `setMS12AudioCompression` / `getMS12AudioCompression`

Controls MS12 dynamic range compression level.

- **Parameters**:
  | Name | Type | Required | Description |
  |------|------|----------|-------------|
  | `audioPort` | string | Yes | Port name |
  | `compresionLevel` | integer | Yes (set only) | 0–10 |
- **Response**: `{ "compressionLevel": 5, "success": true }`

#### `setDolbyVolumeMode` / `getDolbyVolumeMode`

Enables or disables Dolby Volume levelling.

- **Parameters**: `audioPort` (string), `dolbyVolumeMode` (boolean, set only)
- **Response**: `{ "dolbyVolumeMode": true, "success": true }`

#### `setDialogEnhancement` / `getDialogEnhancement` / `resetDialogEnhancement`

Controls dialog enhancement level for speech clarity.

- **Set Parameters**: `audioPort` (string), `enhancementLevel` (integer 0–16)
- **Get Response**: `{ "enhancementLevel": 0, "success": true }`
- **Reset**: Restores platform default; no parameters required.

#### `setIntelligentEqualizerMode` / `getIntelligentEqualizerMode`

Controls MS12 intelligent equalizer mode.

- **Parameters**: `audioPort` (string), `intelligentEqualizerMode` (integer, set only)
- **Response**: `{ "intelligentEqualizerMode": 0, "success": true }`

#### `setGraphicEqualizerMode` / `getGraphicEqualizerMode`

Controls MS12 graphic equalizer mode.

- **Parameters**: `audioPort` (string), `graphicEqualizerMode` (integer, set only)
- **Response**: `{ "graphicEqualizerMode": 0, "success": true }`

#### `setMS12AudioProfile` / `getMS12AudioProfile` / `getSupportedMS12AudioProfiles`

Manages MS12 audio profiles (e.g. Movie, Music, Sports).

- **Set Parameters**: `audioPort` (string), `ms12AudioProfile` (string)
- **Get Response**: `{ "ms12AudioProfile": "Movie", "success": true }`
- **Supported Profiles Response**: `{ "supportedMS12AudioProfiles": ["Movie", "Music", "Sports", "OFF"], "success": true }`

#### `getVolumeLeveller` / `setVolumeLeveller` / `resetVolumeLeveller`

Controls MS12 volume leveller. API version 2 uses an extended object format.

- **v1 Set Parameters**: `audioPort` (string), `mode` (integer 0=off, 1=on, 2=auto)
- **v2 Set Parameters**: `audioPort` (string), `enable` (boolean), `level` (integer 0–10)
- **v1 Get Response**: `{ "enable": true, "level": 5, "success": true }`
- **v2 Get Response**: `{ "volumeLeveller": { "enable": true, "level": 5 }, "success": true }`

#### `getBassEnhancer` / `setBassEnhancer` / `resetBassEnhancer`

Controls bass enhancement boost level.

- **Parameters**: `audioPort` (string), `bassBoost` (integer 0–100, set only)
- **Response**: `{ "bassBoost": 50, "success": true }`

#### `isSurroundDecoderEnabled` / `enableSurroundDecoder`

Controls surround sound decoder pass-through.

- **Enable Parameters**: `audioPort` (string), `surroundDecoderEnabled` (boolean)
- **Query Response**: `{ "surroundDecoderEnabled": true, "success": true }`

#### `getDRCMode` / `setDRCMode`

Gets or sets dynamic range control mode.

- **Parameters**: `audioPort` (string), `DRCMode` (integer 0=Line, 1=RF, set only)
- **Response**: `{ "DRCMode": 0, "success": true }`

#### `getSurroundVirtualizer` / `setSurroundVirtualizer` / `resetSurroundVirtualizer`

Controls surround sound virtualizer (headphone virtualization).

- **v1 Set Parameters**: `audioPort` (string), `enable` (boolean), `boost` (integer 0–96)
- **v2 Set Parameters**: Same structure, extended object response.
- **v2 Get Response**: `{ "surroundVirtualizer": { "enable": true, "boost": 50 }, "success": true }`

#### `getMISteering` / `setMISteering`

Controls Media Intelligence (MI) steering for adaptive audio.

- **Parameters**: `audioPort` (string), `MISteering` (boolean, set only)
- **Response**: `{ "MISteering": false, "success": true }`

#### `setMS12ProfileSettingsOverride`

Overrides specific MS12 profile settings at runtime.

- **Parameters**: Profile key-value overrides (platform-specific).
- **Response**: `{ "success": true }`

---

### 4.4 Associated Audio Methods

#### `setAssociatedAudioMixing` / `getAssociatedAudioMixing`

Controls the mixing of associated audio (AD/secondary audio) with the primary audio stream.

- **Parameters**: `audioPort` (string), `mixing` (boolean, set only)
- **Response**: `{ "mixing": true, "success": true }`
- **Events**: `associatedAudioMixingChanged`

#### `setFaderControl` / `getFaderControl`

Sets the balance fader between primary and secondary (AD) audio streams.

- **Parameters**: `audioPort` (string), `mixerBalance` (integer -32 to 32, set only)
- **Response**: `{ "mixerBalance": 0, "success": true }`
- **Events**: `faderControlChanged`

#### `setPrimaryLanguage` / `getPrimaryLanguage`

Gets or sets the preferred primary audio language (ISO 639-2).

- **Parameters**: `primaryLanguage` (string, 3-char ISO code, set only)
- **Response**: `{ "lang": "eng", "success": true }`
- **Events**: `primaryLanguageChanged`

#### `setSecondaryLanguage` / `getSecondaryLanguage`

Gets or sets the preferred secondary audio language for AD streams (ISO 639-2).

- **Parameters**: `secondaryLanguage` (string, 3-char ISO code, set only)
- **Response**: `{ "lang": "fra", "success": true }`
- **Events**: `secondaryLanguageChanged`

---

## 5. Events

All events are JSON-RPC notifications sent to subscribed clients.

| Event Name | Payload | Trigger |
|------------|---------|---------|
| `resolutionPreChange` | `{}` | Before resolution change begins |
| `resolutionChanged` | `{ "width": int, "height": int }` | Resolution change completed |
| `zoomSettingUpdated` | `{ "zoomSetting": string }` | Zoom/DFC mode changed |
| `activeInputChanged` | `{ "activeInput": bool }` | Display reports STB as active/inactive input |
| `connectedVideoDisplaysUpdated` | `{ "connectedVideoDisplays": string[] }` | HDMI hot-plug connect/disconnect |
| `connectedAudioPortUpdated` | `{ "HotpluggedAudioPort": string, "isConnected": bool }` | Audio port connect/disconnect |
| `audioFormatChanged` | `{ "audioFormat": string }` | Incoming audio format changed |
| `AtmosCapabilityChanged` | `{ "atmos_capability": int }` | Sink Atmos capability changed |
| `videoFormatChanged` | `{ "videoFormat": string }` | Incoming HDR/video format changed |
| `associatedAudioMixingChanged` | `{ "mixing": bool }` | AD mixing state changed |
| `faderControlChanged` | `{ "mixerBalance": int }` | Fader control changed |
| `primaryLanguageChanged` | `{ "primaryLanguage": string }` | Primary audio language changed |
| `secondaryLanguageChanged` | `{ "secondaryLanguage": string }` | Secondary audio language changed |
| `muteStatusChanged` | `{ "muted": bool, "audioPort": string }` | Mute state changed on a port |
| `volumeLevelChanged` | `{ "volumeLevel": float, "audioPort": string }` | Volume level changed on a port |
| `audioPortEnableStatusChanged` | `{ "audioPort": string, "enabled": bool }` | Audio port enable state changed |

---

## 6. Architecture & Integration

### 6.1 Component Diagram

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

### 6.2 ARC/eARC Routing State Machine

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

### 6.3 Timers

| Timer | Timeout | Purpose |
|-------|---------|---------|
| `m_timer` | `RECONNECTION_TIME_IN_MILLISECONDS` (5500ms) | HdmiCecSink reconnection |
| `m_AudioDeviceDetectTimer` | `AUDIO_DEVICE_CONNECTION_CHECK_TIME_IN_MILLISECONDS` (3000ms) | Verify audio device connectivity |
| `m_SADDetectionTimer` | `SAD_UPDATE_CHECK_TIME_IN_MILLISECONDS` (3000ms) | Short Audio Descriptor update |
| `m_ArcDetectionTimer` | `ARC_DETECTION_CHECK_TIME_IN_MILLISECONDS` (1000ms) | ARC device presence polling |
| `m_AudioDevicePowerOnStatusTimer` | `AUDIO_DEVICE_POWER_TRANSITION_TIME_IN_MILLISECONDS` (1000ms) | AVR power state transition |

### 6.4 Caching

| Cache Variable | Invalidation |
|----------------|-------------|
| `currentResolutionCache` | On `resolutionChanged` event |
| `isDisplayConnectedCacheUpdated` | On HDMI hot-plug event |
| `stbHDRcapabilitiesCache` | On HDR capability change |
| `audioPortEnableStatusMap` | On `setEnableAudioPort` or `audioPortEnableStatusChanged` |

### 6.5 Persistent Storage

| Data | Location |
|------|----------|
| Zoom setting | `/opt/persistent/rdkservices/zoomSettings.json` |
| Audio port enable state | DS HAL persistence (`getEnablePersist`/`setEnablePersist`) |
| Audio mode / MS12 settings | DS HAL persistence |
| Color depth | DS HAL persistence |

---

## 7. Dependencies

| Dependency | Purpose |
|-----------|---------|
| DS HAL (`libdshal`) | Device settings hardware abstraction — video/audio port control |
| IARM Bus | System-wide event bus for cross-component signalling |
| RFC API (`rfcapi.h`, `tr181api.h`) | Runtime feature flag access (e.g. `RFC_PWRMGR2`) |
| `org.rdk.HdmiCecSink` | ARC/eARC initiation, Short Audio Descriptor exchange, CEC events |
| `org.rdk.PowerManager` | Power state change notifications |
| `tptimer` | Lightweight repeating/one-shot timer utility |

---

## 8. Configuration

| Config Key | Default | Description |
|-----------|---------|-------------|
| `callsign` | `org.rdk.DisplaySettings` | Plugin registration name |
| `autostart` | Build-time (`PLUGIN_DISPLAYSETTINGS_AUTOSTART`) | Auto-activate on framework start |
| `startuporder` | Build-time (`PLUGIN_DISPLAYSETTINGS_STARTUPORDER`) | Startup ordering relative to other plugins |
| `preconditions` | `Platform` | Required precondition before plugin starts |

---

## 9. API Version Compatibility

The plugin registers handlers for **version 1** (default) and **version 2** (`CreateHandler({2})`). Version 2 overrides the following methods with extended object formats:

| Method | v1 Format | v2 Format |
|--------|-----------|-----------|
| `getVolumeLeveller` | flat `enable`/`level` fields | `volumeLeveller: { enable, level }` object |
| `setVolumeLeveller` | flat fields | object parameter |
| `getSurroundVirtualizer` | flat `enable`/`boost` fields | `surroundVirtualizer: { enable, boost }` object |
| `setSurroundVirtualizer` | flat fields | object parameter |

Clients should use `org.rdk.DisplaySettings.2` callsign for v2 methods.

---

## 10. Open Questions

> These are items that need clarification before full specification can be considered complete.

1. **Zoom setting values**: What is the complete enumeration of valid `zoomSetting` strings beyond `"FULL"` and `"NONE"`? Are these platform-dependent?
2. **`setScartParameter` signature**: The parameter schema for SCART configuration is not documented. What fields are required/optional?
3. **`setMS12ProfileSettingsOverride` schema**: What key-value pairs are valid? Is this platform-specific?
4. **ARC/eARC Volume Control**: When `m_arcEarcAudioEnabled` is true, volume/mute commands appear to be forwarded to the ARC device via CEC. Is this behaviour fully specified?
5. **`getSupportedMS12Config` response schema**: Exact format of the `ms12config` array elements needs confirmation.
6. **`IDeviceOptimizeStateActivator::Request`**: What state values are accepted and what is the expected effect on audio/video output?
7. **`audioPortEnableStatusMap` initialization**: Ports `IDLR0`, `HDMI0`, `SPDIF0`, `SPEAKER0`, `HDMI_ARC0`, `HEADPHONE0` are hardcoded. How are platforms with different port configurations handled?
8. **EDID encoding**: Is the EDID returned by `readEDID` raw binary (base64), hex-encoded, or another format?
