# OpenSpec Coverage Report — DisplaySettings Plugin

**Generated:** 2026-04-23
**Spec File:** openspec/specs/displaysettings-spec.md
**Plugin Version:** 2.0.5

---

## Overall Score: 44 / 100

| Category | Score | Max |
|---|---|---|
| Code to Spec Coverage | 5 | 40 |
| Architecture HLA Specification | 9 | 10 |
| External Interface Specification | 9 | 10 |
| Security Specification | 5 | 10 |
| Versioning & Compatibility Specification | 7 | 10 |
| Conformance Testing Automation & Validation | 5 | 10 |
| Performance Specification | 4 | 10 |
| **TOTAL** | **44** | **100** |

---

## Category Breakdown

### 1. Code to Spec Coverage (5 / 40)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Reference Coverage | 0 | 20 | No `// Spec:` comment found in any of the 4 plugin source files (`DisplaySettings.cpp`, `DisplaySettings.h`, `Module.cpp`, `Module.h`) |
| Spec Existence | 0 | 10 | No spec references exist in code; cannot resolve any references |
| Spec Completeness | 5 | 5 | `openspec/specs/displaysettings-spec.md` exists and contains all required sections: Title (H1) ✓, Description ✓, Requirements ✓ |
| No Orphaned Code | 0 | 5 | All 4 plugin source files are orphaned — none contain a `// Spec:` reference comment |

**Findings:**

Files scanned under `plugin/`:

| File | Has `// Spec:` Reference? |
|---|---|
| `plugin/DisplaySettings.cpp` | No |
| `plugin/DisplaySettings.h` | No |
| `plugin/Module.cpp` | No |
| `plugin/Module.h` | No |

The spec file `openspec/specs/displaysettings-spec.md` is complete and well-structured with Title, Description, and Requirements sections present. However, the codebase has no `// Spec:` annotation comments in any source file. The coverage score for code-to-spec traceability is therefore the minimum possible given a complete spec exists.

---

### 2. Architecture HLA Specification (9 / 10)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Presence of HLA Spec | 3 | 3 | `## Architecture / Design` section present in spec |
| Clarity of Architecture Diagrams | 3 | 3 | Mermaid `graph TD` diagram present with labeled nodes and arrows covering all major flows |
| Component/Module Mapping | 2 | 2 | All major subsystems mapped: Client, Thunder Framework, Video Control Module, Audio Control Module, ARC/eARC State Machine, Power Notification Handler, DS HAL (Video/Audio/MS12 APIs), IARM Bus, HdmiCecSink, PowerManager, Hardware Layer |
| Traceability to Code | 1 | 2 | Component Summary table lists code files, but diagram node labels use logical names only (e.g., "Video Control Module") without directly referencing C++ class names (`DisplaySettings`) or file paths; partial traceability |

**Findings:**

The Mermaid architecture diagram is detailed and complete with labeled data-flow arrows (JSON-RPC, DS API Call, CEC Events via IARM, etc.). The Component Summary table provides mapping between diagram components and their roles. One point deducted because the diagram does not explicitly cross-reference C++ file paths or class names (e.g., `plugin/DisplaySettings.cpp: DisplaySettings::Initialize`), reducing direct code-to-diagram traceability.

---

### 3. External Interface Specification (9 / 10)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Presence of Interface Spec | 3 | 3 | `## External Interfaces` section present with 9 grouped API tables covering all method categories |
| Defined Inputs/Outputs | 3 | 3 | All methods specify parameter name, type (string/bool/int/float), required/optional status, return fields, and error conditions |
| Documentation Completeness | 1 | 2 | 82 of 83 registered methods are documented in the spec; `getSupportedResolutions` (registered at `DisplaySettings.cpp:235`) is absent (intentionally excluded from spec as it originated in archived change `2026-04-14-get-supported-resolutions`) |
| Validation/Examples | 2 | 2 | 6 worked JSON-RPC examples present with full request and response bodies |

**Findings:**

All 83 registered methods were identified by scanning `registerMethodLockedApi` and `Utils::Synchro::RegisterLockedApi` calls in `plugin/DisplaySettings.cpp` (lines 231–323). The v1/v2 variant pattern for `getVolumeLeveller`, `setVolumeLeveller`, `getSurroundVirtualizer`, `setSurroundVirtualizer` is implemented via Thunder handler versioning (handler 1 = v1, handler 2 = v2) and correctly documented in the spec.

One method registered in code is absent from the spec:

| Method | Registration Line | Reason Absent |
|---|---|---|
| `getSupportedResolutions` | `DisplaySettings.cpp:235` | Intentionally excluded — introduced by archived change `openspec/changes/archive/2026-04-14-get-supported-resolutions/` |

The `getCurrentOutputSettings` response schema is documented only as "Comprehensive settings object" in the spec without a formal JSON field definition (flagged in Open Queries).

---

### 4. Security Specification (5 / 10)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Presence of Security Spec | 3 | 3 | `## Security` section present in spec |
| Threat Model/Analysis | 1 | 3 | Section mentions "intra-device only" attack surface and Thunder sandboxing model, but provides no explicit threat model, attack surface analysis, or named threat categories |
| Security Requirements | 1 | 2 | `Utils::Synchro::RegisterLockedApiForHandler` cited as the concurrency protection mechanism; no formal access-control requirements, trust levels, or privilege-separation requirements defined |
| Validation/Testing | 0 | 2 | No security tests, penetration testing references, or security validation criteria found in any test file or spec section |

**Findings:**

The spec acknowledges the security gap under Open Queries: _"The specification does not define an explicit security model or access-control policy."_ The concurrency lock mechanism (`registerMethodLockedApi`) is correctly documented, but no threat model, API authorization policy, or security test strategy has been defined. The Security section scores below half of available points entirely because of the absence of a threat model and validation evidence.

---

### 5. Versioning & Compatibility Specification (7 / 10)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Presence of Versioning Spec | 3 | 3 | `## Versioning & Compatibility` section present in spec |
| Versioning Scheme Defined | 2 | 3 | Plugin version 2.0.5 stated; Thunder JSON-RPC namespace `org.rdk.DisplaySettings.1` documented; no semver policy, release cadence, or version bump criteria defined |
| Backward/Forward Compatibility | 2 | 2 | v1/v2 dual-registration pattern documented; additive-only API change policy stated; platform capability query APIs identified for optional feature detection |
| Migration/Upgrade Path | 0 | 2 | No migration guide, upgrade procedure, or deprecation lifecycle documented |

**Findings:**

The spec correctly documents the dual versioning model (v1/v2 Thunder handler versioning) and the additive API policy. The versioning section is functional but incomplete: it does not define when a version bump is required, what constitutes a breaking change, or how consumers should migrate when APIs are eventually deprecated (e.g., `setScartParameter` is noted as legacy with no documented removal timeline).

---

### 6. Conformance Testing Automation & Validation (5 / 10)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Presence of Conformance Tests | 2 | 3 | L2 test file `Tests/L2Tests/tests/DisplaySettings_L2Test.cpp` confirmed present and enabled in `Tests/L2Tests/CMakeLists.txt` via `PLUGIN_DISPLAYSETTINGS` flag; no DisplaySettings-specific L1 test files found in `Tests/L1Tests/` |
| Test Coverage | 1 | 3 | L2 test file contains 1 test case (`DisplaySettings_L2_MethodTest`) covering a subset of methods with mock setup; spec defines coverage goals but actual test-to-requirement traceability is absent |
| Test Documentation | 2 | 2 | `Tests/README.md` is detailed — documents build steps, `act` command usage, GitHub Actions workflow, and local execution instructions |
| Validation Results | 0 | 2 | No pass/fail results, coverage percentages, or test run summaries documented in spec or test files |

**Findings:**

The `Tests/README.md` is comprehensive and covers all aspects of the test execution workflow. However, the L2 test file contains only a single test fixture (`DisplaySettings_L2test`) with one test method (`DisplaySettings_L2_MethodTest`) that exercises several API calls but does not provide systematic per-requirement coverage. No L1 unit test file specific to DisplaySettings was found (the L1 CMakeLists references `test_UtilsFile.cpp` and a macro for plugin tests, but no DisplaySettings L1 test source was present). Validation results are not tracked in any accessible file.

---

### 7. Performance Specification (4 / 10)

| Sub-Criterion | Score | Max | Evidence |
|---|---|---|---|
| Presence of Performance Spec | 3 | 3 | `## Performance` section present in spec |
| Defined Performance Metrics | 1 | 3 | EDID read latency `100–500ms` noted and CEC latency mentioned; no SLA targets, throughput metrics, or measurable bounds for JSON-RPC response times defined; spec itself states "No explicit SLA-bound performance targets are defined" |
| Test Coverage for Performance | 0 | 2 | No performance tests, benchmarks, or latency measurement tests found in `Tests/L1Tests/` or `Tests/L2Tests/` |
| Results & Validation | 0 | 2 | No performance test results or baseline measurements documented |

**Findings:**

The Performance section acknowledges its own incompleteness. The only quantified metric is the EDID hardware read range (100–500ms), which is a hardware characteristic rather than a defined SLA. All other performance characteristics are described qualitatively ("immediate", "fast", "no observable latency"). No performance tests exist in the test suite.

---

## Gaps & Issues

### Unspecified Methods (registered in code but missing from spec)

| Method | File | Line | Reason |
|---|---|---|---|
| `getSupportedResolutions` | `plugin/DisplaySettings.cpp` | 235 | Intentionally excluded from spec — originated in archived change `openspec/changes/archive/2026-04-14-get-supported-resolutions/`. Spec baseline predates this change. |

### Orphaned Code (no `// Spec:` reference comment found)

All plugin source files are orphaned — none contain a `// Spec:` comment:

| File | Status |
|---|---|
| `plugin/DisplaySettings.cpp` | No spec reference comment |
| `plugin/DisplaySettings.h` | No spec reference comment |
| `plugin/Module.cpp` | No spec reference comment |
| `plugin/Module.h` | No spec reference comment |

### Spec Sections with No Code Traceability

| Spec Section | Issue |
|---|---|
| `## Covered Code` — `plugin/DisplaySettings.conf.in` | File exists in `plugin/` and is referenced correctly — no broken reference |
| `## Architecture / Design` — Mermaid diagram component labels | Logical component names (e.g., "Video Control Module") do not map to C++ class names or file names directly |

### Open Queries Affecting Scores

| Open Query (from spec) | Impacted Category | Score Impact |
|---|---|---|
| No explicit security model or access-control policy defined | Security Specification | −4 pts (Threat Model: −2, Security Requirements: −1, Validation: −2) |
| `getCurrentOutputSettings` response schema not formally defined | External Interface Specification | −0.5 pts (Documentation Completeness) |
| `setScartParameter` has no documented deprecation policy | Versioning & Compatibility | −1 pt (Migration/Upgrade Path) |
| No SLA-bound performance targets defined | Performance Specification | −2 pts (Defined Performance Metrics) |

---

## Suggestions for Improvement

### 1. Code to Spec Coverage (5 / 40 → target: 40 / 40)

- **Add `// Spec: displaysettings-spec` comments** to `plugin/DisplaySettings.cpp`, `plugin/DisplaySettings.h`, `plugin/Module.cpp`, and `plugin/Module.h`. A single class-level or file-level comment per file is sufficient to establish reference coverage and would raise the Reference Coverage sub-score from 0 to 20.
- **Add per-method spec references** (e.g., `// Spec: displaysettings-spec#video-display-apis`) above each `registerMethodLockedApi` call group in `DisplaySettings.cpp` for fine-grained traceability. This is the single highest-impact change in the entire report.
- **Add the spec reference to `getSupportedResolutions`** once it is formally included in the baseline spec (after the archived change is properly promoted).

### 2. Architecture HLA Specification (9 / 10 → target: 10 / 10)

- **Add C++ class and file name annotations** to the Mermaid diagram or Component Summary table, e.g., map "Video Control Module" → `plugin/DisplaySettings.cpp: DisplaySettings::getConnectedVideoDisplays` etc. This directly resolves the one missing point for code traceability.

### 3. External Interface Specification (9 / 10 → target: 10 / 10)

- **Define the `getCurrentOutputSettings` response schema** as a typed JSON object in the spec to achieve full documentation completeness and resolve the flagged Open Query.

### 4. Security Specification (5 / 10 → target: 10 / 10)

- **Add a threat model** identifying at minimum: (a) JSON-RPC privilege escalation risk, (b) unauthorized resolution/audio configuration change, (c) CEC spoofing via IARM Bus. This alone would award the missing 2 points for Threat Model.
- **Define access-control requirements**: specify which Thunder security tokens or privilege levels are required for read vs. write methods (e.g., `setCurrentResolution` should require elevated privilege). This resolves the Security Requirements gap.
- **Add a security test case** to the L2 test suite verifying that unauthorized callers cannot invoke write APIs, which would close the Validation/Testing gap.

### 5. Versioning & Compatibility Specification (7 / 10 → target: 10 / 10)

- **Define a version bump policy**: document what constitutes a major, minor, or patch change (e.g., new method = minor bump, breaking parameter change = major bump). This resolves the Versioning Scheme gap.
- **Document a deprecation and migration path** for legacy APIs (`setScartParameter`, v1 volume/virtualizer APIs): add a target removal version and a migration guide pointing consumers to v2 equivalents.

### 6. Conformance Testing Automation & Validation (5 / 10 → target: 10 / 10)

- **Add L1 unit tests for DisplaySettings**: create `Tests/L1Tests/tests/DisplaySettings_L1Test.cpp` with per-method unit tests using mock DS HAL. This resolves the missing L1 test gap and significantly improves Test Coverage score.
- **Expand L2 test coverage**: add individual `TEST_F` cases per API category (video display, audio port, MS12, accessibility) rather than one monolithic test method. Each test case should reference the corresponding spec requirement.
- **Document and track test results**: add a test results summary section to the spec (or a `Tests/results/` directory) capturing pass/fail counts and coverage percentages per test run.

### 7. Performance Specification (4 / 10 → target: 10 / 10)

- **Define measurable SLA targets** for at minimum: (a) JSON-RPC response time for synchronous queries (e.g., ≤ 50ms for cached queries), (b) EDID read maximum bound (e.g., ≤ 500ms), (c) audio enhancement change latency (e.g., ≤ 100ms). Replace qualitative descriptions with numeric targets.
- **Add performance benchmark tests**: instrument the L2 test suite to measure and assert response times for key APIs (e.g., `getConnectedVideoDisplays`, `getCurrentResolution`).
- **Record a performance baseline**: document one set of measured results against an actual platform to populate the Results & Validation sub-criterion.

---

## References

- Spec: `openspec/specs/displaysettings-spec.md`
- Plugin Source: `plugin/DisplaySettings.cpp`, `plugin/DisplaySettings.h`, `plugin/Module.cpp`, `plugin/Module.h`
- Build Config: `plugin/CMakeLists.txt`
- Tests: `Tests/L1Tests/`, `Tests/L2Tests/tests/DisplaySettings_L2Test.cpp`
- Test Documentation: `Tests/README.md`
- Skill: `.github/skills/openspec-coverage/SKILL.md`
- Archived Change (out of scope): `openspec/changes/archive/2026-04-14-get-supported-resolutions/`
