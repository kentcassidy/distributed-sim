import * as THREE from 'three'

// The worldspace furniture: the volumetric room, thin arrowed RGB axes, and grids on
// every wall EXCEPT the one facing the viewer (the "fourth wall"), which is hidden each
// frame so the box reads as an enclosing gridded room. Tick UNITS are drawn separately
// as constant-size DOM labels (see SceneController's CSS2D layer).
//
// Z-up, world units. Gridlines use ONE global step in every dimension -> square cells.

// opts.axes    -- draw the RGB origin axes inside this box (default true). A sector room in
//                 a partition view passes false; the axes live on the world wireframe instead.
// opts.wallColor -- override the shaded-wall color (default palette.walls). A partition's own
//                 slab passes a color tinted toward the owning federate's hue.
export function buildWorldspace(bounds, palette, opts = {}) {
  const { axes = true, wallColor = palette.walls, edgeColor = palette.gridMajor, edgeOpacity = 0.9 } = opts
  const group = new THREE.Group()
  const size = [
    bounds.max[0] - bounds.min[0],
    bounds.max[1] - bounds.min[1],
    bounds.max[2] - bounds.min[2],
  ]
  const center = new THREE.Vector3(
    (bounds.min[0] + bounds.max[0]) / 2,
    (bounds.min[1] + bounds.max[1]) / 2,
    (bounds.min[2] + bounds.max[2]) / 2,
  )
  const maxExt = Math.max(...size)
  const major = niceStep(maxExt)
  const minor = major / 5

  // room: BackSide box. depthWrite off + a low renderOrder so it sits BEHIND every
  // aircraft -- wings near a wall draw over it instead of clipping into it.
  const room = new THREE.Mesh(
    new THREE.BoxGeometry(size[0], size[1], size[2]),
    new THREE.MeshBasicMaterial({ color: wallColor, side: THREE.BackSide, depthWrite: false }),
  )
  room.position.copy(center)
  room.renderOrder = -10
  group.add(room)

  // persistent outline: 12 limit edges ALWAYS drawn (never culled) so the box reads as a box
  // from any angle -- even where a face grid is hidden. Near edges render dashed + faint.
  const frame = buildEdgeFrame(bounds, { solidColor: edgeColor, solidOpacity: edgeOpacity, dashColor: edgeColor })
  group.add(frame.group)

  // six wall grids, one per face; the near ("fourth") wall is hidden in updateWalls()
  const walls = []
  const planes = [
    [1, 2, 0], // normal X: grid spans Y,Z
    [0, 2, 1], // normal Y: grid spans X,Z
    [0, 1, 2], // normal Z: grid spans X,Y
  ]
  for (const [aU, aV, aN] of planes) {
    for (const side of [0, 1]) {
      const faceVal = side === 0 ? bounds.min[aN] : bounds.max[aN]
      const obj = buildFaceGrid(bounds, aU, aV, aN, faceVal, palette, major, minor)
      walls.push({ obj, aN, side })
      group.add(obj)
    }
  }

  // axes: thin rods through the origin, small arrowheads, inset off the walls
  if (axes) {
    const r = maxExt * 0.0009
    const inset = 0.96
    addAxis(group, bounds, 0, palette.axisX, r, inset)
    addAxis(group, bounds, 1, palette.axisY, r, inset)
    addAxis(group, bounds, 2, palette.axisZ, r, inset)
  }

  function updateWalls(camera) {
    // Show a wall only where we're seeing its INTERIOR face; the near faces (whose interior we
    // can't see) are hidden. faceNear() is projection-aware, so isometric uses the view
    // direction rather than the camera point. The edge frame follows the same near/far test.
    for (const w of walls) w.obj.visible = !faceNear(w.aN, w.side, bounds, camera)
    frame.update(camera)
  }

  return { group, updateWalls }
}

// A box frame whose 12 edges are always drawn. The edges nearest the camera (those shared by
// two near/hidden faces) render at a lower opacity so they don't clutter what sits behind them;
// the rest are full-strength. Both solid. Projection-aware (perspective + isometric).
export function buildEdgeFrame(bounds, opts = {}) {
  const { solidColor = 0x000000, solidOpacity = 0.9, nearOpacity = 0.25 } = opts
  const group = new THREE.Group()
  const val = (ax, side) => (side ? bounds.max[ax] : bounds.min[ax])
  const edges = []
  for (let A = 0; A < 3; A++) {
    const B = (A + 1) % 3
    const C = (A + 2) % 3
    for (const sB of [0, 1]) {
      for (const sC of [0, 1]) {
        const p0 = [0, 0, 0]
        const p1 = [0, 0, 0]
        p0[A] = bounds.min[A]
        p1[A] = bounds.max[A]
        p0[B] = p1[B] = val(B, sB)
        p0[C] = p1[C] = val(C, sC)
        const pos = new Float32Array([...p0, ...p1])
        const far = new THREE.LineSegments(
          edgeGeo(pos),
          new THREE.LineBasicMaterial({ color: solidColor, transparent: true, opacity: solidOpacity, depthWrite: false }),
        )
        const near = new THREE.LineSegments(
          edgeGeo(pos),
          new THREE.LineBasicMaterial({ color: solidColor, transparent: true, opacity: nearOpacity, depthWrite: false }),
        )
        near.visible = false
        group.add(far, near)
        edges.push({ B, C, sB, sC, far, near })
      }
    }
  }
  const update = (camera) => {
    for (const e of edges) {
      const isNear = faceNear(e.B, e.sB, bounds, camera) && faceNear(e.C, e.sC, bounds, camera)
      e.far.visible = !isNear
      e.near.visible = isNear
    }
  }
  return { group, update }
}

function edgeGeo(pos) {
  const g = new THREE.BufferGeometry()
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3))
  return g
}

// Is the face at (axis, side) a NEAR face -- one whose interior we cannot see (outward normal
// points toward the viewer)? Perspective uses the camera point; orthographic uses the parallel
// view direction, which is the fix for isometric wall/edge/units visibility.
const _viewDir = new THREE.Vector3()
function faceNear(ax, side, bounds, camera) {
  const n = side ? 1 : -1 // outward normal sign along this axis
  if (camera.isOrthographicCamera) {
    camera.getWorldDirection(_viewDir) // points into the scene
    return n * _viewDir.getComponent(ax) < 0 // outward normal opposes view dir => faces viewer
  }
  const faceVal = side ? bounds.max[ax] : bounds.min[ax]
  return n * (camera.position.getComponent(ax) - faceVal) > 0 // outward normal toward camera
}

// The world reduced to a plain outline: its 12 limit edges (no wall fill, no grid) plus the
// RGB origin axes. Used in a PARTITION view, where the lit gridded room is the active slab
// and the whole world recedes to this quiet wireframe for spatial context.
export function buildWorldWireframe(bounds, palette, opts = {}) {
  const { axes = true } = opts
  const group = new THREE.Group()
  const size = [bounds.max[0] - bounds.min[0], bounds.max[1] - bounds.min[1], bounds.max[2] - bounds.min[2]]
  const frame = buildEdgeFrame(bounds, { solidColor: palette.gridMajor, solidOpacity: 0.85, dashColor: palette.gridMajor })
  group.add(frame.group)

  if (axes) {
    const r = Math.max(...size) * 0.0009
    addAxis(group, bounds, 0, palette.axisX, r, 0.96)
    addAxis(group, bounds, 1, palette.axisY, r, 0.96)
    addAxis(group, bounds, 2, palette.axisZ, r, 0.96)
  }
  // frameGroup is returned separately so callers can toggle the outer frame while keeping the
  // axes; update(camera) drives its near/far edge dashing each frame.
  return { group, frameGroup: frame.group, update: frame.update }
}

// --- grids -------------------------------------------------------------------

function buildFaceGrid(bounds, aU, aV, aN, faceVal, palette, major, minor) {
  const uMin = bounds.min[aU]
  const uMax = bounds.max[aU]
  const vMin = bounds.min[aV]
  const vMax = bounds.max[aV]

  const minorPts = []
  const majorPts = []
  const P = (u, v) => {
    const c = [0, 0, 0]
    c[aU] = u
    c[aV] = v
    c[aN] = faceVal
    return c
  }
  const lineU = (u, arr) => arr.push(...P(u, vMin), ...P(u, vMax))
  const lineV = (v, arr) => arr.push(...P(uMin, v), ...P(uMax, v))

  forEachTick(uMin, uMax, minor, (u) => lineU(u, minorPts))
  forEachTick(vMin, vMax, minor, (v) => lineV(v, minorPts))
  forEachTick(uMin, uMax, major, (u) => lineU(u, majorPts))
  forEachTick(vMin, vMax, major, (v) => lineV(v, majorPts))

  const g = new THREE.Group()
  g.add(lineSegments(minorPts, palette.gridMinor))
  g.add(lineSegments(majorPts, palette.gridMajor))
  return g
}

function lineSegments(points, color) {
  const geom = new THREE.BufferGeometry()
  geom.setAttribute('position', new THREE.Float32BufferAttribute(points, 3))
  // depthWrite off so the near walls' lines never hide the aircraft
  return new THREE.LineSegments(geom, new THREE.LineBasicMaterial({ color, depthWrite: false }))
}

// --- axes --------------------------------------------------------------------

function addAxis(group, bounds, axis, color, radius, inset) {
  const from = [0, 0, 0]
  const to = [0, 0, 0]
  from[axis] = bounds.min[axis] * inset
  to[axis] = bounds.max[axis] * inset

  const a = new THREE.Vector3(...from)
  const b = new THREE.Vector3(...to)
  const dir = new THREE.Vector3().subVectors(b, a)
  const len = dir.length() || 1e-6
  dir.normalize()

  const rod = new THREE.Mesh(
    new THREE.CylinderGeometry(radius, radius, len, 10),
    new THREE.MeshBasicMaterial({ color }),
  )
  rod.position.copy(a).add(b).multiplyScalar(0.5)
  rod.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir)
  group.add(rod)

  const headLen = radius * 8
  const head = new THREE.Mesh(
    new THREE.ConeGeometry(radius * 2.6, headLen, 14),
    new THREE.MeshBasicMaterial({ color }),
  )
  head.position.copy(b)
  head.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir)
  group.add(head)
}

// --- tick math (shared with the label layer) ---------------------------------

export function niceStep(range, target = 6) {
  const raw = (range || 1) / target
  const mag = Math.pow(10, Math.floor(Math.log10(raw)))
  const norm = raw / mag
  const step = norm < 1.5 ? 1 : norm < 3 ? 2 : norm < 7 ? 5 : 10
  return step * mag
}

export function ticksFor(min, max, step) {
  const out = []
  forEachTick(min, max, step, (t) => out.push(t))
  return out
}

export function formatTick(v) {
  const r = Math.round(v)
  return Math.abs(r) >= 1000 ? (r / 1000).toFixed(r % 1000 ? 1 : 0) + 'k' : String(r)
}

function forEachTick(min, max, step, cb) {
  if (!(step > 0)) return
  const start = Math.ceil(min / step - 1e-9) * step
  for (let x = start; x <= max + 1e-9; x += step) cb(x)
}
