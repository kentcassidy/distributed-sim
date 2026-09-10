<script setup>
// The shell: loads sim_out on mount, drives the viewport, and renders the panel +
// timeline. Playback time is owned by the SceneController (it has the render clock);
// this component just mirrors it for the slider and issues control calls.
import { ref, shallowRef, onMounted, computed } from 'vue'
import Viewport from './components/Viewport.vue'
import { loadSimOut } from './data/loader.js'
import { DEFAULT_AIRCRAFT_SIZE, AIRCRAFT_SIZE_RANGE, hexToCss } from './config.js'

const vp = ref(null)
const timeline = shallowRef(null) // plain data object -- keep it out of deep reactivity
const status = ref('Loading sim_out…')
const aircraft = ref([])
const federates = ref([])

const playing = ref(false)
const currentT = ref(0)
const duration = ref(0)
const speed = ref(1)
const aircraftSize = ref(DEFAULT_AIRCRAFT_SIZE)

const SPEEDS = [0.25, 0.5, 1, 2, 4]

onMounted(async () => {
  try {
    const tl = await loadSimOut()
    timeline.value = tl
    duration.value = tl.duration
    aircraft.value = tl.aircraft.map((a) => ({ ...a, css: hexToCss(a.color) }))
    federates.value = tl.federates.map((f) => ({ ...f, css: hexToCss(f.color) }))
    status.value = `${aircraft.value.length} aircraft · ${federates.value.length} federate file(s) · ${duration.value.toFixed(1)}s`
    vp.value?.play() // autoplay so the first thing you see is motion
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
</script>

<template>
  <div class="app">
    <aside class="panel">
      <h1>DFF Viewer</h1>
      <p class="muted">Distributed Flight-Dynamics Federation</p>
      <p class="status">{{ status }}</p>

      <section>
        <h2>Federates</h2>
        <ul class="list">
          <li v-for="f in federates" :key="f.name">
            <span class="dot" :style="{ background: f.css }"></span>{{ f.name }}
          </li>
          <li v-if="!federates.length" class="placeholder">— none —</li>
        </ul>
      </section>

      <section>
        <h2>Aircraft</h2>
        <ul class="list">
          <li v-for="a in aircraft" :key="a.id">
            <span class="dot" :style="{ background: a.css }"></span>id {{ a.id }}
            <span class="sub">· {{ a.federate }}</span>
          </li>
          <li v-if="!aircraft.length" class="placeholder">— none —</li>
        </ul>
      </section>

      <section>
        <h2>Aircraft size</h2>
        <input
          type="range"
          :min="AIRCRAFT_SIZE_RANGE.min"
          :max="AIRCRAFT_SIZE_RANGE.max"
          step="1"
          v-model.number="aircraftSize"
        />
        <span class="sub">{{ aircraftSize }} m</span>
      </section>
    </aside>

    <main class="stage">
      <div class="viewport-wrap">
        <Viewport ref="vp" :timeline="timeline" :aircraft-size="aircraftSize" @time="onTime" />
      </div>

      <div class="timeline">
        <button class="btn" @click="vp?.togglePlay()">{{ playing ? '❚❚' : '▶' }}</button>
        <input
          class="scrub"
          type="range"
          min="0"
          :max="duration || 0.0001"
          step="0.01"
          :value="currentT"
          @input="vp?.seek(Number($event.target.value))"
        />
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
  grid-template-columns: minmax(240px, 1fr) 2fr;
  height: 100vh;
  font-family: 'Inter', system-ui, -apple-system, 'Segoe UI', sans-serif;
  color: #2c2c2a;
  background: #ffffff;
}

.panel { border-right: 1px solid #e6e4dd; padding: 18px 16px; background: #fbfbfa; overflow-y: auto; }
.panel h1 { font-size: 16px; margin: 0; letter-spacing: -0.2px; }
.panel .muted { color: #8a887f; font-size: 12px; margin: 2px 0 10px; }
.panel .status { font-size: 12px; color: #5f5e5a; margin: 0 0 8px; }
.panel h2 { font-size: 11px; text-transform: uppercase; letter-spacing: 0.6px; color: #8a887f; margin: 18px 0 6px; }

.list { list-style: none; margin: 0; padding: 0; }
.list li { font-size: 13px; padding: 2px 0; display: flex; align-items: center; }
.dot { display: inline-block; width: 10px; height: 10px; border-radius: 50%; margin-right: 8px; }
.sub { color: #8a887f; font-size: 12px; margin-left: 4px; }
.placeholder { color: #b6b4ac; font-style: italic; }

input[type='range'] { width: 100%; accent-color: #6b6a63; }

.stage { display: grid; grid-template-rows: 1fr auto; min-width: 0; min-height: 0; }
.viewport-wrap { position: relative; min-height: 0; }

.timeline {
  border-top: 1px solid #e6e4dd;
  padding: 10px 16px;
  background: #fbfbfa;
  display: flex;
  align-items: center;
  gap: 12px;
}
.btn {
  border: 1px solid #d8d6cf; background: #fff; border-radius: 6px;
  width: 34px; height: 30px; cursor: pointer; font-size: 12px; color: #2c2c2a;
}
.btn:hover { background: #f1f0eb; }
.scrub { flex: 1; }
.time { font-variant-numeric: tabular-nums; font-size: 12.5px; color: #5f5e5a; min-width: 92px; text-align: right; }
.speed { border: 1px solid #d8d6cf; border-radius: 6px; padding: 4px 6px; background: #fff; font-size: 12.5px; }
</style>
