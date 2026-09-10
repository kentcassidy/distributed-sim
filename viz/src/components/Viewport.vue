<script setup>
// Vue<->Three boundary. Vue owns the DOM; Three owns the <canvas> and its loop.
// THE RULE: Three objects never enter Vue reactivity -- the controller is a plain
// variable. Data flows IN via props (timeline, aircraftSize); control flows in via
// the exposed methods; the current time flows OUT via the 'time' event.
import { onMounted, onBeforeUnmount, ref, watch } from 'vue'
import { SceneController } from '../viewport/SceneController.js'

const props = defineProps({
  timeline: { type: Object, default: null },
  aircraftSize: { type: Number, default: 40 },
})
const emit = defineEmits(['time'])

const canvas = ref(null)
let scene = null // plain var on purpose -- NOT reactive

onMounted(() => {
  scene = new SceneController(canvas.value)
  scene.onTime = (t, playing, duration) => emit('time', { t, playing, duration })
  if (props.timeline) scene.setTimeline(props.timeline)
  scene.setAircraftSize(props.aircraftSize)
})

onBeforeUnmount(() => {
  scene?.dispose()
  scene = null
})

watch(() => props.timeline, (tl) => { if (scene && tl) scene.setTimeline(tl) })
watch(() => props.aircraftSize, (s) => scene?.setAircraftSize(s))

defineExpose({
  play: () => scene?.play(),
  pause: () => scene?.pause(),
  togglePlay: () => scene?.togglePlay(),
  seek: (t) => scene?.seek(t),
  setSpeed: (s) => scene?.setSpeed(s),
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
