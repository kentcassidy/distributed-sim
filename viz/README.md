# DFF Viewer (`viz/`)

A 3D worldspace viewer for the Distributed Flight-Dynamics Federation.
**Vue + Vite + Three.js.** It *reads* the NDJSON the C++ federates emit; it never
talks back to them (ADR-0018: the federate only emits). This subproject is walled
off from the C++/CMake build — it has its own `node_modules` and lifecycle.

## Run it

```sh
cd viz
npm install
npm run dev
```

Open the URL Vite prints (default http://localhost:5173).

## What works right now (skeleton)

- A single 3D viewport: **Z-up** world (sim `z` = altitude), Blender-style
  **X-red / Y-green / Z-blue** axes, a quiet ground grid, Desmos-clean background.
- **Middle-drag orbits** about the origin; wheel zooms; left/right-drag pans.
- One **sample paper-airplane** placed in the world and oriented from a real
  `(x, y, z, w)` quaternion — proof the render pipeline is honest.
- The layout shell: left panel (~1/3) + viewport (~2/3) + a timeline strip.

## Not built yet (clean seams are in place)

- NDJSON loading + timeline playback (scrub / play / pause / speed).
- The left panel: aircraft list, hover-sync with the viewport, per-aircraft tags.
- Multiple viewports = per-federate viewpoints (single / splitscreen / all-at-once).
- Sectors drawn as dotted boxes, hover reveals the owner.
- Ghost rendering (owned vs. dead-reckoned) for the later Live experiment.

## Design intent

Simple and clean like Desmos; colors like Blender. The viewport is a reusable
component (`Viewport.vue` + `viewport/SceneController.js`) so it can be instantiated
N times, one per federate's point of view — that multi-viewport comparison is the
visual thesis of the whole project.

**Stack rule:** Three.js objects stay OUT of Vue's reactivity. Vue owns the DOM and
app state; Three owns the `<canvas>` and its render loop; data flows in via props.
