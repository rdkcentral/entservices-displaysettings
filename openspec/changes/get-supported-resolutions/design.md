## Context

`getSupportedResolutions` is an existing JSON-RPC method of the `org.rdk.DisplaySettings` Thunder plugin (callsign `org.rdk.DisplaySettings`). It bridges the WPEFramework middleware layer with the Device Settings (DS) HAL via `device::Host` to query video output port capabilities.

The current specification describes a single-sentence behavior with no documented response for the no-display case. Platform teams have encountered inconsistencies:

- The original implementation used `device::VideoOutputPortConfig::getPortType().getSupportedResolutions()`, which returns a static platform-defined list regardless of what display is physically connected.
- When no external display is connected, some implementations return a default list while others return an empty list, leading to client-side confusion.

The fix aligns the implementation with display-reported capability data to ensure accuracy across display hot-plug events.

## Goals / Non-Goals

**Goals:**
- Switch resolution data source from the static platform list to EDID-sourced data via `vPort.getSupportedTvResolutions()` (the same HAL API used by `getSupportedTvResolutions`).
- Mandate an empty `supportedResolutions` array when no display is connected on the requested port.
- Require the list to reflect the connected display's capabilities after a hot-plug event (live query, no application-level cache for the resolution list).

**Non-Goals:**
- No changes to the JSON-RPC method signature, response schema, or callsign.
- No changes to any other `DisplaySettings` method.
- No new HAL APIs — the existing `getSupportedTvResolutions` HAL call is reused.
- No separate treatment of built-in vs. external ports; the same code path applies to all.

## Decisions

### Decision 1 — Return empty list (not an error) when no display is connected

**Choice**: Return `{ "supportedResolutions": [], "success": true }`.

**Rationale**: Returning an empty array is a safe, non-breaking sentinel that clients can already handle (they check array length). Returning an error code would be a breaking change for existing callers that assume a `200`-style success when no display is attached.

**Alternatives considered**:
- Return `["none"]` (consistent with `getSupportedTvResolutions`) — rejected because `"none"` is not a valid resolution string in the context of `getSupportedResolutions`; it would conflict with caller filtering logic.
- Return an error object — rejected because it breaks backward compatibility.

### Decision 2 — Use `getSupportedTvResolutions` bitmask as the resolution data source

**Choice**: Replace the static `VideoOutputPortConfig::getPortType().getSupportedResolutions()` query with `vPort.getSupportedTvResolutions(&tvResolutions)` and decode the bitmask into resolution strings.

**Rationale**: The `getSupportedTvResolutions` HAL call reflects what the connected display's EDID actually advertises. The previous static list was a platform compile-time capability set that did not update on display change. Using the same bitmask approach as the existing `getSupportedTvResolutions` JSON-RPC method ensures consistency and reuses a proven code pattern.

**Alternatives considered**:
- Keep `VideoOutputPortConfig::getSupportedResolutions()` — rejected because it returns a fixed list that does not reflect the connected display's EDID; inaccurate for external displays.
- Maintain separate paths for built-in vs. external ports — rejected because `getSupportedTvResolutions` works correctly for all port types and the no-display guard already handles the empty-list case.

### Decision 3 — No application-level cache for the supported resolution list

**Choice**: `getSupportedResolutions` queries the HAL on every call. The `isDisplayConnectedCacheUpdated` flag (reset by `OnDisplayHDMIHotPlug`) ensures any re-check of connection state is fresh after a hot-plug event.

**Rationale**: The result naturally reflects live EDID data without additional caching infrastructure. The `getSupportedTvResolutions` HAL call is inexpensive. Adding a separate bitmask cache would duplicate state already managed implicitly by the HAL.

**Alternatives considered**:
- Cache the bitmask result and invalidate on `OnDisplayHDMIHotPlug` — not needed; HAL is the source of truth and is queried on every call already.

## Risks / Trade-offs

- **[Risk] Existing callers that relied on the static platform list for non-EDID ports** → The static list was never spec'd; transitioning to EDID data is the correct behavior. Empty-list is returned when disconnected, which is a documented contract.
- **[Risk] Platform HAL `getSupportedTvResolutions` returning 0 for unsupported ports** → The LOGWARN branch handles bitmask=0; the result is an empty resolution list rather than `["none"]` (which `getSupportedTvResolutions` JSON-RPC uses). Documented in spec.
- **[Risk] YAML parse warning in `openspec/config.yaml`** → Cosmetic, does not impact CLI operation; tracked separately.
