<script setup>
// The shell: loads sim_out, drives the viewport, and renders the control panel.
// Playback time is owned by the SceneController; display + theme state live here and
// are pushed to the scene through the Viewport's exposed methods / props.
import { ref, reactive, shallowRef, onMounted, onBeforeUnmount, computed } from 'vue'
import Viewport from './components/Viewport.vue'
import { loadSimOut } from './data/loader.js'
import { DEFAULT_AIRCRAFT_SIZE, AIRCRAFT_SIZE_RANGE, hexToCss } from './config.js'

const vp = ref(null)
const timeline = shallowRef(null)
const status = ref('Loading sim_out…')
const aircraft = ref([])
const federates = ref([])

// world / view options
const theme = ref('light')
const showGizmo = ref(true)
const showUnits = ref(true)
const isometric = ref(false)
const showVectorField = ref(false)

const playing = ref(false)
const currentT = ref(0)
const duration = ref(0)
const speed = ref(1)
const SPEEDS = [0.25, 0.5, 1, 2, 4]

const fedUi = reactive({}) // name -> {visible, highlighted, halo, size}
const acUi = reactive({}) // id -> {expanded, mode}

// Keyboard transport. Guarded so typing in form controls (incl. the sliders) is
// untouched. Space = play/pause; ←/→ = ±1 ms; Shift+←/→ = ±1 s; Ctrl+←/→ = ±5 s.
function onKey(e) {
  const tag = (e.target && e.target.tagName) || ''
  if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return
  if (e.code === 'Space') {
    e.preventDefault()
    vp.value?.togglePlay()
  } else if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
    e.preventDefault()
    const dir = e.key === 'ArrowRight' ? 1 : -1
    const step = e.ctrlKey ? 5 : e.shiftKey ? 1 : 0.001
    vp.value?.seek(currentT.value + dir * step)
  }
}

onMounted(async () => {
  window.addEventListener('keydown', onKey)
  try {
    const tl = await loadSimOut()
    for (const f of tl.federates) {
      fedUi[f.name] = { visible: true, highlighted: false, halo: false, size: DEFAULT_AIRCRAFT_SIZE }
    }
    for (const a of tl.aircraft) acUi[a.id] = { expanded: false, mode: 'show' }
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

const liveById = computed(() => {
  const m = {}
  const tl = timeline.value
  if (tl) for (const s of tl.sample(currentT.value)) m[s.id] = s
  return m
})

// world options
function toggleTheme() {
  theme.value = theme.value === 'light' ? 'dark' : 'light'
}
function onGizmo(e) {
  showGizmo.value = e.target.checked
  vp.value?.setGizmoVisible(showGizmo.value)
}
function onUnits(e) {
  showUnits.value = e.target.checked
  vp.value?.setUnitsVisible(showUnits.value)
}
function onIso(e) {
  isometric.value = e.target.checked
  vp.value?.setProjection(isometric.value ? 'isometric' : 'perspective')
}

onBeforeUnmount(() => window.removeEventListener('keydown', onKey))

// federate handlers
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

// aircraft handlers
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
  <div class="app" :data-theme="theme">
    <aside class="panel">
      <h1>DFF Viewer</h1>
      <p class="status">{{ status }}</p>

      <!-- World / view options -->
      <section>
        <h2>World</h2>
        <label class="opt">
          <input type="checkbox" :checked="theme === 'dark'" @change="toggleTheme" /> Dark mode
        </label>
        <label class="opt">
          <input type="checkbox" :checked="showGizmo" @change="onGizmo" /> Axis gizmo
        </label>
        <label class="opt">
          <input type="checkbox" :checked="showUnits" @change="onUnits" /> Units
        </label>
        <label class="opt">
          <input type="checkbox" :checked="isometric" @change="onIso" /> Isometric
        </label>
        <label class="opt">
          <input type="checkbox" v-model="showVectorField" /> Vector field
        </label>
        <button class="wbtn" @click="vp?.recenterWorld()" title="fit the whole shared worldspace">
          ⊕ Recenter world
        </button>
      </section>

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
            <button class="chip" title="recenter on this federate's local world"
              @click="vp?.recenterFederate(f.name)">⊕</button>
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
        <Viewport ref="vp" :timeline="timeline" :theme="theme" @time="onTime" />
        <div v-if="showVectorField" class="vf-note">No vector field / wind loaded</div>
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
  /* layout */
  --panel-w: 240px;
  /* light theme (default) */
  --bg: #ffffff;
  --panel-bg: #fbfbfa;
  --card-bg: #ffffff;
  --border: #e6e4dd;
  --line: #d8d6cf;
  --text: #2c2c2a;
  --muted: #8a887f;
  --hover: #f6f5f1;
  --chip-on: #eef1ee;
  --chip-on-border: #9fbf9f;

  display: grid;
  grid-template-columns: var(--panel-w) 1fr;
  height: 100vh;
  font-family: 'Inter', system-ui, -apple-system, 'Segoe UI', sans-serif;
  color: var(--text);
  background: var(--bg);
}
.app[data-theme='dark'] {
  --bg: #1b1b1d;
  --panel-bg: #202023;
  --card-bg: #27272a;
  --border: #38383c;
  --line: #45454a;
  --text: #d6d5d0;
  --muted: #8f8d85;
  --hover: #2c2c30;
  --chip-on: #33402f;
  --chip-on-border: #5c7a4e;
}

.panel { border-right: 1px solid var(--border); padding: 14px 12px; background: var(--panel-bg); overflow-y: auto; }
.panel h1 { font-size: 15px; margin: 0 0 2px; letter-spacing: -0.2px; }
.status { font-size: 11.5px; color: var(--muted); margin: 0 0 6px; }
.panel h2 { font-size: 10.5px; text-transform: uppercase; letter-spacing: 0.6px; color: var(--muted); margin: 16px 0 6px; }
.placeholder { color: var(--muted); font-style: italic; font-size: 12.5px; }

.opt { display: flex; align-items: center; gap: 8px; font-size: 12.5px; padding: 2px 0; cursor: pointer; }
.wbtn { margin-top: 8px; width: 100%; border: 1px solid var(--line); background: var(--card-bg); color: var(--text); border-radius: 6px; padding: 6px 0; font-size: 12px; cursor: pointer; }
.wbtn:hover { background: var(--hover); }

.dot { display: inline-block; width: 10px; height: 10px; border-radius: 50%; flex: none; }

.fed { border: 1px solid var(--border); border-radius: 8px; padding: 8px 9px; margin-bottom: 8px; background: var(--card-bg); }
.fed-top { display: flex; align-items: center; gap: 7px; }
.fed-name { font-size: 12.5px; font-weight: 600; flex: 1; }
.chip { border: 1px solid var(--line); background: var(--card-bg); border-radius: 6px; width: 25px; height: 23px; cursor: pointer; font-size: 12px; line-height: 1; color: var(--muted); padding: 0; }
.chip.on { background: var(--chip-on); border-color: var(--chip-on-border); color: var(--text); }
.fed-size { display: flex; align-items: center; gap: 8px; margin-top: 8px; }
.fed-size .lbl { font-size: 11px; color: var(--muted); white-space: nowrap; }
.fed-size input[type='range'] { flex: 1; accent-color: #6b6a63; }

.ac { border: 1px solid var(--border); border-radius: 8px; margin-bottom: 6px; background: var(--card-bg); overflow: hidden; }
.ac-head { display: flex; align-items: center; gap: 7px; padding: 7px 9px; cursor: pointer; font-size: 12.5px; }
.ac-head:hover { background: var(--hover); }
.caret { color: var(--muted); width: 10px; }
.ac-head .sub { color: var(--muted); font-size: 12px; }
.ac-head .mode { margin-left: auto; font-size: 10px; text-transform: uppercase; letter-spacing: 0.5px; color: var(--muted); }
.ac-head .mode[data-mode='hide'] { color: #c07a4b; }
.ac-head .mode[data-mode='highlight'] { color: #4f9a4f; }
.ac-body { padding: 8px 9px 10px; border-top: 1px solid var(--border); }

.seg { display: flex; gap: 4px; margin-bottom: 8px; }
.seg button { flex: 1; border: 1px solid var(--line); background: var(--card-bg); border-radius: 6px; padding: 4px 0; font-size: 11.5px; cursor: pointer; color: var(--muted); }
.seg button.on { background: var(--chip-on); border-color: var(--chip-on-border); color: var(--text); }

.stats { margin: 0; font-size: 12px; }
.stats > div { display: flex; justify-content: space-between; padding: 1px 0; }
.stats dt { color: var(--muted); margin: 0; }
.stats dd { margin: 0; font-variant-numeric: tabular-nums; }

.stage { display: grid; grid-template-rows: 1fr auto; min-width: 0; min-height: 0; }
.viewport-wrap { position: relative; min-height: 0; }
.vf-note {
  position: absolute; top: 12px; left: 50%; transform: translateX(-50%);
  background: color-mix(in srgb, var(--panel-bg) 88%, transparent);
  border: 1px solid var(--border); color: var(--muted);
  font-size: 12px; padding: 5px 12px; border-radius: 999px; pointer-events: none;
}
.timeline { border-top: 1px solid var(--border); padding: 10px 16px; background: var(--panel-bg); display: flex; align-items: center; gap: 12px; }
.btn { border: 1px solid var(--line); background: var(--card-bg); border-radius: 6px; width: 34px; height: 30px; cursor: pointer; font-size: 12px; color: var(--text); }
.btn:hover { background: var(--hover); }
.scrub { flex: 1; accent-color: #6b6a63; }
.time { font-variant-numeric: tabular-nums; font-size: 12.5px; color: var(--muted); min-width: 92px; text-align: right; }
.speed { border: 1px solid var(--line); border-radius: 6px; padding: 4px 6px; background: var(--card-bg); color: var(--text); font-size: 12.5px; }
</style>
