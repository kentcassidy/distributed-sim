import * as THREE from 'three'
import { OrbitControls } from 'three/addons/controls/OrbitControls.js'
import { CSS2DRenderer, CSS2DObject } from 'three/addons/renderers/CSS2DRenderer.js'
import { makeAircraftMesh } from './aircraftMesh.js'
import { buildWorldspace, niceStep, ticksFor, formatTick } from './worldspace.js'
import { buildGizmo } from './gizmo.js'
import { DEFAULT_AIRCRAFT_SIZE, THEMES } from '../config.js'

// SceneController -- one 3D viewport: renderer(s), scene, camera, controls, render loop,
// playback clock, display state, constant-size axis labels (CSS2D), corner gizmo, and a
// perspective/isometric projection toggle. Vue feeds it a Timeline + control calls.
// Convention: Z-up world, X-red/Y-green/Z-blue axes.

const HIGHLIGHT_SCALE = 1.7

export class SceneController {
  constructor(canvas) {
    this.canvas = canvas
    this._raf = null
    this.onTime = null

    this.theme = 'light'
    this.palette = THEMES.light
    this.projection = 'perspective'

    // playback
    this.timeline = null
    this.currentTime = 0
    this.playing = false
    this.speed = 1

    // display state
    this.aircraftMode = new Map()
    this.fedVisible = new Map()
    this.fedSize = new Map()
    this.fedOf = new Map()
    this._fedBoxes = new Map()
    this.showUnits = true

    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true })
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))

    // DOM overlay for crisp, constant-size tick labels
    this.labelRenderer = new CSS2DRenderer()
    const lr = this.labelRenderer.domElement
    lr.style.position = 'absolute'
    lr.style.top = '0'
    lr.style.left = '0'
    lr.style.pointerEvents = 'none'
    canvas.parentElement.appendChild(lr)

    this.scene = new THREE.Scene()
    this.scene.background = new THREE.Color(this.palette.background)

    this.camera = this._makeCamera('perspective', 1)
    this.camera.position.set(8, -8, 6)
    this.camera.lookAt(0, 0, 0)
    this._buildControls()

    this.scene.add(new THREE.AmbientLight(0xffffff, 0.8))
    const key = new THREE.DirectionalLight(0xffffff, 0.85)
    key.position.set(1, -1, 2)
    this.scene.add(key)

    this.worldGroup = new THREE.Group()
    this.overlayGroup = new THREE.Group()
    this.fleetGroup = new THREE.Group()
    this.labelGroup = new THREE.Group()
    this.scene.add(this.worldGroup, this.overlayGroup, this.fleetGroup, this.labelGroup)
    this.meshes = new Map()
    this.worldspace = null
    this.world = null
    this._labels = []

    this.gizmo = buildGizmo(this.palette)
    this.gizmoVisible = true

    this._clock = new THREE.Clock()
    this._tmpSize = new THREE.Vector2()
    this._onResize = this._onResize.bind(this)
    this._resizeObserver = new ResizeObserver(this._onResize)
    this._resizeObserver.observe(canvas.parentElement)
    this._onResize()

    this._loop = this._loop.bind(this)
    this._raf = requestAnimationFrame(this._loop)
  }

  // --- camera / controls -------------------------------------------------------

  _makeCamera(mode, aspect) {
    let cam
    if (mode === 'isometric') {
      const h = this._orthoH || 1000
      cam = new THREE.OrthographicCamera((-h * aspect) / 2, (h * aspect) / 2, h / 2, -h / 2, -1e6, 1e6)
    } else {
      cam = new THREE.PerspectiveCamera(50, aspect, 0.1, 1e7)
    }
    cam.up.set(0, 0, 1)
    return cam
  }

  _buildControls() {
    this.controls = new OrbitControls(this.camera, this.renderer.domElement)
    this.controls.enableDamping = true
    this.controls.dampingFactor = 0.08
    this.controls.mouseButtons = {
      LEFT: THREE.MOUSE.PAN,
      MIDDLE: THREE.MOUSE.ROTATE,
      RIGHT: THREE.MOUSE.PAN,
    }
  }

  setProjection(mode) {
    if (mode === this.projection) return
    this.projection = mode
    if (!this.world) return

    const target = this.controls.target.clone()
    const offset = this.camera.position.clone().sub(target)
    const dir = offset.clone().normalize()
    const dist = offset.length() || this._worldDiag()

    if (mode === 'isometric') this._orthoH = this._worldDiag() / (this.camera.zoom || 1)
    this.controls.dispose()
    this.camera = this._makeCamera(mode, this._aspect())
    this.camera.position.copy(target).addScaledVector(dir, dist)
    this.camera.updateProjectionMatrix()
    this._buildControls()
    this.controls.target.copy(target)
    this.controls.update()
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
    this.worldspace = buildWorldspace(this.world, this.palette)
    this.worldGroup.add(this.worldspace.group)
    this._buildLabels()

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

  // --- display controls --------------------------------------------------------

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
  setUnitsVisible(on) {
    this.showUnits = on
    this.labelGroup.visible = on
    for (const l of this._labels) l.obj.visible = on
  }

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

  _ensureFedBox(name) {
    if (this._fedBoxes.has(name)) return
    const color = this._fedColor(name)
    const box = dashedBox(this.world, color, this._worldMaxExtent() * 0.02, 0.95)
    const halo = dashedBox(this._expand(this.world, 0.06), color, this._worldMaxExtent() * 0.02, 0.45)
    box.visible = false
    halo.visible = false
    this.overlayGroup.add(box, halo)
    this._fedBoxes.set(name, { box, halo })
  }
  _fedColor(name) {
    const f = this.timeline?.federates.find((f) => f.name === name)
    return f ? f.color : 0x333333
  }

  // --- theme / gizmo -----------------------------------------------------------

  setTheme(theme) {
    this.theme = theme
    this.palette = THEMES[theme] || THEMES.light
    this.scene.background = new THREE.Color(this.palette.background)
    if (this.world) {
      this._clearGroup(this.worldGroup)
      this.worldspace = buildWorldspace(this.world, this.palette)
      this.worldGroup.add(this.worldspace.group)
      this._buildLabels()
    }
    this._disposeScene(this.gizmo.scene)
    this.gizmo = buildGizmo(this.palette)
  }
  setGizmoVisible(on) {
    this.gizmoVisible = on
  }

  // --- playback ----------------------------------------------------------------

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
    for (const l of this._labels) l.el.remove()
    this.labelRenderer.domElement.remove()
    if (this.gizmo) this._disposeScene(this.gizmo.scene)
    this.renderer.dispose()
  }

  // --- labels (CSS2D, constant screen size) ------------------------------------

  _buildLabels() {
    for (const l of this._labels) l.el.remove()
    this._labels.length = 0
    this._clearGroup(this.labelGroup)
    if (!this.world) return

    // Per-axis step so every axis (incl. the thin Z) gets ~6 readable ticks. (The grid
    // cells still use one GLOBAL step to stay square; labels are decoupled from that.)
    for (let axis = 0; axis < 3; axis++) {
      const step = niceStep(this.world.max[axis] - this.world.min[axis])
      for (const v of ticksFor(this.world.min[axis], this.world.max[axis], step)) {
        if (Math.abs(v) < step * 1e-6) continue // skip origin
        const div = document.createElement('div')
        div.textContent = formatTick(v)
        div.style.font = '12px system-ui, sans-serif'
        div.style.color = this.palette.tick
        div.style.whiteSpace = 'nowrap'
        div.style.pointerEvents = 'none'
        const obj = new CSS2DObject(div)
        obj.visible = this.showUnits
        this.labelGroup.add(obj)
        this._labels.push({ axis, value: v, obj, el: div })
      }
    }
    this.labelGroup.visible = this.showUnits
  }

  // Park labels on the box edges, matplotlib-style: X/Y along the back-bottom edges,
  // Z up the back vertical edge; the chosen edges flip as the camera orbits.
  _updateLabels(camera) {
    if (!this.world || this._labels.length === 0) return
    const b = this.world
    const c = [(b.min[0] + b.max[0]) / 2, (b.min[1] + b.max[1]) / 2, (b.min[2] + b.max[2]) / 2]
    const cam = camera.position
    const off = this._worldMaxExtent() * 0.015
    const val = (ax, side) => (side ? b.max[ax] : b.min[ax])

    // A wall is drawn only where we see its interior (same rule as the grids/room).
    const wallVisible = (ax, side) => {
      const n = side ? 1 : -1 // outward normal component along ax
      return n * (cam.getComponent(ax) - val(ax, side)) < 0
    }

    // For a label axis A, choose the edge (fixed sides of the other two axes) that is
    // attached to at least one VISIBLE wall and is closest to the camera -- so labels
    // ride a drawn grid plane instead of floating on a hidden corner.
    const pickEdge = (A) => {
      const [B, C] = A === 0 ? [1, 2] : A === 1 ? [0, 2] : [0, 1]
      let best = null
      let bestD = Infinity
      for (const sB of [0, 1]) {
        for (const sC of [0, 1]) {
          if (!(wallVisible(B, sB) || wallVisible(C, sC))) continue
          const p = [0, 0, 0]
          p[A] = c[A]
          p[B] = val(B, sB)
          p[C] = val(C, sC)
          const d = (p[0] - cam.x) ** 2 + (p[1] - cam.y) ** 2 + (p[2] - cam.z) ** 2
          if (d < bestD) {
            bestD = d
            best = { B, C, sB, sC }
          }
        }
      }
      if (!best) {
        best = { B, C, sB: cam.getComponent(B) > c[B] ? 1 : 0, sC: cam.getComponent(C) > c[C] ? 1 : 0 }
      }
      return best
    }

    const edges = [pickEdge(0), pickEdge(1), pickEdge(2)]
    for (const l of this._labels) {
      const e = edges[l.axis]
      const coord = [0, 0, 0]
      coord[l.axis] = l.value
      coord[e.B] = val(e.B, e.sB) + (e.sB ? off : -off) // nudge outward for legibility
      coord[e.C] = val(e.C, e.sC) + (e.sC ? off : -off)
      l.obj.position.set(coord[0], coord[1], coord[2])
    }
  }

  // --- geometry helpers --------------------------------------------------------

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
  _worldDiag() {
    const b = this.world
    return Math.hypot(b.max[0] - b.min[0], b.max[1] - b.min[1], b.max[2] - b.min[2])
  }
  _aspect() {
    const p = this.canvas.parentElement
    return p.clientHeight ? p.clientWidth / p.clientHeight : 1
  }

  // Fit `bounds` in view. preserveDir keeps the current orbit angle (used by the
  // recenter buttons); otherwise a default 3/4 angle is used (initial framing).
  _frameCamera(bounds, preserveDir = false) {
    const center = new THREE.Vector3(
      (bounds.min[0] + bounds.max[0]) / 2,
      (bounds.min[1] + bounds.max[1]) / 2,
      (bounds.min[2] + bounds.max[2]) / 2,
    )
    const diag =
      Math.hypot(bounds.max[0] - bounds.min[0], bounds.max[1] - bounds.min[1], bounds.max[2] - bounds.min[2]) || 1
    let dir
    if (preserveDir && this.controls) {
      dir = this.camera.position.clone().sub(this.controls.target)
      if (dir.lengthSq() < 1e-9) dir.set(1, -1, 0.7)
      dir.normalize()
    } else {
      dir = new THREE.Vector3(1, -1, 0.7).normalize()
    }
    if (this.camera.isOrthographicCamera) {
      this._orthoH = diag
      const a = this._aspect()
      this.camera.left = (-diag * a) / 2
      this.camera.right = (diag * a) / 2
      this.camera.top = diag / 2
      this.camera.bottom = -diag / 2
      this.camera.zoom = 1
      this.camera.position.copy(center).addScaledVector(dir, diag)
    } else {
      this.camera.position.copy(center).addScaledVector(dir, diag * 0.9)
      this.camera.near = diag * 0.001
      this.camera.far = diag * 20
    }
    this.camera.updateProjectionMatrix()
    this.controls.target.copy(center)
    this.controls.update()
  }

  // --- recenter ----------------------------------------------------------------

  // Recenter also snaps back to the initial 3/4 viewing angle (preserveDir = false).
  recenterWorld() {
    if (this.world) this._frameCamera(this.world, false)
  }

  recenterFederate(name) {
    const b = this._federateBounds(name)
    if (b) this._frameCamera(this._expand(b, 0.12), false)
    else this.recenterWorld()
  }

  // AABB over the positions of the aircraft this federate owns (its "local world").
  _federateBounds(name) {
    if (!this.timeline) return null
    const min = [Infinity, Infinity, Infinity]
    const max = [-Infinity, -Infinity, -Infinity]
    let any = false
    for (const tr of this.timeline.tracks.values()) {
      if (tr.federate !== name) continue
      for (const p of tr.pos) {
        any = true
        for (let i = 0; i < 3; i++) {
          if (p[i] < min[i]) min[i] = p[i]
          if (p[i] > max[i]) max[i] = p[i]
        }
      }
    }
    return any ? { min, max } : null
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
    if (this.worldspace) this.worldspace.updateWalls(this.camera)
    if (this.showUnits) this._updateLabels(this.camera)
    this.renderer.render(this.scene, this.camera)
    this.labelRenderer.render(this.scene, this.camera)
    this._renderGizmo()
    if (this.onTime) {
      this.onTime(this.currentTime, this.playing, this.timeline ? this.timeline.duration : 0)
    }
    this._raf = requestAnimationFrame(this._loop)
  }

  _renderGizmo() {
    if (!this.gizmoVisible || !this.gizmo) return
    const size = this.renderer.getSize(this._tmpSize)
    const gs = Math.max(64, Math.min(120, size.x * 0.16))
    const m = 10
    const x = size.x - gs - m
    const y = size.y - gs - m
    this.gizmo.update(this.camera)
    this.renderer.autoClear = false
    this.renderer.setScissorTest(true)
    this.renderer.setViewport(x, y, gs, gs)
    this.renderer.setScissor(x, y, gs, gs)
    this.renderer.clearDepth()
    this.renderer.render(this.gizmo.scene, this.gizmo.camera)
    this.renderer.setScissorTest(false)
    this.renderer.setViewport(0, 0, size.x, size.y)
    this.renderer.autoClear = true
  }

  _onResize() {
    const parent = this.canvas.parentElement
    const w = parent.clientWidth
    const h = parent.clientHeight
    if (w === 0 || h === 0) return
    this.renderer.setSize(w, h, false)
    this.labelRenderer.setSize(w, h)
    const aspect = w / h
    if (this.camera.isOrthographicCamera) {
      const oh = this._orthoH || this._worldDiag() || 1000
      this.camera.left = (-oh * aspect) / 2
      this.camera.right = (oh * aspect) / 2
      this.camera.top = oh / 2
      this.camera.bottom = -oh / 2
    } else {
      this.camera.aspect = aspect
    }
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
  _disposeScene(scene) {
    scene.traverse((o) => {
      o.geometry?.dispose?.()
      o.material?.map?.dispose?.()
      o.material?.dispose?.()
    })
  }
}

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
