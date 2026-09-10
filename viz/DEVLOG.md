# viz/ — development mini-timeline

A local, viewer-only journal (separate from the project devlog). Newest first.

## 2026-09-10 — per-federate viewpoints (multi-viewport)

- **Shared playback clock** (`viewport/clock.js`): App advances it once/frame; every
  SceneController reads `playback.t` in its own loop, so all panes stay in sync.
  Removed per-controller clocks / play-seek-setSpeed / onTime.
- **Prop-driven viewports**: SceneController now takes a per-pane `viewFilter` (which
  federates it shows) and applies display state via `applyFederateStates` /
  `applyAircraftModes`; Viewport passes everything as props so a freshly-mounted pane
  gets current state. Recenter is delivered as incrementing nonce props (no refs).
- **Viewpoint control** (World section): Fused | Separated × All | Selected. Separated
  lays out a square-ish grid — `cols=ceil(√N)` (vertical split first), rows fill after;
  any leftover cells hold a Fused view. Federate select checkboxes appear under Selected.
- **Panes**: colored border on hover, synced both ways with the federate panel row
  (`hovered`); title label (color dot + name/Fused) top-left; ordered by federate name.
- Widened panel to 268px.
- **Deferred**: adjustable seams (resizable splits) — grid is equal tracks for now.

## 2026-09-09 (h) — NDJSON meta/role evolution (viewer side)

- Wrote the schema contract: `viz/NDJSON_SCHEMA.md` (meta line: federate/dt/sectors;
  per-aircraft `role` owned|ghost; back-compat rules; reference C++ emitter change).
- Pipeline now speaks it: `loader` takes the federate name from `meta.federate` (falls
  back to filename); `timeline` carries `role`, and `dt`/`sectors` from meta. God/truth
  view built from `role:"owned"` only; ghost rows are understood but deferred to the
  per-federate viewpoint feature. Current sim_out files (no meta/role) load unchanged.
- Panel shows each aircraft's role · federate.
- `tools/gen-sample.mjs`: emits new-schema sample files (meta + role) for testing +
  as the concrete reference for the federate emitter.
- **Deferred (main-library context):** the actual `AircraftFederate.cpp` emitter change
  (meta line + `role`), specced in NDJSON_SCHEMA.md — not applied here.

## 2026-09-09 (g) — label edges, recenter angle, box behind planes

- **Unit labels** now ride the closest edge that is ATTACHED TO A VISIBLE wall (per axis,
  pick the nearest of the corner-edges bordering a drawn plane), so they no longer float
  on a hidden near corner when viewing through a corner. Same visibility rule as grids.
- **Recenter** buttons now also reset to the initial 3/4 viewing angle (preserveDir off).
- **Color box behind planes**: room material is depthWrite:false + renderOrder -10, so
  aircraft wings near a wall draw over it instead of clipping into it.

## 2026-09-09 (f) — corrections: interior walls, near-edge units, recenter

- **Walls**: now show a grid on a face iff we're seeing its INTERIOR (back) face — i.e.
  the same faces as the BackSide room color (outward-normal·toCamera < 0). Replaces the
  "hide one wall" rule; leaves the interior-facing 2–3 walls, matching the color.
- **Units**: (a) placed on the CLOSEST edge to the camera now (was farthest); (b) tick
  values use a PER-AXIS nice step so the thin Z axis actually gets labels (the global
  grid step gave Z none). Grid cells stay square on the global step; labels decoupled.
- **Recenter**: `recenterWorld()` (fits the whole shared worldspace) on the World panel;
  `recenterFederate(name)` (fits that federate's owned aircraft bounds) as a ⊕ chip per
  federate. Both keep the current orbit angle. Identical while one federate owns all.

## 2026-09-09 (e) — transport keys, edge units, enclosing grids, isometric

- **Keyboard transport**: Space = play/pause; ←/→ = ±1 ms; Shift+←/→ = ±1 s;
  Ctrl+←/→ = ±5 s. Guarded so form controls (incl. sliders) keep native key behavior.
- **Axis units**: moved from view-scaled 3D sprites to **CSS2D DOM labels** — constant
  12px, crisp. Parked on the **box edges** (matplotlib-style: X/Y on the back-bottom
  edges, Z up the back vertical edge), flipping as the camera orbits. Z units included.
  Show/hide via a **Units** toggle.
- **Grids on all walls but the "fourth wall"**: six fixed wall grids; each frame the one
  facing the camera is hidden. Grid lines use `depthWrite:false` so near walls don't hide
  aircraft. Compensates for perspective vs the old 3-far-wall look.
- **Isometric toggle** (World section): swaps the perspective camera for an orthographic
  one (parallel projection), preserving orientation; controls rebuilt for the new camera.

## 2026-09-09 (d) — readability pass

- Narrower panel (`--panel-w: 240px`) → more room for the viewport.
- **Dark mode** for the whole screen: CSS variables (light/dark) for the DOM; scene
  palette in `config.js` `THEMES`; `SceneController.setTheme()` rebuilds worldspace +
  gizmo. Bumped grid/tick contrast.
- **Corner axis gizmo** (`gizmo.js`) mirrors the camera orientation, top-right, with the
  X/Y/Z labels — so the in-scene axis letters were removed. Toggle in the World section.
  Rendered via a scissored corner viewport (autoClear off so it doesn't wipe the frame).
- **Square gridlines**: one global step in every dimension (true sim distance), not
  per-axis. Thinner axis rods; small arrowheads; axes inset so the +Z arrow no longer
  collides with the federate bounding box.
- **Vector field** toggle (World section) → shows a "No vector field / wind loaded"
  overlay for now; real arrows (thin, thickness = strength, behind planes) are the TODO.

## 2026-09-09 (c) — control panel + axis/grid system

- **Far-wall grids**: 3 flat grids (`worldspace.js`) that park on the far walls and
  follow the camera as you orbit (matplotlib-style). Major + minor gridlines; kept the
  volumetric room fill. Grids inset slightly to avoid z-fighting the walls.
- **Axes**: arrowheads at the + tips, axis letters, and numeric tick units along X/Y/Z
  (canvas-texture sprites, depth-test off so they stay legible).
- **Federation control** (panel top): per-federate show/hide, highlight (draws a strong
  dashed bounding box — = the whole worldspace for now, since one federate owns all),
  optional halo (outward offset box), and a size slider for all its aircraft.
- **Per-aircraft containers**: collapsible; show/hide/highlight mode; live stats
  (pos/vel/speed/alt) sampled at the current time when expanded.
- Timeline now carries velocity too (for the stats).

## 2026-09-09 (b) — auto-ingest + worldspace + playback

- **Auto-ingest**: Vite dev plugin serves repo-root `sim_out/` at `/sim_out/`
  (`index.json` lists files; each file streamed). Drop `.ndjson` in, reload, it loads.
  Dev-only convenience; the federate still just emits files.
- Confirmed the REAL schema from F1/F2: `{"t",aircraft:[{id,pos,vel,quat}]}` — no
  `meta`/`role` yet (that evolution still pending). Loader tolerates a future meta line.
- **Data layer**: `ndjson.js` (parse, crash-safe), `timeline.js` (merge federate files
  into per-aircraft tracks; lerp pos + nlerp quat; data bounds; `sample(t)`), `loader.js`.
- **Worldspace**: a placeholder cuboid auto-derived from data bounds (folds in origin,
  stops any axis collapsing to a sliver, pads). Room walls via a **BackSide box** —
  only far walls/ceiling render, near ones never occlude (the requested backwall +
  "ceiling from underside" behaviour). Thicker RGB **axis rods** through the origin
  spanning the cuboid. Camera auto-frames the box.
- **Aircraft**: one mesh per id, **colored by federate** (F1 orange, F2 blue), sized in
  world units via a **left-panel size slider** (default 40 m — tiny vs the ~2 km box).
- **Playback**: timeline strip (play/pause, scrub, speed, time readout); clock lives in
  the SceneController; autoplay on load; replay loops.
- Panel now lists federates + aircraft with color swatches.

### Open / to confirm
- "all vs tagged" size control — implemented a single size slider for now; need to know
  what all-vs-tagged should switch between (needs selection/tagging, not built yet).
- Floor gridlines were dropped in favour of the room; can bring them back on the floor.
- Worldspace bounds are a PLACEHOLDER (data-derived); swap for real sim world params
  once the emitter provides them (the meta line).

## 2026-09-09 (a) — skeleton stood up

- Chose the stack: **Vue + Vite + Three.js** (plain Three inside a Vue component;
  skipped TresJS for legibility/control). Vite is dev-server only — the federate
  still just emits NDJSON.
- Decided the world is **Z-up** to match the sim (z = altitude) and Blender; axes
  X-red/Y-green/Z-blue.
- Built: Vite+Vue app; layout shell (left panel ~1/3, viewport ~2/3, timeline strip);
  `Viewport.vue` + `SceneController.js` with isometric camera, middle-drag orbit,
  zoom/pan, grid, axes, lights, and one sample aircraft oriented from a real quaternion.
- Established the stack rule: Three objects stay out of Vue reactivity.

### Next
- Define the NDJSON schema evolution (`meta` first line + `role:"owned"` per aircraft)
  and write a synthetic sample generator so the viewer is demoable without the C++ run.
- NDJSON loader + timeline playback.
- Pin the sim-frame <-> graphics-frame axis/quaternion mapping against real data.

### Open questions / notes
- ADR-0018 says "Canvas 2D" — this went 3D/WebGL. Log the supersede next doc-sync.
- Sim quaternion convention vs Three.js: verify the mapping once real frames load.
