# BOHEMIA-ALIFE — portfolio brief

## Short project description

An independent C++20 simulation of a 1,000-person medieval town. The project focuses on bounded simulation work, distance-based NPC LOD, behavior arbitration, crime evidence, and a deterministic layered armor model. A Win32 viewer makes the systems inspectable while they run.

## CV bullets

- Designed a fixed-capacity NPC simulation core with cache-aligned SoA storage, generation-checked body handles, a preallocated 128-body pool, 24-hour schedule clocks, and three LOD tiers with hydration hysteresis.
- Implemented bounded behavior-object leases and cleanup on subbrain preemption, time-sliced schedule updates, and a two-stage crime deduction example combining occluded audio, visible movement, and item provenance.
- Built a dependency-free Win32/GDI interactive viewer for 1,000 moving NPCs, plus a layered armor gameplay model with per-layer energy and condition accounting.

## Measured core timing

The included Release benchmark report records a **0.0243 ms P95** total for the 1,000-NPC measured step (P50: **0.0149 ms**). It was measured on an Intel Core i3-13100F with MSVC 19.51. The timed work is the simulation core and its BDI/area/crime scenarios; it is not a whole-frame game or CRYENGINE benchmark. The result is machine-specific. See [the complete benchmark report](Bohemia-Benchmark-Results.md) for method and other population sizes.

## Demo

Build and run `BohemiaLiveSim` using the commands in the root [README](../README.md). Move the camera focus across the route layout to inspect LOD transitions. Use `1`, `2`, and `3` to trigger the crime, tavern assignment, and armor examples.

## Integration status

The standalone viewer and simulation core are the runnable deliverable. CRYENGINE 5.7 Schematyc adapter source and a sample level are included, but the adapters have not been linked into a custom game plugin or verified in Sandbox in this checkout. Do not list the engine plugin as a finished integration until that build and runtime check are completed.
