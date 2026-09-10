import * as THREE from 'three'
import { OrbitControls } from 'three/addons/controls/OrbitControls.js'
import { makeAircraftMesh } from './aircraftMesh.js'
import { buildWorldspace } from './worldspace.js'
import { DEFAULT_AIRCRAFT_SIZE } from '../config.js'

// SceneController -- one 3D viewport: renderer, scene, camera, controls, render loop,
// playback clock, and the display state (per-aircraft mode, per-federate visibility/
// size/highlight/halo). Vue feeds it a Timeline and control calls; it emits time back.
// Convention: Z-up world, X-red/Y-green/Z-blue axes.

const HIGHLIGHT_SCALE = 1.7

export class SceneController {
  constructor(canvas) {
    this.canvas = canvas
    this._raf = null
    this.onTime = null

    // playback
    this.timeline = null
    this.currentTime = 0
    this.playing = false
    this.speed = 1

    // display state
    this.aircraftMode = new Map() // id -> 'show' | 'hide' | 'highlight'
    this.fedVisible = new Map() // name -> bool
    this.fedSize = new Map() // name -> metres
    this.fedOf = new Map() // id -> federate name
    this._fedBoxes = new Map() // name -> { box, halo }

    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true })
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))

    this.scene = new THREE.Scene()
    this.scene.background = new THREE.Color(0xf7f7f5)

    this.camera = new THREE.PerspectiveCamera(50, 1, 0.1, 1e7)
    this.camera.up.set(0, 0, 1)
    this.camera.position.set(8, -8, 6)
    this.camera.lookAt(0, 0, 0)

    this.controls = new OrbitControls(this.camera, this.renderer.domElement)
    this.controls.enableDamping = true
    this.controls.dampingFactor = 0.08
    this.controls.mouseButtons = {
      LEFT: THREE.MOUSE.PAN,
      MIDDLE: THREE.MOUSE.ROTATE,
      RIGHT: THREE.MOUSE.PAN,
    }

    this.scene.add(new THREE.AmbientLight(0xffffff, 0.8))
    const key = new THREE.DirectionalLight(0xffffff, 0.85)
    key.position.set(1, -1, 2)
    this.scene.add(key)

    this.worldGroup = new THREE.Group() // room + axes + grids
    this.overlayGroup = new THREE.Group() // federate boxes / halos
    this.fleetGroup = new THREE.Group() // aircraft
    this.scene.add(this.worldGroup, this.overlayGroup, this.fleetGroup)
    this.meshes = new Map() // id -> mesh
    this.worldspace = null
    this.world = null // {min,max}

    this._clock = new THREE.Clock()
    this._onResize = this._onResize.bind(this)
    this._resizeObserver = new ResizeObserver(this._onResize)
    this._resizeObserver.observe(canvas.parentElement)
    this._onResize()

    this._loop = this._loop.bind(this)
    this._raf = requestAnimationFrame(this._loop)
  }

  // --- timeline / fleet --------------------------------------------------------

  setTimeline(timeline) {
    this.timeline = timeline
    this.currentTime = 0

    this._clearGroup(this.worldGroup)
    this._clearGroup(this.overlayGroup)
    this._clearGroup(this.fleetGroup)
    this.meshes.clear()
    this._fedBoxes.clear()

    this.world = this._worldBounds(timeline.bounds)
    this.worldspace = buildWorldspace(this.world)
    this.worldGroup.add(this.worldspace.group)

    // default display state
    this.fedOf.clear()
    for (const f of timeline.federates) {
      if (!this.fedVisible.has(f.name)) this.fedVisible.set(f.name, true)
      if (!this.fedSize.has(f.name)) this.fedSize.set(f.name, DEFAULT_AIRCRAFT_SIZE)
    }
    for (const ac of timeline.aircraft) {
      this.fedOf.set(ac.id, ac.federate)
      if (!this.aircraftMode.has(ac.id)) this.aircraftMode.set(ac.id, 'show')
      const mesh = makeAircraftMesh(ac.color)
      this.fleetGroup.add(mesh)
      this.meshes.set(ac.id, mesh)
    }

    this._frameCamera(this.world)
    this._refreshFleet()
    this._applyTime()
  }

  // --- display controls (called from the panel) --------------------------------

  setAircraftMode(id, mode) {
    this.aircraftMode.set(id, mode)
    this._refreshFleet()
  }

  setFederateVisible(name, visible) {
    this.fedVisible.set(name, visible)
    this._refreshFleet()
  }

  setFederateSize(name, size) {
    this.fedSize.set(name, size)
    this._refreshFleet()
  }

  setFederateHighlight(name, on) {
    this._ensureFedBox(name)
    this._fedBoxes.get(name).box.visible = on
  }

  setFederateHalo(name, on) {
    this._ensureFedBox(name)
    this._fedBoxes.get(name).halo.visible = on
  }

  // Recompute every mesh's visibility, scale, and emphasis from the display state.
  _refreshFleet() {
    for (const [id, mesh] of this.meshes) {
      const fed = this.fedOf.get(id)
      const mode = this.aircraftMode.get(id) || 'show'
      const fedOn = this.fedVisible.get(fed) !== false
      const highlight = mode === 'highlight'

      mesh.visible = fedOn && mode !== 'hide'
      const size = this.fedSize.get(fed) || DEFAULT_AIRCRAFT_SIZE
      mesh.scale.setScalar((size / 2) * (highlight ? HIGHLIGHT_SCALE : 1))
      mesh.material.emissive.setHex(highlight ? mesh.material.color.getHex() : 0x000000)
      mesh.material.emissiveIntensity = highlight ? 0.55 : 0
    }
  }

  // For now a federate "owns" the whole worldspace, so its box IS the worldspace
  // border (a strong dashed cuboid); the halo is an outward offset. When federates
  // own sectors, this becomes the union of their sectors.
  _ensureFedBox(name) {
    if (this._fedBoxes.has(name)) return
    const color = this._fedColor(name)
    const box = dashedBox(this.world, color, this._worldMaxExtent() * 0.02, 2.2)
    const halo = dashedBox(this._expand(this.world, 0.06), color, this._worldMaxExtent() * 0.02, 1.2)
    box.visible = false
    halo.visible = false
    this.overlayGroup.add(box, halo)
    this._fedBoxes.set(name, { box, halo })
  }

  _fedColor(name) {
    const f = this.timeline?.federates.find((f) => f.name === name)
    return f ? f.color : 0x333333
  }

  // --- playback ---------------------------------------------------------------

  play() { this.playing = true }
  pause() { this.playing = false }
  togglePlay() { this.playing = !this.playing }
  setSpeed(s) { this.speed = s }
  seek(t) {
    if (!this.timeline) return
    this.currentTime = Math.max(0, Math.min(t, this.timeline.duration))
    this._applyTime()
  }

  dispose() {
    if (this._raf) cancelAnimationFrame(this._raf)
    this._resizeObserver.disconnect()
    this.controls.dispose()
    this._clearGroup(this.worldGroup)
    this._clearGroup(this.overlayGroup)
    this._clearGroup(this.fleetGroup)
    this.renderer.dispose()
  }

  // --- helpers ----------------------------------------------------------------

  _worldBounds(dataBounds) {
    const min = [...dataBounds.min]
    const max = [...dataBounds.max]
    for (let i = 0; i < 3; i++) {
      min[i] = Math.min(min[i], 0)
      max[i] = Math.max(max[i], 0)
    }
    const ext = [max[0] - min[0], max[1] - min[1], max[2] - min[2]]
    const largest = Math.max(...ext) || 1
    const floor = largest * 0.2
    for (let i = 0; i < 3; i++) {
      if (ext[i] < floor) {
        const c = (min[i] + max[i]) / 2
        min[i] = c - floor / 2
        max[i] = c + floor / 2
      }
    }
    for (let i = 0; i < 3; i++) {
      const pad = (max[i] - min[i]) * 0.05
      min[i] -= pad
      max[i] += pad
    }
    return { min, max }
  }

  _expand(b, frac) {
    const min = [...b.min]
    const max = [...b.max]
    for (let i = 0; i < 3; i++) {
      const m = (max[i] - min[i]) * frac
      min[i] -= m
      max[i] += m
    }
    return { min, max }
  }

  _worldMaxExtent() {
    const b = this.world
    return Math.max(b.max[0] - b.min[0], b.max[1] - b.min[1], b.max[2] - b.min[2])
  }

  _frameCamera(world) {
    const center = new THREE.Vector3(
      (world.min[0] + world.max[0]) / 2,
      (world.min[1] + world.max[1]) / 2,
      (world.min[2] + world.max[2]) / 2,
    )
    const diag = new THREE.Vector3(
      world.max[0] - world.min[0],
      world.max[1] - world.min[1],
      world.max[2] - world.min[2],
    ).length()
    const dir = new THREE.Vector3(1, -1, 0.7).normalize()
    this.camera.position.copy(center).addScaledVector(dir, diag * 0.9)
    this.camera.near = diag * 0.001
    this.camera.far = diag * 20
    this.camera.updateProjectionMatrix()
    this.controls.target.copy(center)
    this.controls.update()
  }

  _applyTime() {
    if (!this.timeline) return
    for (const s of this.timeline.sample(this.currentTime)) {
      const mesh = this.meshes.get(s.id)
      if (!mesh) continue
      mesh.position.set(s.pos[0], s.pos[1], s.pos[2])
      mesh.quaternion.set(s.quat[0], s.quat[1], s.quat[2], s.quat[3]).normalize()
    }
  }

  _loop() {
    const dt = this._clock.getDelta()
    if (this.playing && this.timeline) {
      const dur = this.timeline.duration
      this.currentTime += dt * this.speed
      if (this.currentTime >= dur) this.currentTime = 0
      this._applyTime()
    }
    this.controls.update()
    if (this.worldspace) this.worldspace.update(this.camera)
    this.renderer.render(this.scene, this.camera)
    if (this.onTime) {
      this.onTime(this.currentTime, this.playing, this.timeline ? this.timeline.duration : 0)
    }
    this._raf = requestAnimationFrame(this._loop)
  }

  _onResize() {
    const parent = this.canvas.parentElement
    const w = parent.clientWidth
    const h = parent.clientHeight
    if (w === 0 || h === 0) return
    this.renderer.setSize(w, h, false)
    this.camera.aspect = w / h
    this.camera.updateProjectionMatrix()
  }

  _clearGroup(group) {
    for (let i = group.children.length - 1; i >= 0; i--) {
      const child = group.children[i]
      group.remove(child)
      child.traverse?.((o) => {
        o.geometry?.dispose?.()
        o.material?.map?.dispose?.()
        o.material?.dispose?.()
      })
    }
  }
}

// A dashed wireframe cuboid for federate borders / halos.
function dashedBox(bounds, color, dash, opacity) {
  const size = [
    bounds.max[0] - bounds.min[0],
    bounds.max[1] - bounds.min[1],
    bounds.max[2] - bounds.min[2],
  ]
  const edges = new THREE.EdgesGeometry(new THREE.BoxGeometry(...size))
  const line = new THREE.LineSegments(
    edges,
    new THREE.LineDashedMaterial({ color, dashSize: dash, gapSize: dash * 0.6, transparent: true, opacity }),
  )
  line.position.set(
    (bounds.min[0] + bounds.max[0]) / 2,
    (bounds.min[1] + bounds.max[1]) / 2,
    (bounds.min[2] + bounds.max[2]) / 2,
  )
  line.computeLineDistances()
  return line
}
