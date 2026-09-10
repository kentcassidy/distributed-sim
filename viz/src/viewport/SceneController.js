import * as THREE from 'three'
import { OrbitControls } from 'three/addons/controls/OrbitControls.js'
import { makeAircraftMesh } from './aircraftMesh.js'
import { PALETTE, DEFAULT_AIRCRAFT_SIZE } from '../config.js'

// SceneController -- a plain (non-Vue) class owning one 3D viewport: renderer, scene,
// camera, controls, the render loop, and the playback clock. The Vue component feeds
// it a Timeline and control calls; it feeds back the current time via onTime().
//
// Convention (sim + Blender): world is Z-UP (z = altitude); axes X-red/Y-green/Z-blue.

export class SceneController {
  constructor(canvas) {
    this.canvas = canvas
    this._raf = null
    this.onTime = null // (t, playing, duration) => void

    // playback state
    this.timeline = null
    this.currentTime = 0
    this.playing = false
    this.speed = 1
    this.aircraftSize = DEFAULT_AIRCRAFT_SIZE

    // --- Renderer ---
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true })
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))

    // --- Scene ---
    this.scene = new THREE.Scene()
    this.scene.background = new THREE.Color(PALETTE.background)

    // --- Camera (Z-up), framed to the data once a timeline loads ---
    this.camera = new THREE.PerspectiveCamera(50, 1, 0.1, 1e7)
    this.camera.up.set(0, 0, 1)
    this.camera.position.set(8, -8, 6)
    this.camera.lookAt(0, 0, 0)

    // --- Controls: middle-drag orbits about center; wheel zoom; L/R drag pans ---
    this.controls = new OrbitControls(this.camera, this.renderer.domElement)
    this.controls.enableDamping = true
    this.controls.dampingFactor = 0.08
    this.controls.mouseButtons = {
      LEFT: THREE.MOUSE.PAN,
      MIDDLE: THREE.MOUSE.ROTATE,
      RIGHT: THREE.MOUSE.PAN,
    }

    // --- Lights (for the aircraft's standard material) ---
    this.scene.add(new THREE.AmbientLight(0xffffff, 0.8))
    const key = new THREE.DirectionalLight(0xffffff, 0.85)
    key.position.set(1, -1, 2)
    this.scene.add(key)

    // groups we rebuild whenever the timeline changes
    this.worldGroup = new THREE.Group() // room + axes
    this.fleetGroup = new THREE.Group() // aircraft meshes
    this.scene.add(this.worldGroup, this.fleetGroup)
    this.meshes = new Map() // aircraft id -> mesh

    this._clock = new THREE.Clock()
    this._onResize = this._onResize.bind(this)
    this._resizeObserver = new ResizeObserver(this._onResize)
    this._resizeObserver.observe(canvas.parentElement)
    this._onResize()

    this._loop = this._loop.bind(this)
    this._raf = requestAnimationFrame(this._loop)
  }

  // --- public API (called from Vue) ---

  setTimeline(timeline) {
    this.timeline = timeline
    this.currentTime = 0

    this._clearGroup(this.worldGroup)
    this._clearGroup(this.fleetGroup)
    this.meshes.clear()

    const world = this._worldBounds(timeline.bounds)
    this._buildRoom(world)
    this._buildAxes(world)

    for (const ac of timeline.aircraft) {
      const mesh = makeAircraftMesh(ac.color)
      mesh.scale.setScalar(this.aircraftSize / 2) // base mesh is ~2 units long
      this.fleetGroup.add(mesh)
      this.meshes.set(ac.id, mesh)
    }

    this._frameCamera(world)
    this._applyTime()
  }

  setAircraftSize(size) {
    this.aircraftSize = size
    const s = size / 2
    for (const mesh of this.meshes.values()) mesh.scale.setScalar(s)
  }

  play() {
    this.playing = true
  }
  pause() {
    this.playing = false
  }
  togglePlay() {
    this.playing = !this.playing
  }
  setSpeed(s) {
    this.speed = s
  }
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
    this._clearGroup(this.fleetGroup)
    this.renderer.dispose()
  }

  // --- world building ---

  // Placeholder worldspace box until the sim emits real world params (a meta line):
  // take the data AABB, fold in the origin so the axes cross at (0,0,0), stop any
  // dimension from collapsing to a sliver, then pad. Gives a real cuboid to inhabit.
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

  // The worldspace "room": a box drawn with BackSide, so only the faces BEHIND the
  // scene render. Near walls/floor never occlude; the far walls, and the ceiling when
  // you look from below, appear on their own. This is the backwall behaviour requested.
  _buildRoom(world) {
    const size = [world.max[0] - world.min[0], world.max[1] - world.min[1], world.max[2] - world.min[2]]
    const center = [(world.min[0] + world.max[0]) / 2, (world.min[1] + world.max[1]) / 2, (world.min[2] + world.max[2]) / 2]
    const geom = new THREE.BoxGeometry(size[0], size[1], size[2])
    const mat = new THREE.MeshBasicMaterial({ color: PALETTE.walls, side: THREE.BackSide })
    const room = new THREE.Mesh(geom, mat)
    room.position.set(center[0], center[1], center[2])
    this.worldGroup.add(room)
    this._worldMaxExtent = Math.max(...size)
  }

  // Thicker axes than AxesHelper's 1px lines: colored rods through the origin, each
  // spanning the cuboid along its dimension.
  _buildAxes(world) {
    const r = (this._worldMaxExtent || 1) * 0.0016
    this.worldGroup.add(this._rod([world.min[0], 0, 0], [world.max[0], 0, 0], PALETTE.axisX, r))
    this.worldGroup.add(this._rod([0, world.min[1], 0], [0, world.max[1], 0], PALETTE.axisY, r))
    this.worldGroup.add(this._rod([0, 0, world.min[2]], [0, 0, world.max[2]], PALETTE.axisZ, r))
  }

  _rod(from, to, color, radius) {
    const a = new THREE.Vector3(...from)
    const b = new THREE.Vector3(...to)
    const dir = new THREE.Vector3().subVectors(b, a)
    const len = dir.length() || 1e-6
    const geom = new THREE.CylinderGeometry(radius, radius, len, 12)
    const mesh = new THREE.Mesh(geom, new THREE.MeshBasicMaterial({ color }))
    mesh.position.copy(a).add(b).multiplyScalar(0.5)
    mesh.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir.normalize())
    return mesh
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

  // --- per-frame ---

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
      if (this.currentTime >= dur) this.currentTime = 0 // loop the replay
      this._applyTime()
    }
    this.controls.update()
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
      child.geometry?.dispose()
      child.material?.dispose()
    }
  }
}
