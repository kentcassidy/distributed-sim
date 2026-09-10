<script setup>
// The shell: loads sim_out, drives the viewport, and renders the control panel.
// Playback time is owned by the SceneController; this mirrors it for the slider.
// Display state (per-federate, per-aircraft) lives here and is pushed to the scene
// through the Viewport's exposed methods.
import { ref, reactive, shallowRef, onMounted, computed } from 'vue'
import Viewport from './components/Viewport.vue'
import { loadSimOut } from './data/loader.js'
import { DEFAULT_AIRCRAFT_SIZE, AIRCRAFT_SIZE_RANGE, hexToCss } from './config.js'

const vp = ref(null)
const timeline = shallowRef(null)
const status = ref('Loading sim_out…')
const aircraft = ref([])
const federates = ref([])

const playing = ref(false)
const currentT = ref(0)
const duration = ref(0)
const speed = ref(1)
const SPEEDS = [0.25, 0.5, 1, 2, 4]

// per-federate UI state: name -> {visible, highlighted, halo, size}
const fedUi = reactive({})
// per-aircraft UI state: id -> {expanded, mode}
const acUi = reactive({})

onMounted(async () => {
  try {
    const tl = await loadSimOut()
    for (const f of tl.federates) {
      fedUi[f.name] = { visible: true, highlighted: false, halo: false, size: DEFAULT_AIRCRAFT_SIZE }
    }
    for (const a of tl.aircraft) {
      acUi[a.id] = { expanded: false, mode: 'show' }
    }
    timeline.value = tl
    duration.value = tl.duration
    aircraft.value = tl.aircraft.map((a) => ({ ...a, css: hexToCss(a.color) }))
    federates.value = tl.federates.map((f) => ({ ...f, css: hexToCss(f.color) }))
    status.value = `${aircraft.value.length} aircraft · ${federates.value.length} federate file(s) · ${duration.value.toFixed(1)}s`
    vp.value?.play()
  } catch (e) {
    status.value = 'Could not load sim_out/: ' + e.message
    console.error(e)
  }
})

function onTime({ t, playing: p, duration: d }) {
  currentT.value = t
  playing.value = p
  if (d) duration.value = d
}
const timeLabel = computed(() => `${currentT.value.toFixed(1)} / ${duration.value.toFixed(1)} s`)

// live per-aircraft state at the current time (recomputed as playback advances)
const liveById = computed(() => {
  const m = {}
  const tl = timeline.value
  if (tl) for (const s of tl.sample(currentT.value)) m[s.id] = s
  return m
})

// --- federate handlers ---
function toggleFedVisible(name) {
  fedUi[name].visible = !fedUi[name].visible
  vp.value?.setFederateVisible(name, fedUi[name].visible)
}
function toggleFedHighlight(name) {
  fedUi[name].highlighted = !fedUi[name].highlighted
  vp.value?.setFederateHighlight(name, fedUi[name].highlighted)
}
function toggleFedHalo(name) {
  fedUi[name].halo = !fedUi[name].halo
  vp.value?.setFederateHalo(name, fedUi[name].halo)
}
function onFedSize(name, e) {
  fedUi[name].size = Number(e.target.value)
  vp.value?.setFederateSize(name, fedUi[name].size)
}

// --- aircraft handlers ---
function toggleExpand(id) {
  acUi[id].expanded = !acUi[id].expanded
}
function setMode(id, mode) {
  acUi[id].mode = mode
  vp.value?.setAircraftMode(id, mode)
}

const f1 = (n) => (n === undefined ? '—' : n.toFixed(1))
const speedOf = (v) => (v ? Math.hypot(v[0], v[1], v[2]) : 0)
</script>

<template>
  <div class="app">
    <aside class="panel">
      <h1>DFF Viewer</h1>
      <p class="status">{{ status }}</p>

      <!-- Federation control -->
      <section>
        <h2>Federation</h2>
        <div v-for="f in federates" :key="f.name" class="fed">
          <div class="fed-top">
            <span class="dot" :style="{ background: f.css }"></span>
            <span class="fed-name">{{ f.name }}</span>
            <button class="chip" :class="{ on: fedUi[f.name].visible }" title="show / hide"
              @click="toggleFedVisible(f.name)">{{ fedUi[f.name].visible ? '👁' : '🚫' }}</button>
            <button class="chip" :class="{ on: fedUi[f.name].highlighted }" title="highlight bounds"
              @click="toggleFedHighlight(f.name)">▢</button>
            <button class="chip" :class="{ on: fedUi[f.name].halo }" title="halo"
              @click="toggleFedHalo(f.name)">◌</button>
          </div>
          <div class="fed-size">
            <span class="lbl">size</span>
            <input type="range" :min="AIRCRAFT_SIZE_RANGE.min" :max="AIRCRAFT_SIZE_RANGE.max" step="1"
              :value="fedUi[f.name].size" @input="onFedSize(f.name, $event)" />
            <span class="lbl">{{ fedUi[f.name].size }}m</span>
          </div>
        </div>
        <p v-if="!federates.length" class="placeholder">— none —</p>
      </section>

      <!-- Per-aircraft containers -->
      <section>
        <h2>Aircraft</h2>
        <div v-for="a in aircraft" :key="a.id" class="ac">
          <div class="ac-head" @click="toggleExpand(a.id)">
            <span class="caret">{{ acUi[a.id].expanded ? '▾' : '▸' }}</span>
            <span class="dot" :style="{ background: a.css }"></span>
            <span>id {{ a.id }}</span>
            <span class="sub">· {{ a.federate }}</span>
            <span class="mode" :data-mode="acUi[a.id].mode">{{ acUi[a.id].mode }}</span>
          </div>
          <div v-if="acUi[a.id].expanded" class="ac-body">
            <div class="seg">
              <button :class="{ on: acUi[a.id].mode === 'show' }" @click="setMode(a.id, 'show')">Show</button>
              <button :class="{ on: acUi[a.id].mode === 'hide' }" @click="setMode(a.id, 'hide')">Hide</button>
              <button :class="{ on: acUi[a.id].mode === 'highlight' }" @click="setMode(a.id, 'highlight')">Highlight</button>
            </div>
            <dl class="stats">
              <div><dt>pos</dt><dd>{{ f1(liveById[a.id]?.pos[0]) }}, {{ f1(liveById[a.id]?.pos[1]) }}, {{ f1(liveById[a.id]?.pos[2]) }}</dd></div>
              <div><dt>vel</dt><dd>{{ f1(liveById[a.id]?.vel[0]) }}, {{ f1(liveById[a.id]?.vel[1]) }}, {{ f1(liveById[a.id]?.vel[2]) }}</dd></div>
              <div><dt>speed</dt><dd>{{ f1(speedOf(liveById[a.id]?.vel)) }} m/s</dd></div>
              <div><dt>alt (z)</dt><dd>{{ f1(liveById[a.id]?.pos[2]) }} m</dd></div>
            </dl>
          </div>
        </div>
        <p v-if="!aircraft.length" class="placeholder">— none —</p>
      </section>
    </aside>

    <main class="stage">
      <div class="viewport-wrap">
        <Viewport ref="vp" :timeline="timeline" @time="onTime" />
      </div>
      <div class="timeline">
        <button class="btn" @click="vp?.togglePlay()">{{ playing ? '❚❚' : '▶' }}</button>
        <input class="scrub" type="range" min="0" :max="duration || 0.0001" step="0.01"
          :value="currentT" @input="vp?.seek(Number($event.target.value))" />
        <span class="time">{{ timeLabel }}</span>
        <select class="speed" :value="speed" @change="speed = Number($event.target.value); vp?.setSpeed(speed)">
          <option v-for="s in SPEEDS" :key="s" :value="s">{{ s }}×</option>
        </select>
      </div>
    </main>
  </div>
</template>

<style scoped>
.app {
  display: grid;
  grid-template-columns: minmax(260px, 1fr) 2fr;
  height: 100vh;
  font-family: 'Inter', system-ui, -apple-system, 'Segoe UI', sans-serif;
  color: #2c2c2a;
  background: #ffffff;
}

.panel { border-right: 1px solid #e6e4dd; padding: 16px 14px; background: #fbfbfa; overflow-y: auto; }
.panel h1 { font-size: 16px; margin: 0 0 2px; letter-spacing: -0.2px; }
.panel .status { font-size: 12px; color: #5f5e5a; margin: 0 0 8px; }
.panel h2 { font-size: 11px; text-transform: uppercase; letter-spacing: 0.6px; color: #8a887f; margin: 18px 0 6px; }
.placeholder { color: #b6b4ac; font-style: italic; font-size: 12.5px; }

.dot { display: inline-block; width: 10px; height: 10px; border-radius: 50%; flex: none; }

/* federate rows */
.fed { border: 1px solid #eae8e1; border-radius: 8px; padding: 8px 10px; margin-bottom: 8px; background: #fff; }
.fed-top { display: flex; align-items: center; gap: 8px; }
.fed-name { font-size: 13px; font-weight: 600; flex: 1; }
.chip {
  border: 1px solid #d8d6cf; background: #fff; border-radius: 6px; width: 26px; height: 24px;
  cursor: pointer; font-size: 12px; line-height: 1; color: #5f5e5a; padding: 0;
}
.chip.on { background: #eef1ee; border-color: #9fbf9f; color: #2c2c2a; }
.fed-size { display: flex; align-items: center; gap: 8px; margin-top: 8px; }
.fed-size .lbl { font-size: 11px; color: #8a887f; white-space: nowrap; }
.fed-size input[type='range'] { flex: 1; accent-color: #6b6a63; }

/* aircraft containers */
.ac { border: 1px solid #eae8e1; border-radius: 8px; margin-bottom: 6px; background: #fff; overflow: hidden; }
.ac-head { display: flex; align-items: center; gap: 8px; padding: 7px 10px; cursor: pointer; font-size: 13px; }
.ac-head:hover { background: #f6f5f1; }
.caret { color: #8a887f; width: 10px; }
.ac-head .sub { color: #8a887f; font-size: 12px; }
.ac-head .mode { margin-left: auto; font-size: 10px; text-transform: uppercase; letter-spacing: 0.5px; color: #8a887f; }
.ac-head .mode[data-mode='hide'] { color: #b06a4b; }
.ac-head .mode[data-mode='highlight'] { color: #2f7d32; }
.ac-body { padding: 8px 10px 10px; border-top: 1px solid #eee; }

.seg { display: flex; gap: 4px; margin-bottom: 8px; }
.seg button {
  flex: 1; border: 1px solid #d8d6cf; background: #fff; border-radius: 6px; padding: 4px 0;
  font-size: 11.5px; cursor: pointer; color: #5f5e5a;
}
.seg button.on { background: #eef1ee; border-color: #9fbf9f; color: #2c2c2a; }

.stats { margin: 0; font-size: 12px; }
.stats > div { display: flex; justify-content: space-between; padding: 1px 0; }
.stats dt { color: #8a887f; margin: 0; }
.stats dd { margin: 0; font-variant-numeric: tabular-nums; }

/* timeline */
.stage { display: grid; grid-template-rows: 1fr auto; min-width: 0; min-height: 0; }
.viewport-wrap { position: relative; min-height: 0; }
.timeline { border-top: 1px solid #e6e4dd; padding: 10px 16px; background: #fbfbfa; display: flex; align-items: center; gap: 12px; }
.btn { border: 1px solid #d8d6cf; background: #fff; border-radius: 6px; width: 34px; height: 30px; cursor: pointer; font-size: 12px; color: #2c2c2a; }
.btn:hover { background: #f1f0eb; }
.scrub { flex: 1; accent-color: #6b6a63; }
.time { font-variant-numeric: tabular-nums; font-size: 12.5px; color: #5f5e5a; min-width: 92px; text-align: right; }
.speed { border: 1px solid #d8d6cf; border-radius: 6px; padding: 4px 6px; background: #fff; font-size: 12.5px; }
</style>
