import * as THREE from 'three'
import { OrbitControls } from 'three/addons/controls/OrbitControls.js'
import { CSS2DRenderer, CSS2DObject } from 'three/addons/renderers/CSS2DRenderer.js'
import { LineSegments2 } from 'three/addons/lines/LineSegments2.js'
import { LineSegmentsGeometry } from 'three/addons/lines/LineSegmentsGeometry.js'
import { LineMaterial } from 'three/addons/lines/LineMaterial.js'
import { makeAircraftMesh } from './aircraftMesh.js'
import { buildWorldspace, buildWorldWireframe, niceStep, ticksFor, formatTick } from './worldspace.js'
import { buildGizmo } from './gizmo.js'
import { playback } from './clock.js'
import { DEFAULT_AIRCRAFT_SIZE, THEMES, mix } from '../config.js'

// SceneController -- one 3D viewport. Time comes from the shared playback clock; display
// state (per-aircraft mode, per-federate visibility/size/highlight/halo) and a per-pane
// VIEW FILTER (which federates this pane shows) are applied from the Vue layer. Multiple
// instances coexist (one per pane) and all read the same clock, so they stay in sync.
// Z-up world, X-red/Y-green/Z-blue.

const HIGHLIGHT_SCALE = 1.7
const MIN_LABEL_PX = 22 // min on-screen spacing between tick units before we thin them out
const PARTITION_TINT = 0.15 // how far a partition's shaded walls shift toward its owner hue
const HIGHLIGHT_LW = 2 // px line width for the (fat) highlight edges -- ~1px over the default

export class SceneController {
  constructor(canvas) {
    this.canvas = canvas
    this._raf = null

    this.theme = 'light'
    this.palette = THEMES.light
    this.projection = 'perspective'

    this.timeline = null
    this.world = null
    this.viewFilter = null // null = all federates; else array of federate names

    // display state
    this.aircraftMode = new Map()
    this.fedVisible = new Map()
    this.fedSize = new Map()
    this.fedHighlight = new Map()
    this.fedHalo = new Map()
    this.fedOf = new Map()
    this._fedBoxes = new Map()
    this.showUnits = true
    this.sizeMultiplier = 1 // global master multiplier over every federate's marker size
    this.trackId = null // aircraft id the camera is following (null = free)

    // partition geometry (from the controller meta)
    this.sectors = [] // [{id, owner, min, max}]
    this.sectorOf = new Map() // owner name -> {min,max}
    this.showSectors = false // the global "reveal every slab" toggle (off = clean default)
    this.showWorldFrame = true // partition view: draw the quiet whole-world wireframe
    this.showTint = true // owner-hue tinting of slab walls + reveal faces
    this._sole = null // the sole federate this pane shows (partition view), else null
    this._worldEdges = null // the world wireframe's edge frame group (toggled by showWorldFrame)
    this._wireframeUpdate = null // per-frame near/far edge update for the world wireframe
    this._labelBounds = null // bounds the tick units attach to (slab in a partition view, else world)
    this._fatMaterials = [] // LineMaterials needing a pixel-resolution uniform on resize

    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true })
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))

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
    this.sectorGroup = new THREE.Group() // owner-colored partition boxes
    this.overlayGroup = new THREE.Group()
    this.fleetGroup = new THREE.Group()
    this.labelGroup = new THREE.Group()
    this.scene.add(this.worldGroup, this.sectorGroup, this.overlayGroup, this.fleetGroup, this.labelGroup)
    this.meshes = new Map()
    this.worldspace = null
    this._labels = []

    this.gizmo = buildGizmo(this.palette)
    this.gizmoVisible = true

    this._tmpSize = new THREE.Vector2()
    this._onResize = this._onResize.bind(this)
    this._resizeObserver = new ResizeObserver(this._onResize)
    this._resizeObserver.observe(canvas.parentElement)
    this._onResize()

    // Click the corner gizmo to snap the view down that axis. Capture phase + stopPropagation so
    // a gizmo hit doesn't also start an OrbitControls drag.
    this._ray = new THREE.Raycaster()
    this._onPointerDown = this._onPointerDown.bind(this)
    this.renderer.domElement.addEventListener('pointerdown', this._onPointerDown, true)

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
    this.controls.mouseButtons = { LEFT: THREE.MOUSE.PAN, MIDDLE: THREE.MOUSE.ROTATE, RIGHT: THREE.MOUSE.PAN }
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

    this._clearGroup(this.fleetGroup)
    this.meshes.clear()

    // partition geometry: keep the sectors and index them by owner
    this.sectors = timeline.sectors || []
    this.sectorOf.clear()
    for (const s of this.sectors) this.sectorOf.set(s.owner, { min: s.min, max: s.max })

    // World box: prefer the controller's authoritative bounds (drawn FLUSH, so the sectors,
    // halos and 0-edges line up); fall back to padded data bounds when a run has no controller
    // meta (the old flat files).
    this.world = timeline.world ? { min: [...timeline.world.min], max: [...timeline.world.max] } : this._worldBounds(timeline.bounds)

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

    this._buildWorldGeometry()
    this._refreshBoxes()
    this._refreshFleet()
    this._frameForMode()
    this._applyTime(playback.t)
  }

  // --- applied from Vue --------------------------------------------------------

  setViewFilter(filter) {
    // null (or undefined) = all federates; an array = only those names (empty = none). A pane
    // filtered to exactly one federate (that owns a slab) is a PARTITION view; anything else is
    // the standard/fused view. The distinction drives the whole render, so rebuild geometry.
    const prevSole = this._sole
    this.viewFilter = Array.isArray(filter) ? filter : null
    this._buildWorldGeometry()
    this._refreshBoxes()
    this._refreshFleet()
    if (this.world && this._sole !== prevSole) this._frameForMode()
  }

  applyAircraftModes(modes) {
    for (const [id, mode] of Object.entries(modes || {})) this.aircraftMode.set(Number(id), mode)
    this._refreshFleet()
  }

  applyFederateStates(states) {
    for (const [name, s] of Object.entries(states || {})) {
      this.fedVisible.set(name, s.visible !== false)
      if (s.size != null) this.fedSize.set(name, s.size)
      this.fedHighlight.set(name, !!s.highlight)
      this.fedHalo.set(name, !!s.halo)
    }
    this._refreshBoxes()
    this._refreshFleet()
  }

  setTheme(theme) {
    this.theme = theme
    this.palette = THEMES[theme] || THEMES.light
    this.scene.background = new THREE.Color(this.palette.background)
    if (this.world) {
      this._buildWorldGeometry()
      this._refreshBoxes()
    }
    this._disposeScene(this.gizmo.scene)
    this.gizmo = buildGizmo(this.palette)
  }

  setGizmoVisible(on) {
    this.gizmoVisible = on
  }

  setUnitsVisible(on) {
    this.showUnits = on
    this.labelGroup.visible = on
    for (const l of this._labels) l.obj.visible = on
  }

  setSizeMultiplier(m) {
    this.sizeMultiplier = m || 1
    this._refreshFleet()
  }

  // Follow one aircraft: null frees the camera; a new id zooms in on it once, then _loop keeps
  // the camera translating with it. Ignored in a pane that doesn't show that aircraft.
  setTrack(id) {
    const next = id == null ? null : Number(id)
    const changed = next !== this.trackId
    this.trackId = next
    if (next != null && changed && this._canTrack(next)) this._zoomToAircraft(next)
  }

  setSectorsVisible(on) {
    // The global reveal: light every slab's edges + tint its faces (the fused-view equivalent
    // of turning on highlight for all federates). Rebuild, but don't move the camera.
    this.showSectors = on
    this._buildWorldGeometry()
    this._refreshBoxes()
  }

  setWorldFrameVisible(on) {
    // Cheap toggle of the partition view's whole-world wireframe (axes stay).
    this.showWorldFrame = on
    if (this._worldEdges) this._worldEdges.visible = on
  }

  setTintVisible(on) {
    // Owner-hue tinting is baked into wall/face materials, so rebuild to apply.
    this.showTint = on
    this._buildWorldGeometry()
    this._refreshBoxes()
  }

  recenterWorld() {
    if (this.world) this._frameCamera(this.world, false)
  }

  recenterFederate(name) {
    const b = this._federateBounds(name)
    if (b) this._frameCamera(this._expand(b, 0.12), false)
    else this.recenterWorld()
  }

  _canTrack(id) {
    const mesh = this.meshes.get(id)
    if (!mesh) return false
    const fed = this.fedOf.get(id)
    return this._inFilter(fed) && this.fedVisible.get(fed) !== false
  }

  _zoomToAircraft(id) {
    const mesh = this.meshes.get(id)
    if (!mesh) return
    const p = mesh.position
    const markerLen = (this.fedSize.get(this.fedOf.get(id)) || DEFAULT_AIRCRAFT_SIZE) * this.sizeMultiplier
    const dist = Math.max(markerLen * 8, 30)
    const dir = this.camera.position.clone().sub(this.controls.target)
    if (dir.lengthSq() < 1e-9) dir.set(1, -1, 0.7)
    dir.normalize()
    this.controls.target.copy(p)
    this.camera.position.copy(p).addScaledVector(dir, dist)
    if (!this.camera.isOrthographicCamera) {
      this.camera.near = Math.max(dist * 0.001, 0.01)
      this.camera.far = Math.max(dist * 20, this._worldDiag() * 2)
    }
    this.camera.updateProjectionMatrix()
    this.controls.update()
  }

  // Translate the camera + target by the tracked aircraft's per-frame motion (follow without
  // changing the relative view). Runs before controls.update() in the loop.
  _followTrack() {
    if (this.trackId == null || !this._canTrack(this.trackId)) return
    const p = this.meshes.get(this.trackId).position
    const t = this.controls.target
    this.camera.position.x += p.x - t.x
    this.camera.position.y += p.y - t.y
    this.camera.position.z += p.z - t.z
    t.set(p.x, p.y, p.z)
  }

  // --- internals ---------------------------------------------------------------

  _inFilter(fed) {
    return !this.viewFilter || this.viewFilter.includes(fed)
  }

  _refreshFleet() {
    for (const [id, mesh] of this.meshes) {
      const fed = this.fedOf.get(id)
      const mode = this.aircraftMode.get(id) || 'show'
      const fedOn = this.fedVisible.get(fed) !== false
      const highlight = mode === 'highlight'
      mesh.visible = fedOn && mode !== 'hide' && this._inFilter(fed)
      const size = this.fedSize.get(fed) || DEFAULT_AIRCRAFT_SIZE
      mesh.scale.setScalar((size / 2) * this.sizeMultiplier * (highlight ? HIGHLIGHT_SCALE : 1))
      mesh.material.emissive.setHex(highlight ? mesh.material.color.getHex() : 0x000000)
      mesh.material.emissiveIntensity = highlight ? 0.55 : 0
    }
  }

  // --- world / partition geometry ---------------------------------------------

  // The sole federate this pane shows (partition view), or null (standard/fused view).
  _soleFederate() {
    if (Array.isArray(this.viewFilter) && this.viewFilter.length === 1) {
      const name = this.viewFilter[0]
      if (this.sectorOf.has(name)) return name
    }
    return null
  }

  // Rebuild world furniture + partition reveals + per-federate overlay boxes. Chooses the
  // treatment from the pane's filter: a single-federate pane draws the world as a wireframe
  // with only that federate's tinted, gridded slab; a fused pane draws the full gridded world.
  _buildWorldGeometry() {
    this._clearGroup(this.worldGroup)
    this._clearGroup(this.sectorGroup)
    this._clearGroup(this.overlayGroup)
    this._fedBoxes.clear()
    this._fatMaterials = []
    this.worldspace = null
    if (!this.world) {
      this._sole = null
      return
    }

    const sole = this._soleFederate()
    this._sole = sole
    this._worldEdges = null
    this._wireframeUpdate = null

    if (sole) {
      // partition view: quiet world wireframe (edges + axes) + this slab as the lit, tinted room.
      // Axes stay in the wireframe group; only its edge frame is toggled by showWorldFrame.
      const wf = buildWorldWireframe(this.world, this.palette, { axes: true })
      wf.frameGroup.visible = this.showWorldFrame
      this._worldEdges = wf.frameGroup
      this._wireframeUpdate = wf.update
      this.worldGroup.add(wf.group)
      const slab = this.sectorOf.get(sole)
      const owner = this._fedColor(sole)
      const wallColor = this.showTint ? mix(this.palette.walls, owner, PARTITION_TINT) : this.palette.walls
      // the slab's outer edges in the OWNER color, so they separate from the grey world wireframe
      this.worldspace = buildWorldspace(slab, this.palette, { axes: false, wallColor, edgeColor: owner })
      this.worldGroup.add(this.worldspace.group)
      this._labelBounds = slab
      if (this.showSectors) for (const s of this.sectors) if (s.owner !== sole) this._addSectorReveal(s)
    } else {
      // standard/fused view: the whole world as the gridded, shaded room
      this.worldspace = buildWorldspace(this.world, this.palette, { axes: true })
      this.worldGroup.add(this.worldspace.group)
      this._labelBounds = this.world
      if (this.showSectors) for (const s of this.sectors) this._addSectorReveal(s)
    }

    this._buildOverlays()
    this._buildLabels()
    this._applyResolution()
  }

  // A revealed slab: fat owner-colored edges + a faint owner-tinted volume. Used by the global
  // "Sectors" reveal (and, in a partition view, for the OTHER slabs).
  _addSectorReveal(sec) {
    const color = this._fedColor(sec.owner)
    const { seg, mat } = fatBox(sec, color, HIGHLIGHT_LW, 0.8)
    this.sectorGroup.add(seg)
    this._fatMaterials.push(mat)
    if (this.showTint) this.sectorGroup.add(faceTint(sec, color, 0.1))
  }

  // Per-federate highlight (fat solid edges) + halo (fine dashes), one per federate, hidden
  // until toggled. Filter-INDEPENDENT: you can light up another partition from inside a pane.
  _buildOverlays() {
    if (!this.timeline) return
    for (const f of this.timeline.federates) {
      const color = this._fedColor(f.name)
      const b = this.sectorOf.get(f.name) || this.world
      const { seg: box, mat } = fatBox(b, color, HIGHLIGHT_LW, 0.95)
      this._fatMaterials.push(mat)
      // HALO — ON HOLD (viewer-generated placeholder). Per the user, the halo region is SOURCE
      // DATA the federates should define in meta (expected once near-seam collision lands); it
      // must not be synthesized here. Until the meta carries it, this is a uniform-gap stand-in
      // (constant absolute margin on every face; the old per-axis % ballooned the long-edge ends).
      const haloMargin = this._worldMinExtent() * 0.06
      const halo = dashedBox(this._expandBy(b, haloMargin), color, this._worldMaxExtent() * 0.008, 0.62)
      box.visible = false
      halo.visible = false
      this.overlayGroup.add(box, halo)
      this._fedBoxes.set(f.name, { box, halo })
    }
  }

  _refreshBoxes() {
    for (const [name, b] of this._fedBoxes) {
      b.box.visible = !!this.fedHighlight.get(name)
      b.halo.visible = !!this.fedHalo.get(name)
    }
  }

  _applyResolution() {
    const s = this.renderer.getSize(this._tmpSize)
    for (const m of this._fatMaterials) m.resolution.set(s.x, s.y)
  }

  _frameForMode() {
    if (!this.world) return
    if (this._sole) this._frameCamera(this._expand(this.sectorOf.get(this._sole), 0.12))
    else this._frameCamera(this.world)
  }

  _fedColor(name) {
    const f = this.timeline?.federates.find((f) => f.name === name)
    return f ? f.color : 0x333333
  }

  dispose() {
    if (this._raf) cancelAnimationFrame(this._raf)
    this._resizeObserver.disconnect()
    this.renderer.domElement.removeEventListener('pointerdown', this._onPointerDown, true)
    this.controls.dispose()
    this._clearGroup(this.worldGroup)
    this._clearGroup(this.sectorGroup)
    this._clearGroup(this.overlayGroup)
    this._clearGroup(this.fleetGroup)
    for (const l of this._labels) l.el.remove()
    this.labelRenderer.domElement.remove()
    if (this.gizmo) this._disposeScene(this.gizmo.scene)
    this.renderer.dispose()
  }

  // --- labels ------------------------------------------------------------------

  _buildLabels() {
    for (const l of this._labels) l.el.remove()
    this._labels.length = 0
    this._clearGroup(this.labelGroup)
    const b = this._labelBounds || this.world
    if (!b) return
    for (let axis = 0; axis < 3; axis++) {
      const step = niceStep(b.max[axis] - b.min[axis])
      for (const v of ticksFor(b.min[axis], b.max[axis], step)) {
        if (Math.abs(v) < step * 1e-6) continue
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

  _updateLabels(camera) {
    const b = this._labelBounds || this.world
    if (!b || this._labels.length === 0) return
    const c = [(b.min[0] + b.max[0]) / 2, (b.min[1] + b.max[1]) / 2, (b.min[2] + b.max[2]) / 2]
    const cam = camera.position
    const ext = Math.max(b.max[0] - b.min[0], b.max[1] - b.min[1], b.max[2] - b.min[2])
    const off = ext * 0.004 // sit close to the edge (small, since flush bounds have no margin)
    const val = (ax, side) => (side ? b.max[ax] : b.min[ax])
    // Isometric (orthographic) visibility uses the parallel view direction, not the camera point.
    const iso = camera.isOrthographicCamera
    const dir = iso ? camera.getWorldDirection(new THREE.Vector3()) : null
    const wallVisible = (ax, side) => {
      const n = side ? 1 : -1
      if (iso) return n * dir.getComponent(ax) > 0 // interior visible = outward normal along view dir
      return n * (cam.getComponent(ax) - val(ax, side)) < 0
    }
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
      if (!best) best = { B, C, sB: cam.getComponent(B) > c[B] ? 1 : 0, sC: cam.getComponent(C) > c[C] ? 1 : 0 }
      return best
    }
    const edges = [pickEdge(0), pickEdge(1), pickEdge(2)]
    for (const l of this._labels) {
      const e = edges[l.axis]
      const coord = [0, 0, 0]
      coord[l.axis] = l.value
      coord[e.B] = val(e.B, e.sB) + (e.sB ? off : -off)
      coord[e.C] = val(e.C, e.sC) + (e.sC ? off : -off)
      l.obj.position.set(coord[0], coord[1], coord[2])
    }

    // Declutter: per axis, project ticks to the screen and hide any that fall within
    // MIN_LABEL_PX of the last kept one, so a zoomed-out view keeps only spaced indicators.
    const sz = this.renderer.getSize(this._tmpSize)
    const pv = this._tmpProj || (this._tmpProj = new THREE.Vector3())
    const perAxis = [[], [], []]
    for (const l of this._labels) perAxis[l.axis].push(l)
    for (const arr of perAxis) {
      arr.sort((a, b) => a.value - b.value)
      let lx = -1e9
      let ly = -1e9
      for (const l of arr) {
        pv.set(l.obj.position.x, l.obj.position.y, l.obj.position.z).project(camera)
        const onScreen = pv.z < 1 && Math.abs(pv.x) <= 1.15 && Math.abs(pv.y) <= 1.15
        const sx = (pv.x * 0.5 + 0.5) * sz.x
        const sy = (1 - (pv.y * 0.5 + 0.5)) * sz.y
        const spaced = Math.hypot(sx - lx, sy - ly) >= MIN_LABEL_PX
        l.obj.visible = onScreen && spaced
        if (l.obj.visible) {
          lx = sx
          ly = sy
        }
      }
    }
  }

  // --- geometry ----------------------------------------------------------------

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
  // Per-axis fractional padding -- fine for camera framing, but NOT for the halo (it makes the
  // long axis's margin dwarf the short axis's). Kept for framing only.
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
  // Constant absolute margin on every face -- a uniform gap from the bounds (used by the halo).
  _expandBy(b, m) {
    return {
      min: [b.min[0] - m, b.min[1] - m, b.min[2] - m],
      max: [b.max[0] + m, b.max[1] + m, b.max[2] + m],
    }
  }
  _worldMaxExtent() {
    const b = this.world
    return Math.max(b.max[0] - b.min[0], b.max[1] - b.min[1], b.max[2] - b.min[2])
  }
  _worldMinExtent() {
    const b = this.world
    return Math.min(b.max[0] - b.min[0], b.max[1] - b.min[1], b.max[2] - b.min[2])
  }
  _worldDiag() {
    const b = this.world
    return Math.hypot(b.max[0] - b.min[0], b.max[1] - b.min[1], b.max[2] - b.min[2])
  }
  _aspect() {
    const p = this.canvas.parentElement
    return p.clientHeight ? p.clientWidth / p.clientHeight : 1
  }
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

  _applyTime(t) {
    if (!this.timeline) return
    for (const s of this.timeline.sample(t)) {
      const mesh = this.meshes.get(s.id)
      if (!mesh) continue
      mesh.position.set(s.pos[0], s.pos[1], s.pos[2])
      mesh.quaternion.set(s.quat[0], s.quat[1], s.quat[2], s.quat[3]).normalize()
    }
  }

  _loop() {
    if (this.timeline) this._applyTime(playback.t)
    this._followTrack()
    this.controls.update()
    if (this.worldspace) this.worldspace.updateWalls(this.camera)
    if (this._wireframeUpdate) this._wireframeUpdate(this.camera)
    if (this.showUnits) this._updateLabels(this.camera)
    this.renderer.render(this.scene, this.camera)
    this.labelRenderer.render(this.scene, this.camera)
    this._renderGizmo()
    this._raf = requestAnimationFrame(this._loop)
  }

  // Gizmo viewport rectangle in the renderer's pixel space (bottom-left origin), plus the full
  // canvas size. Shared by the gizmo render and the click hit-test so they stay in lockstep.
  _gizmoRegion() {
    const size = this.renderer.getSize(this._tmpSize)
    const gs = Math.max(56, Math.min(110, size.x * 0.16))
    const m = 8
    return { x: size.x - gs - m, y: size.y - gs - m, gs, w: size.x, h: size.y }
  }

  _onPointerDown(e) {
    if (!this.gizmoVisible || !this.gizmo) return
    const rect = this.renderer.domElement.getBoundingClientRect()
    const r = this._gizmoRegion()
    const px = e.clientX - rect.left
    const pyBottom = r.h - (e.clientY - rect.top) // WebGL viewport is bottom-left origin
    if (px < r.x || px > r.x + r.gs || pyBottom < r.y || pyBottom > r.y + r.gs) return
    const u = ((px - r.x) / r.gs) * 2 - 1
    const v = ((pyBottom - r.y) / r.gs) * 2 - 1
    this._ray.setFromCamera({ x: u, y: v }, this.gizmo.camera)
    const hit = this._ray.intersectObjects(this.gizmo.hits, false)[0]
    if (hit) {
      e.preventDefault()
      e.stopPropagation() // don't let this pointerdown also start an orbit drag
      this._snapToAxis(hit.object.userData.axis)
    }
  }

  // Snap the camera to look down the +axis, keeping the current target and distance. The other
  // two axes come out orthogonal on screen (Blender-style). Z uses +Y up (top-down).
  _snapToAxis(axis) {
    const target = this.controls.target
    const dist = this.camera.position.distanceTo(target) || this._worldDiag() || 1
    this.camera.up.set(0, 0, 1)
    if (axis === 2) this.camera.up.set(0, 1, 0)
    const dir = new THREE.Vector3()
    dir.setComponent(axis, 1)
    this.camera.position.copy(target).addScaledVector(dir, dist)
    this.camera.updateProjectionMatrix()
    this.controls.update()
  }

  _renderGizmo() {
    if (!this.gizmoVisible || !this.gizmo) return
    const r = this._gizmoRegion()
    this.gizmo.update(this.camera)
    this.renderer.autoClear = false
    this.renderer.setScissorTest(true)
    this.renderer.setViewport(r.x, r.y, r.gs, r.gs)
    this.renderer.setScissor(r.x, r.y, r.gs, r.gs)
    this.renderer.clearDepth()
    this.renderer.render(this.gizmo.scene, this.gizmo.camera)
    this.renderer.setScissorTest(false)
    this.renderer.setViewport(0, 0, r.w, r.h)
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
    this._applyResolution() // fat-line materials need the pixel size to render at a stable width
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

function boxCenter(bounds) {
  return [
    (bounds.min[0] + bounds.max[0]) / 2,
    (bounds.min[1] + bounds.max[1]) / 2,
    (bounds.min[2] + bounds.max[2]) / 2,
  ]
}

// A fat (pixel-width) owner-colored wire box. Uses LineSegments2 because WebGL ignores
// linewidth on ordinary lines; the caller must keep mat.resolution set to the pixel size.
// Returns { seg, mat } so the material can be tracked for resolution updates.
function fatBox(bounds, color, linewidth, opacity) {
  const size = [bounds.max[0] - bounds.min[0], bounds.max[1] - bounds.min[1], bounds.max[2] - bounds.min[2]]
  const edges = new THREE.EdgesGeometry(new THREE.BoxGeometry(...size))
  const geo = new LineSegmentsGeometry().fromEdgesGeometry(edges)
  edges.dispose()
  const mat = new LineMaterial({ color, linewidth, transparent: true, opacity, depthWrite: false })
  const seg = new LineSegments2(geo, mat)
  const c = boxCenter(bounds)
  seg.position.set(c[0], c[1], c[2])
  return { seg, mat }
}

// A faint owner-tinted volume (interior faces) for a revealed slab -- the "hue on the faces".
function faceTint(bounds, color, opacity) {
  const size = [bounds.max[0] - bounds.min[0], bounds.max[1] - bounds.min[1], bounds.max[2] - bounds.min[2]]
  const mesh = new THREE.Mesh(
    new THREE.BoxGeometry(...size),
    new THREE.MeshBasicMaterial({ color, transparent: true, opacity, side: THREE.BackSide, depthWrite: false }),
  )
  const c = boxCenter(bounds)
  mesh.position.set(c[0], c[1], c[2])
  mesh.renderOrder = -5
  return mesh
}

function dashedBox(bounds, color, dash, opacity) {
  const size = [bounds.max[0] - bounds.min[0], bounds.max[1] - bounds.min[1], bounds.max[2] - bounds.min[2]]
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
