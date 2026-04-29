## ADDED Requirements

### Requirement: getSupportedResolutions returns EDID-sourced resolutions for any connected display port
For any video display port with a display connected, `getSupportedResolutions` SHALL return the list of resolutions as reported by the connected display's EDID data, obtained via the `getSupportedTvResolutions` HAL API.

#### Scenario: External display connected returns EDID resolutions
- **WHEN** a client calls `getSupportedResolutions` with `videoDisplay` set to a port (e.g., `"HDMI0"`)
- **AND** a display is connected to that port
- **THEN** the response SHALL contain `"supportedResolutions"` populated with the resolutions advertised in the display's EDID
- **AND** `"success"` SHALL be `true`

### Requirement: getSupportedResolutions defaults to the platform default port when videoDisplay is omitted
When the optional `videoDisplay` parameter is not specified in the request, `getSupportedResolutions` SHALL default to the platform's default video output port as returned by `device::Host::getDefaultVideoPortName()` (typically `"HDMI0"`).

#### Scenario: No videoDisplay parameter supplied
- **WHEN** a client calls `getSupportedResolutions` without including a `videoDisplay` field in the params object
- **THEN** the method SHALL behave as if `videoDisplay` were set to the platform default port
- **AND** the response SHALL include `"supportedResolutions"` and `"success": true`

### Requirement: getSupportedResolutions returns an empty list when no display is connected
When `getSupportedResolutions` is called for a port and no display is currently connected, the method SHALL return an empty `supportedResolutions` array.

#### Scenario: No display connected on port
- **WHEN** a client calls `getSupportedResolutions` for a port
- **AND** no display is connected to that port
- **THEN** the response SHALL be `{ "supportedResolutions": [], "success": true }`

### Requirement: getSupportedResolutions reflects updated EDID after display reconnection
The supported resolution list SHALL be refreshed when a different display is connected to a port, so that subsequent calls to `getSupportedResolutions` reflect the newly connected display's capabilities.

#### Scenario: Different display connected after initial query
- **WHEN** a client has previously called `getSupportedResolutions` for a port
- **AND** the connected display is replaced with a different display
- **THEN** a subsequent call to `getSupportedResolutions` for that port SHALL return the new display's supported resolutions
- **AND** SHALL NOT return the resolution list from the prior display
