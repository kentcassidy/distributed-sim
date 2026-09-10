<script setup>
// Shell: loads sim_out, owns display + viewpoint state, and lays out one or more
// viewports. Playback is a single shared clock (viewport/clock.js). Display state flows
// to panes as PROPS. The left region is two collapsible panels: Controls | Aircraft.
import { ref, reactive, shallowRef, onMounted, onBeforeUnmount, computed } from 'vue'
import Viewport from './components/Viewport.vue'
import { loadSimOut } from './data/loader.js'
import { playback, advance } from './viewport/clock.js'
import { DEFAULT_AIRCRAFT_SIZE, AIRCRAFT_SIZE_RANGE, hexToCss } from './config.js'

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
const projection = computed(() => (isometric.value ? 'isometric' : 'perspective'))

// viewpoint
const layout = ref('fused')
const scope = ref('all')
const selected = reactive({})
const hovered = ref(null)

// panels + aircraft list controls
const worldCollapsed = ref(false)
const fleetCollapsed = ref(false)
const sortBy = ref('federation') // 'federation' | 'id'
const filterFed = ref('all')
const syncActive = ref(false) // only list aircraft whose federate is shown

// recenter (nonce props)
const recenterWorldNonce = ref(0)
const recenterFed = ref({ name: null, n: 0 })

// playback
const playing = ref(false)
const currentT = ref(0)
const duration = ref(0)
const speed = ref(1)
const SPEEDS = [0.25, 0.5, 1, 2, 4]

const fedUi = reactive({}) // name -> {visible, highlight, halo, size}
const acUi = reactive({}) // id -> {expanded, mode}

const gridCols = computed(
  () => `${worldCollapsed.value ? '26px' : '238px'} ${fleetCollapsed.value ? '26px' : '256px'} 1fr`,
)

const acModes = computed(() => {
  const m = {}
  for (const a of aircraft.value) m[a.id] = acUi[a.id]?.mode || 'show'
  return m
})

// Aircraft list: filter by federation + (optionally) active-only, then group/sort.
const fleetGroups = computed(() => {
  let list = aircraft.value.slice()
  if (filterFed.value !== 'all') list = list.filter((a) => a.federate === filterFed.value)
  if (syncActive.value) list = list.filter((a) => fedUi[a.federate]?.visible !== false)
  if (sortBy.value === 'id') {
    list.sort((a, b) => a.id - b.id)
    return [{ federate: null, items: list }]
  }
  list.sort((a, b) => a.federate.localeCompare(b.federate) || a.id - b.id)
  const byFed = new Map()
  for (const a of list) {
    if (!byFed.has(a.federate)) byFed.set(a.federate, [])
    byFed.get(a.federate).push(a)
  }
  return [...byFed.entries()].map(([federate, items]) => ({ federate, items }))
})
const fedCss = (name) => federates.value.find((f) => f.name === name)?.css

// panes (fused vs separated square-ish grid; leftover cells -> fused)
const panes = computed(() => {
  const all = [...federates.value].sort((a, b) => a.name.localeCompare(b.name))
  const chosen = scope.value === 'all' ? all : all.filter((f) => selected[f.name])
  const fusedFilter = scope.value === 'all' || chosen.length === 0 ? null : chosen.map((f) => f.name)
  if (layout.value === 'fused' || chosen.length <= 1) {
    const single = layout.value === 'separated' && chosen.length === 1 ? chosen[0] : null
    return {
      cols: 1,
      rows: 1,
      cells: [
        single
          ? { key: 'f:' + single.name, filter: [single.name], label: single.name, css: single.css, federate: single.name, span: 1 }
          : { key: 'fused', filter: fusedFilter, label: 'Fused', css: null, federate: null, span: 1 },
      ],
    }
  }
  const N = chosen.length
  const cols = Math.ceil(Math.sqrt(N))
  const rows = Math.ceil(N / cols)
  const cells = chosen.map((f) => ({ key: 'f:' + f.name, filter: [f.name], label: f.name, css: f.css, federate: f.name, span: 1 }))
  const leftover = cols * rows - N
  if (leftover > 0) cells.push({ key: 'fused-fill', filter: fusedFilter, label: 'Fused', css: null, federate: null, span: leftover })
  return { cols, rows, cells }
})

// --- load + clock loop ---
let rafId = 0
function frame(now) {
  advance(now)
  currentT.value = playback.t
  rafId = requestAnimationFrame(frame)
}
function onKey(e) {
  const tag = (e.target && e.target.tagName) || ''
  if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return
  if (e.code === 'Space') {
    e.preventDefault()
    togglePlay()
  } else if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
    e.preventDefault()
    const dir = e.key === 'ArrowRight' ? 1 : -1
    const step = e.ctrlKey ? 5 : e.shiftKey ? 1 : 0.001
    seek(currentT.value + dir * step)
  }
}
onMounted(async () => {
  window.addEventListener('keydown', onKey)
  rafId = requestAnimationFrame(frame)
  try {
    const tl = await loadSimOut()
    for (const f of tl.federates) {
      fedUi[f.name] = { visible: true, highlight: false, halo: false, size: DEFAULT_AIRCRAFT_SIZE }
      selected[f.name] = true
    }
    for (const a of tl.aircraft) acUi[a.id] = { expanded: false, mode: 'show' }
    timeline.value = tl
    aircraft.value = tl.aircraft.map((a) => ({ ...a, css: hexToCss(a.color) }))
    federates.value = tl.federates.map((f) => ({ ...f, css: hexToCss(f.color) }))
    playback.duration = tl.duration
    playback.t = 0
    playback.speed = 1
    playback.playing = true
    duration.value = tl.duration
    playing.value = true
    status.value = `${aircraft.value.length} aircraft · ${federates.value.length} federate(s) · ${tl.duration.toFixed(1)}s`
  } catch (e) {
    status.value = 'Could not load sim_out/: ' + e.message
    console.error(e)
  }
})
onBeforeUnmount(() => {
  window.removeEventListener('keydown', onKey)
  cancelAnimationFrame(rafId)
})

// playback
function togglePlay() {
  playback.playing = !playback.playing
  playing.value = playback.playing
}
function seek(t) {
  playback.t = Math.max(0, Math.min(t, playback.duration || 0))
  currentT.value = playback.t
}
function setSpeed(s) {
  playback.speed = s
  speed.value = s
}
const timeLabel = computed(() => `${currentT.value.toFixed(1)} / ${duration.value.toFixed(1)} s`)

// world options
function toggleTheme() {
  theme.value = theme.value === 'light' ? 'dark' : 'light'
}
function recenterWorld() {
  recenterWorldNonce.value++
}
function recenterFederate(name) {
  recenterFed.value = { name, n: recenterFed.value.n + 1 }
}

// federate + aircraft handlers
function toggleFedVisible(name) {
  fedUi[name].visible = !fedUi[name].visible
}
function toggleFedHighlight(name) {
  fedUi[name].highlight = !fedUi[name].highlight
}
function toggleFedHalo(name) {
  fedUi[name].halo = !fedUi[name].halo
}
function onFedSize(name, e) {
  fedUi[name].size = Number(e.target.value)
}
function toggleExpand(id) {
  acUi[id].expanded = !acUi[id].expanded
}
function setMode(id, mode) {
  acUi[id].mode = mode
}

// live stats
const liveById = computed(() => {
  const m = {}
  const tl = timeline.value
  if (tl) for (const s of tl.sample(currentT.value)) m[s.id] = s
  return m
})
const f1 = (n) => (n === undefined ? '—' : n.toFixed(1))
const speedOf = (v) => (v ? Math.hypot(v[0], v[1], v[2]) : 0)
</script>

<template>
  <div class="app" :data-theme="theme" :style="{ gridTemplateColumns: gridCols }">
    <!-- Panel 1: controls -->
    <aside class="panel" :class="{ collapsed: worldCollapsed }">
      <button class="tab" @click="worldCollapsed = !worldCollapsed" :title="worldCollapsed ? 'expand' : 'collapse'">{{ worldCollapsed ? '›' : '‹' }}</button>
      <div v-if="!worldCollapsed" class="panel-body">
        <h1>DFF Viewer</h1>
        <p class="status">{{ status }}</p>

        <section>
          <h2>World</h2>
          <div class="seg small">
            <button :class="{ on: layout === 'fused' }" @click="layout = 'fused'">Fused</button>
            <button :class="{ on: layout === 'separated' }" @click="layout = 'separated'">Separated</button>
          </div>
          <div class="seg small">
            <button :class="{ on: scope === 'all' }" @click="scope = 'all'">All</button>
            <button :class="{ on: scope === 'selected' }" @click="scope = 'selected'">Selected</button>
          </div>
          <label class="opt"><input type="checkbox" :checked="theme === 'dark'" @change="toggleTheme" /> Dark mode</label>
          <label class="opt"><input type="checkbox" :checked="showGizmo" @change="showGizmo = $event.target.checked" /> Axis gizmo</label>
          <label class="opt"><input type="checkbox" :checked="showUnits" @change="showUnits = $event.target.checked" /> Units</label>
          <label class="opt"><input type="checkbox" :checked="isometric" @change="isometric = $event.target.checked" /> Isometric</label>
          <label class="opt"><input type="checkbox" v-model="showVectorField" /> Vector field</label>
          <button class="wbtn" @click="recenterWorld">⊕ Recenter world</button>
        </section>

        <section>
          <h2>Federation</h2>
          <div v-for="f in federates" :key="f.name" class="fed" :class="{ hov: hovered === f.name }"
            @mouseenter="hovered = f.name" @mouseleave="hovered = null">
            <div class="fed-top">
              <input v-if="scope === 'selected'" type="checkbox" class="sel" :checked="selected[f.name]" @change="selected[f.name] = $event.target.checked" title="include in viewpoint" />
              <span class="dot" :style="{ background: f.css }"></span>
              <span class="fed-name">{{ f.name }}</span>
              <button class="chip" :class="{ on: fedUi[f.name].visible }" title="show / hide" @click="toggleFedVisible(f.name)">{{ fedUi[f.name].visible ? '👁' : '🚫' }}</button>
              <button class="chip" :class="{ on: fedUi[f.name].highlight }" title="highlight bounds" @click="toggleFedHighlight(f.name)">▢</button>
              <button class="chip" :class="{ on: fedUi[f.name].halo }" title="halo" @click="toggleFedHalo(f.name)">◌</button>
              <button class="chip" title="recenter on this federate" @click="recenterFederate(f.name)">⊕</button>
            </div>
            <div class="fed-size">
              <span class="lbl">size</span>
              <input type="range" :min="AIRCRAFT_SIZE_RANGE.min" :max="AIRCRAFT_SIZE_RANGE.max" step="1" :value="fedUi[f.name].size" @input="onFedSize(f.name, $event)" />
              <span class="lbl">{{ fedUi[f.name].size }}m</span>
            </div>
          </div>
          <p v-if="!federates.length" class="placeholder">— none —</p>
        </section>
      </div>
      <div v-else class="strip">Controls</div>
    </aside>

    <!-- Panel 2: aircraft -->
    <aside class="panel" :class="{ collapsed: fleetCollapsed }">
      <button class="tab" @click="fleetCollapsed = !fleetCollapsed" :title="fleetCollapsed ? 'expand' : 'collapse'">{{ fleetCollapsed ? '›' : '‹' }}</button>
      <div v-if="!fleetCollapsed" class="panel-body">
        <h2>Aircraft <span class="count">{{ aircraft.length }}</span></h2>
        <div class="fleet-controls">
          <label>Sort
            <select v-model="sortBy"><option value="federation">Federation</option><option value="id">ID</option></select>
          </label>
          <label>Show
            <select v-model="filterFed">
              <option value="all">All federations</option>
              <option v-for="f in federates" :key="f.name" :value="f.name">{{ f.name }}</option>
            </select>
          </label>
          <label class="opt"><input type="checkbox" v-model="syncActive" /> Only active federations</label>
        </div>

        <div v-for="g in fleetGroups" :key="g.federate || 'all'">
          <div v-if="g.federate" class="grp"><span class="dot" :style="{ background: fedCss(g.federate) }"></span>{{ g.federate }} <span class="count">{{ g.items.length }}</span></div>
          <div v-for="a in g.items" :key="a.id" class="ac" @mouseenter="hovered = a.federate" @mouseleave="hovered = null">
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
                <div><dt>role</dt><dd>{{ a.role }} · {{ a.federate }}</dd></div>
                <div><dt>pos</dt><dd>{{ f1(liveById[a.id]?.pos[0]) }}, {{ f1(liveById[a.id]?.pos[1]) }}, {{ f1(liveById[a.id]?.pos[2]) }}</dd></div>
                <div><dt>vel</dt><dd>{{ f1(liveById[a.id]?.vel[0]) }}, {{ f1(liveById[a.id]?.vel[1]) }}, {{ f1(liveById[a.id]?.vel[2]) }}</dd></div>
                <div><dt>speed</dt><dd>{{ f1(speedOf(liveById[a.id]?.vel)) }} m/s</dd></div>
              </dl>
            </div>
          </div>
        </div>
        <p v-if="!fleetGroups.length || !aircraft.length" class="placeholder">— none —</p>
      </div>
      <div v-else class="strip">Aircraft</div>
    </aside>

    <!-- Stage -->
    <main class="stage">
      <div class="panes" :style="{ '--cols': panes.cols, '--rows': panes.rows }">
        <div v-for="cell in panes.cells" :key="cell.key" class="pane"
          :style="{ gridColumn: cell.span > 1 ? 'span ' + cell.span : null, borderColor: hovered && hovered === cell.federate ? cell.css : 'var(--border)' }"
          @mouseenter="hovered = cell.federate" @mouseleave="hovered = null">
          <Viewport :timeline="timeline" :theme="theme" :filter="cell.filter" :aircraft-modes="acModes" :federate-states="fedUi"
            :show-gizmo="showGizmo" :show-units="showUnits" :projection="projection" :recenter-world-nonce="recenterWorldNonce" :recenter-fed="recenterFed" />
          <div class="pane-title"><span class="dot" v-if="cell.css" :style="{ background: cell.css }"></span>{{ cell.label }}</div>
        </div>
      </div>
      <div class="timeline">
        <button class="btn" @click="togglePlay">{{ playing ? '❚❚' : '▶' }}</button>
        <input class="scrub" type="range" min="0" :max="duration || 0.0001" step="0.01" :value="currentT" @input="seek(Number($event.target.value))" />
        <span class="time">{{ timeLabel }}</span>
        <select class="speed" :value="speed" @change="setSpeed(Number($event.target.value))">
          <option v-for="s in SPEEDS" :key="s" :value="s">{{ s }}×</option>
        </select>
      </div>
    </main>
  </div>
</template>

<style scoped>
.app {
  --bg: #ffffff; --panel-bg: #fbfbfa; --card-bg: #ffffff; --border: #e6e4dd;
  --line: #d8d6cf; --text: #2c2c2a; --muted: #8a887f; --hover: #f6f5f1;
  --chip-on: #eef1ee; --chip-on-border: #9fbf9f;
  display: grid;
  grid-template-columns: 238px 256px 1fr;
  height: 100vh;
  font-family: 'Inter', system-ui, -apple-system, 'Segoe UI', sans-serif;
  color: var(--text);
  background: var(--bg);
}
.app[data-theme='dark'] {
  --bg: #1b1b1d; --panel-bg: #202023; --card-bg: #27272a; --border: #38383c;
  --line: #45454a; --text: #d6d5d0; --muted: #8f8d85; --hover: #2c2c30;
  --chip-on: #33402f; --chip-on-border: #5c7a4e;
}

.panel { position: relative; border-right: 1px solid var(--border); background: var(--panel-bg); overflow: hidden; min-width: 0; }
.panel-body { height: 100%; overflow-y: auto; padding: 14px 20px 14px 12px; }
.tab { position: absolute; top: 50%; right: 0; transform: translateY(-50%); width: 15px; height: 46px; display: flex; align-items: center; justify-content: center; border: 1px solid var(--line); border-right: none; border-radius: 7px 0 0 7px; background: var(--card-bg); color: var(--muted); cursor: pointer; z-index: 3; font-size: 13px; padding: 0; }
.tab:hover { background: var(--hover); color: var(--text); }
.strip { writing-mode: vertical-rl; transform: rotate(180deg); height: 100%; display: flex; align-items: center; justify-content: center; color: var(--muted); font-size: 11px; letter-spacing: 1.5px; text-transform: uppercase; }

.panel h1 { font-size: 15px; margin: 0 0 2px; letter-spacing: -0.2px; }
.status { font-size: 11.5px; color: var(--muted); margin: 0 0 8px; }
.panel h2 { font-size: 10.5px; text-transform: uppercase; letter-spacing: 0.6px; color: var(--muted); margin: 16px 0 6px; }
.panel-body > h2:first-child { margin-top: 0; }
.count { color: var(--muted); font-weight: 400; }
.placeholder { color: var(--muted); font-style: italic; font-size: 12.5px; }

.opt { display: flex; align-items: center; gap: 8px; font-size: 12.5px; padding: 2px 0; cursor: pointer; }
.wbtn { margin-top: 8px; width: 100%; border: 1px solid var(--line); background: var(--card-bg); color: var(--text); border-radius: 6px; padding: 6px 0; font-size: 12px; cursor: pointer; }
.wbtn:hover { background: var(--hover); }

.seg { display: flex; gap: 4px; margin-bottom: 6px; }
.seg button { flex: 1; border: 1px solid var(--line); background: var(--card-bg); border-radius: 6px; padding: 4px 0; font-size: 11.5px; cursor: pointer; color: var(--muted); }
.seg button.on { background: var(--chip-on); border-color: var(--chip-on-border); color: var(--text); }
.seg.small button { padding: 3px 0; font-size: 11px; }

.fleet-controls { display: flex; flex-direction: column; gap: 6px; margin-bottom: 10px; }
.fleet-controls > label:not(.opt) { display: flex; align-items: center; justify-content: space-between; gap: 8px; font-size: 12px; color: var(--muted); }
.fleet-controls select { flex: 1; border: 1px solid var(--line); border-radius: 6px; padding: 3px 6px; background: var(--card-bg); color: var(--text); font-size: 12px; }

.grp { position: sticky; top: 0; background: var(--panel-bg); z-index: 1; display: flex; align-items: center; gap: 6px; font-size: 11px; text-transform: uppercase; letter-spacing: 0.5px; color: var(--muted); padding: 8px 0 4px; }

.dot { display: inline-block; width: 10px; height: 10px; border-radius: 50%; flex: none; }

.fed { border: 1px solid var(--border); border-radius: 8px; padding: 8px 9px; margin-bottom: 8px; background: var(--card-bg); }
.fed.hov { border-color: var(--muted); }
.fed-top { display: flex; align-items: center; gap: 7px; }
.fed-top .sel { flex: none; }
.fed-name { font-size: 12.5px; font-weight: 600; flex: 1; }
.chip { border: 1px solid var(--line); background: var(--card-bg); border-radius: 6px; width: 24px; height: 23px; cursor: pointer; font-size: 11.5px; line-height: 1; color: var(--muted); padding: 0; }
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
.stats { margin: 0; font-size: 12px; }
.stats > div { display: flex; justify-content: space-between; padding: 1px 0; }
.stats dt { color: var(--muted); margin: 0; }
.stats dd { margin: 0; font-variant-numeric: tabular-nums; }

.stage { display: grid; grid-template-rows: 1fr auto; min-width: 0; min-height: 0; }
.panes { display: grid; grid-template-columns: repeat(var(--cols), 1fr); grid-template-rows: repeat(var(--rows), 1fr); min-width: 0; min-height: 0; }
.pane { position: relative; border: 2px solid var(--border); min-width: 0; min-height: 0; overflow: hidden; }
.pane-title { position: absolute; top: 6px; left: 8px; display: flex; align-items: center; gap: 6px; font-size: 11.5px; font-weight: 600; color: var(--text); background: color-mix(in srgb, var(--panel-bg) 80%, transparent); padding: 2px 8px; border-radius: 6px; pointer-events: none; }

.timeline { border-top: 1px solid var(--border); padding: 10px 16px; background: var(--panel-bg); display: flex; align-items: center; gap: 12px; }
.btn { border: 1px solid var(--line); background: var(--card-bg); border-radius: 6px; width: 34px; height: 30px; cursor: pointer; font-size: 12px; color: var(--text); }
.btn:hover { background: var(--hover); }
.scrub { flex: 1; accent-color: #6b6a63; }
.time { font-variant-numeric: tabular-nums; font-size: 12.5px; color: var(--muted); min-width: 92px; text-align: right; }
.speed { border: 1px solid var(--line); border-radius: 6px; padding: 4px 6px; background: var(--card-bg); color: var(--text); font-size: 12.5px; }
</style>
