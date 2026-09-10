# viz/ — development mini-timeline

A local, viewer-only journal (separate from the project devlog). Newest first.

## 2026-09-09 — skeleton stood up

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
