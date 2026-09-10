# ADR-0018: NDJSON Logging Seam and Browser Viewer

- **Status:** Accepted
- **Date:** 2026-09-09
- **Deciders:** Kent

## Context

The simulation needs a logging/interchange format for two purposes: recording truth
for the partition-invariance comparison, and feeding a visualization of the worldspace
(aircraft moving, the controller's sectors, handoffs). ADR-0003 fixed the "C++
computes, scripting presents" split and named CSV as the interchange format in
passing. But the data here is not flat: quaternion orientation, a time-varying **list**
of sector AABBs, and discrete **events** (handoff, sector split/merge) all flatten
badly into CSV columns. A richer, still-simple, still-streamable format is needed —
and the visualization must be chosen without pulling web/DB machinery into the
RTI-facing federate.

## Decision

The logging/interchange seam is **NDJSON** (newline-delimited JSON: one JSON object
per line) — a per-timestep frame (`{t, aircraft[], sectors[]}`) plus event lines. The
**C++ side only emits** NDJSON (append to a file and/or stdout); it runs no web server
and no database. Visualization is a **browser** page rendering **Canvas 2D** top-down
(dashed rectangles for sectors, glyphs for aircraft): **replay** first (the page reads
the NDJSON file), with **realtime** as an optional later flip via a small
Server-Sent-Events sidecar that tails the live NDJSON. This **refines** the
CSV-interchange consequence of ADR-0003, which otherwise stands.

## Alternatives Considered

- **CSV (the ADR-0003 default).** *Buys:* trivial, matplotlib-native. *Costs:*
  flattens quaternions / sector-lists / events badly. Rejected for this data shape.
- **SQLite / a relational DB.** *Buys:* queryability; scales to N aircraft and
  post-hoc analysis. *Costs:* a C++ dependency in the federate, and unnatural for a
  realtime push. Deferred: if queryable history is wanted, load NDJSON → SQLite
  offline, with no DB code in the federate.
- **OpenGL / C++ viewer over X11.** *Buys:* native 3D, plane meshes. *Costs:* window/
  context/shader/camera work and X11-over-WSL2 friction, for a partitioning picture a
  2D top-down view actually shows more clearly. Deferred to post-MVP polish.
- **NDJSON + browser (chosen).** *Buys:* fits the structured/event data; append-only,
  streamable, crash-safe; keeps all web/DB complexity out of the C++ federate; and lets
  one file serve both replay and realtime. *Costs:* two small presentation pieces (an
  HTML page; later an SSE relay) — accepted, and both in scripting per ADR-0003.

## Consequences

- The federate/logger emit NDJSON; nothing on the C++ side gains a web or DB
  dependency, and the charter's "do not build a transport" rule is respected.
- One data file drives both replay and (later) realtime; the viewer is off the
  critical path and cannot break the federation.
- The controller writes its sector geometry + assignment events into the same stream,
  which is what makes segmentation/merging visible.
- SQLite and a richer 3D viewer remain clean, additive upgrades.

## Update (2026-09-10) — viewer went 3D (WebGL), not Canvas 2D

The **NDJSON seam and the "C++ only emits" rule stand unchanged.** Only the viewer
*technology* is revised: the entity state is genuinely 3D (a `Vec3` position and a
quaternion), so the "Canvas 2D top-down" decision above was underscoped. The viewer
built in `viz/` is **Vue + Vite + Three.js** (WebGL, Z-up, RGB axes), replay-first,
with realtime via SSE still the later flip. This is the "richer 3D viewer" the
alternatives list had deferred — pulled forward because it is both more faithful to the
data and more compelling for the presentation, and it remains off the critical path
(dev-server-only; no web/DB in the federate). The NDJSON schema also grew a `meta` line
and a per-aircraft `role` (see `viz/NDJSON_SCHEMA.md`); the C++ emitter change to
produce them is pending, to be done in the main library.
