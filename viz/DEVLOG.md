# viz/ — development mini-timeline

A local, viewer-only journal (separate from the project devlog). Newest first.

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
