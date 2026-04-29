## ADDED Requirements

### Requirement: getSupportedResolutions returns EDID-sourced resolutions for external display ports
For video display ports connected to an external display, `getSupportedResolutions` SHALL return the list of resolutions as reported by the connected display's EDID data.

#### Scenario: External display connected returns EDID resolutions
- **WHEN** a client calls `getSupportedResolutions` with `videoDisplay` set to an external port (e.g., `"HDMI0"`)
- **AND** an external display is connected to that port
- **THEN** the response SHALL contain `"supportedResolutions"` populated with the resolutions advertised in the display's EDID
- **AND** `"success"` SHALL be `true`

### Requirement: getSupportedResolutions returns platform-defined resolutions for built-in display ports
For video display ports associated with a built-in panel, `getSupportedResolutions` SHALL return the list of resolutions from platform capabilities, independent of any physical connection state.

#### Scenario: Built-in display port returns platform capability list
- **WHEN** a client calls `getSupportedResolutions` with `videoDisplay` set to a built-in panel port
- **THEN** the response SHALL contain `"supportedResolutions"` populated with the platform-defined resolution list for that panel
- **AND** `"success"` SHALL be `true`

### Requirement: getSupportedResolutions defaults to HDMI0 when videoDisplay is omitted
When the optional `videoDisplay` parameter is not specified in the request, `getSupportedResolutions` SHALL default to the `"HDMI0"` port.

#### Scenario: No videoDisplay parameter supplied
- **WHEN** a client calls `getSupportedResolutions` without including a `videoDisplay` field in the params object
- **THEN** the method SHALL behave as if `videoDisplay` were set to `"HDMI0"`
- **AND** the response SHALL include `"supportedResolutions"` and `"success": true`

### Requirement: getSupportedResolutions returns an empty list when no external display is connected
When `getSupportedResolutions` is called for an external display port and no display is currently connected, the method SHALL return an empty `supportedResolutions` array.

#### Scenario: No display connected on external port
- **WHEN** a client calls `getSupportedResolutions` for an external port (e.g., `"HDMI0"`)
- **AND** no display is connected to that port
- **THEN** the response SHALL be `{ "supportedResolutions": [], "success": true }`

### Requirement: getSupportedResolutions reflects updated EDID after display reconnection
The supported resolution list for an external display port SHALL be refreshed when a different display is connected to that port, so that subsequent calls to `getSupportedResolutions` reflect the newly connected display's capabilities.

#### Scenario: Different display connected after initial query
- **WHEN** a client has previously called `getSupportedResolutions` for an external port
- **AND** the connected display is replaced with a different display
- **THEN** a subsequent call to `getSupportedResolutions` for that port SHALL return the new display's supported resolutions
- **AND** SHALL NOT return the previously cached resolution list from the prior display
