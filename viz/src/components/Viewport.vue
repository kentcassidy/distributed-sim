<script setup>
// A single 3D viewport. Vue owns the DOM; Three.js owns the <canvas> and its own
// render loop. THE RULE for this stack: Three.js objects (scene, camera, meshes)
// stay OUT of Vue's reactivity -- we keep the controller in a plain variable, never
// a ref()/reactive(). Wrapping Three objects in reactivity tanks performance and can
// corrupt Three's internals. Data flows IN via props/events (added in later steps).
import { onMounted, onBeforeUnmount, ref } from 'vue'
import { SceneController } from '../viewport/SceneController.js'

const canvas = ref(null)
let scene = null   // plain var on purpose -- NOT reactive

onMounted(() => {
  scene = new SceneController(canvas.value)
})

onBeforeUnmount(() => {
  scene?.dispose()
  scene = null
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
