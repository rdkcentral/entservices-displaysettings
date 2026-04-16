## Context

The DisplaySettings plugin is part of the Thunder framework (formerly WPEFramework) for RDK devices. It provides JSON-RPC APIs for display configuration and management. Currently, the plugin lacks a standardized method to query the supported resolutions of connected displays.

The Display Manager (DS) HAL provides low-level access to display hardware and EDID data. The plugin needs to expose this capability through a Thunder JSON-RPC interface while handling multiple video ports (HDMI0, HDMI1, etc.) and different display types (built-in vs. external).

**Constraints:**
- Must follow Thunder plugin architecture and JSON-RPC conventions
- Must use DS HAL APIs for resolution queries
- Must handle both synchronous queries and dynamic display connection events
- Must maintain backward compatibility with existing DisplaySettings APIs

## Goals / Non-Goals

**Goals:**
- Expose a JSON-RPC method `getSupportedResolutions` in the DisplaySettings plugin
- Support querying resolutions for any video display port with HDMI0 as default
- Return resolution data from EDID for external displays
- Return platform-defined resolutions for built-in displays
- Return empty array when no display is connected
- Follow existing DisplaySettings plugin patterns for error handling and response format

**Non-Goals:**
- Real-time EDID monitoring or event notifications (handle queries only, not subscriptions)
- Validation or filtering of resolution capabilities (return raw EDID/platform data)
- Support for non-HDMI display types in this change (future enhancement)
- Persistence of resolution data across reboots

## Decisions

### Decision 1: Use DS HAL `dsGetSupportedVideoCodingFormats()` or equivalent
**Rationale:** The DS HAL provides standardized APIs for querying display capabilities including EDID parsing. Using these APIs ensures:
- Consistent behavior across different SoC platforms
- Proper EDID parsing without reimplementing the logic
- Access to both built-in and external display capabilities

**Alternative Considered:** Direct EDID parsing in the plugin layer was rejected because it would duplicate HAL functionality and reduce portability.

### Decision 2: Default videoDisplay parameter to "HDMI0"
**Rationale:** HDMI0 is the primary display port on most RDK devices. Making it the default:
- Simplifies the most common use case (querying primary display)
- Matches existing DisplaySettings plugin conventions
- Maintains backward compatibility if parameter structure evolves

**Alternative Considered:** Requiring the parameter was rejected to reduce API verbosity for the common case.

### Decision 3: Return empty array instead of error when no display connected
**Rationale:** An empty resolution list is a valid state, not an error condition. This approach:
- Simplifies client logic (no error handling for disconnected displays)
- Aligns with the spec requirement for graceful handling
- Allows clients to distinguish "no resolutions" from API errors

**Alternative Considered:** Returning an error code was rejected because it forces clients to handle a non-error condition as an exception.

### Decision 4: Use existing Thunder JSON-RPC registration pattern
**Rationale:** Follow the established pattern in DisplaySettings.cpp:
- Register method in `Initialize()` using `Register<>()` template
- Implement handler as member function returning `uint32_t`
- Use `JsonObject` for request/response parameters
- Maintain consistency with other DisplaySettings methods

**Alternative Considered:** Using Thunder's newer JSON schema validation was deferred to avoid diverging from current plugin patterns.

### Decision 5: Query DS HAL synchronously in the handler
**Rationale:** Resolution queries are fast operations (reading cached EDID data):
- No need for asynchronous patterns or worker threads
- Reduces complexity and potential race conditions
- Matches performance characteristics of similar DisplaySettings methods

**Alternative Considered:** Caching EDID data in the plugin was rejected because the HAL already caches it, and we'd need to handle invalidation on display changes.

## Risks / Trade-offs

### Risk: DS HAL API availability varies by platform
**Mitigation:** Document required DS HAL version in plugin documentation. Add preprocessor checks for DS HAL API availability if needed.

### Risk: EDID parsing failures on malformed data
**Mitigation:** Rely on DS HAL's error handling. Return empty array if HAL returns error, log warning for debugging.

### Risk: Resolution list may be large for some displays
**Mitigation:** JSON-RPC can handle typical EDID resolution lists (usually <50 entries). Monitor performance in testing.

### Trade-off: No event notification for display changes
**Impact:** Clients must poll to detect resolution changes after display connection events.
**Rationale:** Event subscription is out of scope for this change. Can be added as future enhancement.

### Trade-off: Returning raw EDID data without filtering
**Impact:** Clients receive all EDID-reported resolutions, including potentially unsupported modes.
**Rationale:** Keeps plugin layer simple. Filtering logic (if needed) belongs in the HAL or client application.

## Migration Plan

**Deployment:**
1. Implement and test the new method in DisplaySettings plugin
2. Update plugin configuration if needed (no config changes expected)
3. Deploy with standard Thunder plugin update process
4. No database or state migration required

**Rollback:**
- Removing the method is safe (no existing clients depend on it)
- No state changes to revert

**Testing:**
- Unit tests for parameter handling and response format
- L2 tests for EDID parsing with various display types
- Integration tests with different video ports

## Open Questions

- Should the response include additional metadata (e.g., preferred resolution, refresh rates)?
  - **Resolution:** Start with basic resolution list. Add metadata in future if client needs emerge.

- Should we validate that the requested videoDisplay port exists?
  - **Resolution:** Let DS HAL handle validation. Return empty array for invalid ports (same as disconnected).
