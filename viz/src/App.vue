<script setup>
// The layout shell. Right ~2/3 is the 3D viewport with a timeline strip beneath it;
// left ~1/3 is the panel (federate/viewpoint switching + aircraft list). Only the
// viewport is functional in this skeleton; the rest are honest placeholders so the
// intended structure is visible without pretending the features exist yet.
import Viewport from './components/Viewport.vue'
</script>

<template>
  <div class="app">
    <aside class="panel">
      <h1>DFF Viewer</h1>
      <p class="muted">Distributed Flight-Dynamics Federation</p>

      <section>
        <h2>Viewpoint</h2>
        <p class="placeholder">— per-federate views (single / split / all) coming next —</p>
      </section>

      <section>
        <h2>Aircraft</h2>
        <p class="placeholder">— list + hover sync coming next —</p>
      </section>
    </aside>

    <main class="stage">
      <div class="viewport-wrap">
        <Viewport />
      </div>
      <div class="timeline">
        <span class="placeholder">&#9654;&ensp;&mdash; timeline / playback coming next &mdash;</span>
      </div>
    </main>
  </div>
</template>

<style scoped>
.app {
  display: grid;
  /* left panel ~1/3, viewport ~2/3 */
  grid-template-columns: minmax(240px, 1fr) 2fr;
  height: 100vh;
  font-family: 'Inter', system-ui, -apple-system, 'Segoe UI', sans-serif;
  color: #2c2c2a;
  background: #ffffff;
}

.panel {
  border-right: 1px solid #e6e4dd;
  padding: 18px 16px;
  background: #fbfbfa;
  overflow-y: auto;
}
.panel h1 { font-size: 16px; margin: 0; letter-spacing: -0.2px; }
.panel .muted { color: #8a887f; font-size: 12px; margin: 2px 0 18px; }
.panel h2 {
  font-size: 11px; text-transform: uppercase; letter-spacing: 0.6px;
  color: #8a887f; margin: 18px 0 6px;
}

.placeholder { color: #b6b4ac; font-size: 12.5px; font-style: italic; }

.stage {
  display: grid;
  grid-template-rows: 1fr auto;   /* viewport fills, timeline hugs the bottom */
  min-width: 0; min-height: 0;    /* let the grid child actually shrink */
}
.viewport-wrap { position: relative; min-height: 0; }
.timeline {
  border-top: 1px solid #e6e4dd;
  padding: 12px 16px;
  background: #fbfbfa;
}
</style>
