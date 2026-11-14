# Implementation architecture

## Simulation core

`src/core/SimLodAndCrime.*` owns fixed-size SoA columns aligned to 64-byte boundaries. The demo initializes persistent NPC IDs, schedule jitter, role speed, route progress, distance, LOD tier, and body handles without growing containers. Each `Tick` visits the fixed population and applies deterministic `(npcId + tickIndex) % period == 0` time slicing. Graph-soul route progress advances as scalar edge progress; schedule clocks wrap over a 24-hour day.

LOD transitions preserve the stored world position. The 128-slot body pool carries transient props, animation state, and combat flags. `Release` scrubs those fields, advances the slot generation, and returns the slot to its free stack. Canonical item ownership remains in `SoulState`.

## Behavior and preemption

`BehaviorObjects.*` keeps a fixed registry of generation-checked handles and small occupant arrays. Navigation doors return fixed subtree-injection frames. `AreaBrain` enforces minimum role cardinality and applies a configured solo fallback. Named Forge, Tavern, Market, and Guardhouse area configurations share that solver. `CrimeArrestSituation` assigns stable accuser, guard, suspect, and witness roles.

`SubbrainArchitecture.*` evaluates L0–L3 priority, selects BDI intentions with a +0.18 commitment bonus and a 20-tick lock, and applies a cleanup tree before a higher layer becomes active. The cleanup releases a station lease by handle, clears the injected frame, and restores any temporary item state to its canonical value.

## Crime evidence

Each witness memory buffer has 16 fixed slots. `RecordAuditoryLockpick` always uses `UnknownActor`; the boolean wall flag affects clarity only. `RecordVisualExit` ignores events without visual line-of-sight. The deduction engine requires both an earlier sound event and a later visible exit, then applies spatial-temporal decay and item-provenance likelihood multipliers. The demo fixture produces approximately 0.89 posterior for actor `#412` carrying item `7001`, whose owner is `#14`.

This is a compact deterministic evidence model, not a calibrated forensic Bayesian network.

## Armor solver

The model tracks plate, maille, and gambeson condition separately. Plate resistance decreases with condition using `base * (0.4 + 0.6 * condition)`. The strike solver computes incidence from the normalized direction/normal dot product, applies the configured glancing rule, propagates energy through plate and maille, attenuates residual blunt energy through the gambeson, and spends stamina before health. `ArmorImpact` reports an energy ledger for each strike. Coefficients are game tuning values and are not attributed to Williams.

## CRYENGINE adapter

`Code/Components/BohemiaAlifeComponents.*` contains four `IEntityComponent` adapters with Schematyc registration. The world adapter is written to run a 30 Hz fixed step, emit `CRY_PROFILE_FUNCTION(PROFILE_AI)` markers, and draw hydrated agents and debug geometry through `IRenderAuxGeom`. These files are outside the standalone CMake build. In this checkout they have been compiled to object files against the installed headers, but the plugin has not been linked or run: matching CRYENGINE import libraries are unavailable here, and `bin/win_x64/Game.dll` remains the engine template binary.

## Verification and artifacts

CMake builds `bohemia_core`, `BohemiaAlifeTests`, `BohemiaShowcaseExporter`, and, on Windows, `BohemiaLiveSim`. The 10 CTest entries cover cleanup, role cardinality, navigation injection, situation roles, LOD hygiene, commitment hysteresis, wall-occluded crime deduction, schedule staggering, armor energy/durability, and steady-state allocation instrumentation.

The exporter writes local hardware timings for 200, 1,000, and 5,000 NPC populations, then renders a 2x supersampled 1920x1080 PNG with the district, LOD zones, evidence chain, armor callout, charts, and inspector. The Win32 live view and the CRYENGINE adapter now read the same authored route graph from `src/core/TownRouteGraph.h`. Benchmark numbers are machine-specific and are generated when the exporter is run.
