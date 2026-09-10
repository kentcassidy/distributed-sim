# Kanban breakdown — 3D Worldspace Viewer (`viz/`)

Discrete cards for the GitHub Projects board, sequenced to show the arc from
**architecting → implementing → hardening**. Copy each as an issue/card. Tags:
`[ARCH]` design decision, `[IMPL]` build, `[POLISH]` refinement, `[BACKLOG]` deferred.
Suggested columns: **Done** (1–11), **Next** (12), **Backlog** (13–16).

> Epic: **Distributed Flight-Dynamics Federation — worldspace viewer.** A browser 3D
> replay of the federation's NDJSON truth, built to make the distributed simulation
> legible for the presentation. Off the critical path; the C++ federate only emits.

## Done

1. **[ARCH] Choose the visualization stack & data seam.**
   NDJSON as the C++↔viewer seam (federate only emits); Vue + Vite + Three.js; Z-up
   world / RGB axes. *Done when:* decision recorded (ADR-0018 update) + empty scene renders.

2. **[IMPL] Scaffold the Vite + Vue + Three.js app.**
   `viz/` project; a `Viewport` component with an isometric camera, middle-drag orbit,
   zoom/pan, ground grid, axes; Three kept out of Vue reactivity. *Done when:* a sample
   aircraft renders, oriented from a real quaternion.

3. **[IMPL] NDJSON auto-ingest pipeline.**
   Dev endpoint serving `sim_out/`; parse + merge per-federate files into one timeline
   (position lerp, quaternion nlerp, world bounds). *Done when:* real F1/F2 files load
   and animate.

4. **[IMPL] Timeline playback.**
   Shared clock synced across viewports; play/pause, scrub, speed; keyboard transport
   (Space / arrows / Shift / Ctrl). *Done when:* aircraft fly and controls drive them.

5. **[IMPL] Worldspace furniture.**
   Volumetric room (interior-facing walls); camera-following wall grids (major/minor,
   square cells); arrowed RGB axes; constant-size edge tick units (incl. Z); corner
   orientation gizmo. *Done when:* reads as a clean 3D plot in light & dark.

6. **[IMPL] Control panel — federation & aircraft.**
   Per-federate show/hide/highlight(bounds)/halo/size; per-aircraft show/hide/highlight
   with live stats. *Done when:* toggles affect the scene live.

7. **[POLISH] Readability pass.**
   Dark mode (whole screen + viewport); higher-contrast grids/units; isometric
   (orthographic) toggle; recenter (world + per-federate). *Done when:* legible on a
   projector.

8. **[POLISH] Worldspace correctness fixes.**
   Interior-only walls (match room color); units on the nearest visible-wall edge;
   color box behind aircraft (no wing clipping); axis arrowheads clear of the bounds box.
   *Done when:* no clipping/floating artifacts while orbiting.

9. **[ARCH+IMPL] NDJSON schema evolution — `meta` + `role`.**
   Schema contract (`viz/NDJSON_SCHEMA.md`); loader/timeline parse `meta.federate`, `dt`,
   `sectors`, per-aircraft `role`, with back-compat; sample generator. *Done when:*
   new-schema files load and current files still work.

10. **[ARCH+IMPL] Per-federate viewpoints (multi-viewport).**
    Fused | Separated × All | Selected; square-ish grid layout; federate-colored pane
    borders with hover-sync; pane titles. *Done when:* Separated·All shows one synced
    viewport per federate.

11. **[IMPL] Two-panel UI + aircraft sort/filter.**
    Collapsible Controls | Aircraft panels; sort/filter by federation with sticky group
    headers; "only active federations" sync. *Done when:* the aircraft list scales/scrolls.

## Next

12. **[ARCH+IMPL] C++ emitter: emit `meta` + `role`.** *(main library)*
    Patch `AircraftFederate.cpp` per `viz/NDJSON_SCHEMA.md` (meta line in `initWorld`,
    `role:"owned"` in `step()`). Compile in the container. *Done when:* regenerated
    `sim_out/` files carry meta + role and load unchanged.

## Backlog

13. **[BACKLOG] Adjustable viewport seams.** Draggable gutters to resize the grid tracks.

14. **[BACKLOG] Ghost rendering / Live viewpoints.** Render `role:"ghost"` distinctly
    (translucent, lagged) once the Live experiment produces dead-reckoned ghosts.

15. **[BACKLOG] Sector rendering.** Draw `meta.sectors` as the real federate/sector
    boxes (replacing the current worldspace-as-federate-box placeholder).

16. **[BACKLOG] Vector-field (wind/storm) overlay.** Thin arrows at grid points,
    thickness = strength, behind the planes. Toggle exists; shows a placeholder now.
