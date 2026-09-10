<script setup>
// One pane. Vue owns the DOM; Three owns the <canvas>. All persistent state arrives as
// PROPS (so a freshly-mounted pane gets the current world/theme/display state), and the
// two transient actions (recenter) arrive as incrementing nonce props. Playback time is
// read from the shared clock inside the SceneController, so panes need no time prop.
import { onMounted, onBeforeUnmount, ref, watch } from 'vue'
import { SceneController } from '../viewport/SceneController.js'

const props = defineProps({
  timeline: { type: Object, default: null },
  theme: { type: String, default: 'light' },
  filter: { type: Array, default: null }, // federate names, or null for all
  aircraftModes: { type: Object, default: () => ({}) },
  federateStates: { type: Object, default: () => ({}) },
  showGizmo: { type: Boolean, default: true },
  showUnits: { type: Boolean, default: true },
  projection: { type: String, default: 'perspective' },
  recenterWorldNonce: { type: Number, default: 0 },
  recenterFed: { type: Object, default: () => ({ name: null, n: 0 }) },
})

const canvas = ref(null)
let scene = null

function applyAll() {
  scene.setTheme(props.theme)
  scene.setViewFilter(props.filter)
  if (props.timeline) scene.setTimeline(props.timeline)
  scene.applyFederateStates(props.federateStates)
  scene.applyAircraftModes(props.aircraftModes)
  scene.setGizmoVisible(props.showGizmo)
  scene.setUnitsVisible(props.showUnits)
  scene.setProjection(props.projection)
}

onMounted(() => {
  scene = new SceneController(canvas.value)
  applyAll()
})
onBeforeUnmount(() => {
  scene?.dispose()
  scene = null
})

watch(() => props.timeline, (t) => {
  if (!scene || !t) return
  scene.setTimeline(t)
  scene.applyFederateStates(props.federateStates)
  scene.applyAircraftModes(props.aircraftModes)
})
watch(() => props.theme, (t) => scene?.setTheme(t))
watch(() => props.filter, (f) => scene?.setViewFilter(f))
watch(() => props.aircraftModes, (m) => scene?.applyAircraftModes(m), { deep: true })
watch(() => props.federateStates, (s) => scene?.applyFederateStates(s), { deep: true })
watch(() => props.showGizmo, (v) => scene?.setGizmoVisible(v))
watch(() => props.showUnits, (v) => scene?.setUnitsVisible(v))
watch(() => props.projection, (v) => scene?.setProjection(v))

watch(() => props.recenterWorldNonce, () => scene?.recenterWorld())
watch(() => props.recenterFed.n, () => {
  const name = props.recenterFed.name
  if (!name || !scene) return
  // only act if this pane actually shows that federate
  if (props.filter === null || props.filter.includes(name)) scene.recenterFederate(name)
})
</script>

<template>
  <div class="viewport">
    <canvas ref="canvas"></canvas>
  </div>
</template>

<style scoped>
.viewport { width: 100%; height: 100%; position: relative; overflow: hidden; }
canvas { display: block; width: 100%; height: 100%; }
</style>
