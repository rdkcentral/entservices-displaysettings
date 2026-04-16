# DisplaySettings Plugin Specification

## 1. Overview

The DisplaySettings plugin provides APIs to query and configure display-related properties on RDK-based devices. It exposes Thunder/JSON-RPC methods for managing video output, supported resolutions, zoom, and display modes.

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
    +-----------+------------------+
                |
      API Calls / IARM Events
                |
    +-----------v------------+
    | Device Host Abstraction|
    +-----------+------------+
                |
         HDMI/Panel/EDID
                |
    +-----------v------------+
    |   Display Hardware     |
    +------------------------+


## 2. Capabilities

- Query connected video displays
- Query and set supported resolutions (per display port, EDID-aware)
- Query/set current resolution
- Query supported video displays
- Query/set zoom settings
- Query supported TV/settop resolutions
- Read EDID data
- Query HDR support and output settings

---

## 3. APIs

| Method Name                        | Description                                      | Params                | Returns                |
|-------------------------------------|--------------------------------------------------|-----------------------|------------------------|
| getConnectedVideoDisplays          | List connected video displays                    | —                     | Array of display names |
| getSupportedResolutions            | List supported resolutions for a display port    | videoDisplay (opt)    | Array of resolutions   |
| getCurrentResolution / setCurrentResolution | Get/set current resolution                | videoDisplay, resolution, persist | Resolution/status      |
| getSupportedVideoDisplays          | List supported video displays                    | —                     | Array of display names |
| getSupportedTvResolutions          | List supported TV resolutions                    | videoDisplay (opt)    | Array of resolutions   |
| getSupportedSettopResolutions      | List set-top box resolutions                     | —                     | Array of resolutions   |
| getZoomSetting / setZoomSetting    | Get/set zoom setting                             | videoDisplay, zoomSetting | Zoom info/status   |
| readEDID / readHostEDID            | Read EDID data                                   | videoDisplay          | EDID data              |
| getTvHDRSupport / getSettopHDRSupport | Get HDR support info                        | —                     | HDR info               |
| getCurrentOutputSettings           | Get current output settings                      | —                     | Output settings        |
| setForceHDRMode                    | Force HDR mode                                   | mode                  | Status                 |
| getDefaultResolution               | Get default resolution for a display             | videoDisplay (opt)    | Resolution             |
| getVideoFormat                     | Get video format info                            | videoDisplay (opt)    | Format info            |

---

## 4. Data Models

- **Resolution:** e.g., "720p", "1080i", "2160p60"
- **VideoDisplay:** e.g., "HDMI0", "HDMI1"
- **ZoomSetting:** e.g., "normal", "zoom", "stretch"
- **EDID Data:** Raw or parsed EDID structure
- **HDR Info:** Supported HDR types, capabilities

---

## 5. Behavior & Constraints

- For external displays, supported resolutions are read from EDID.
- For built-in displays, resolutions are platform-defined.
- If no display is connected, an empty list is returned.
- Setting resolution may require a "persist" flag to save the setting.
- All methods return a "success" boolean and relevant data or error info.

---

## 6. Example: getSupportedResolutions

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "org.rdk.DisplaySettings.1.getSupportedResolutions",
  "params": { "videoDisplay": "HDMI0" }
}
```

**Response:**
```json
{
  "success": true,
  "supportedResolutions": ["720p", "1080i", "1080p60"]
}
```

---

## 7. Error Handling

- If the display port is not connected or invalid, returns an empty array.
- Standard JSON-RPC error codes for invalid parameters or internal errors.
