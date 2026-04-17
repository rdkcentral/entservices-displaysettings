## ADDED Requirements

### Requirement: Query supported resolutions via JSON-RPC
The DisplaySettings plugin SHALL provide a JSON-RPC method `getSupportedResolutions` that returns the list of display resolutions supported by the connected or built-in display device.

#### Scenario: Query resolutions for default HDMI port
- **WHEN** client invokes `getSupportedResolutions` without parameters
- **THEN** system returns the list of supported resolutions for HDMI0 port

#### Scenario: Query resolutions for specific video display port
- **WHEN** client invokes `getSupportedResolutions` with `videoDisplay` parameter set to a specific port (e.g., "HDMI1")
- **THEN** system returns the list of supported resolutions for the specified port

### Requirement: Return resolutions from platform capabilities
The system SHALL retrieve the list of supported resolutions from platform capabilities (device layer abstraction) rather than directly from EDID.

#### Scenario: External display connected
- **WHEN** an external display is connected to the specified video port
- **THEN** system queries the device layer and returns the list of supported resolutions based on platform capabilities

#### Scenario: External display changed
- **WHEN** a different external display is connected to the specified video port
- **THEN** system updates and returns supported resolutions based on platform capabilities

**Note:** The underlying device layer may use EDID data as one input to determine platform capabilities, but the API returns platform-supported resolutions, not raw EDID data.

### Requirement: Return platform resolutions for built-in displays
For built-in display panels, the system SHALL return the list of supported resolutions based on platform capabilities.

#### Scenario: Query built-in display resolutions
- **WHEN** client queries resolutions for a built-in display port
- **THEN** system returns the resolutions supported by the built-in display panel as defined by platform capabilities

### Requirement: Handle disconnected displays
The system SHALL return an empty list when no display is connected to the specified video port.

#### Scenario: No display connected
- **WHEN** client queries resolutions for a video port with no connected display
- **THEN** system returns an empty array

#### Scenario: Display disconnected after previous query
- **WHEN** display is disconnected after a previous successful query
- **THEN** subsequent queries return an empty array until a display is reconnected

### Requirement: Method parameters
The `getSupportedResolutions` method SHALL accept an optional parameters object.

#### Scenario: Optional videoDisplay parameter
- **WHEN** client provides a `videoDisplay` parameter in the request
- **THEN** system queries resolutions for the specified video port

#### Scenario: Missing videoDisplay parameter defaults to HDMI0
- **WHEN** client invokes the method without a `videoDisplay` parameter
- **THEN** system defaults to querying resolutions for the HDMI0 port

### Requirement: Return format
The method SHALL return a JSON response containing an array of resolution strings.

#### Scenario: Successful resolution query
- **WHEN** resolutions are available for the specified port
- **THEN** system returns a success response with an array of resolution strings

#### Scenario: Empty resolution list
- **WHEN** no resolutions are available (e.g., no display connected)
- **THEN** system returns a success response with an empty array
