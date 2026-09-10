import * as THREE from 'three'

// The worldspace furniture: the volumetric room, thin arrowed RGB axes, and grids on
// every wall EXCEPT the one facing the viewer (the "fourth wall"), which is hidden each
// frame so the box reads as an enclosing gridded room. Tick UNITS are drawn separately
// as constant-size DOM labels (see SceneController's CSS2D layer).
//
// Z-up, world units. Gridlines use ONE global step in every dimension -> square cells.

export function buildWorldspace(bounds, palette) {
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
    new THREE.MeshBasicMaterial({ color: palette.walls, side: THREE.BackSide, depthWrite: false }),
  )
  room.position.copy(center)
  room.renderOrder = -10
  group.add(room)

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
      const normal = new THREE.Vector3()
      normal.setComponent(aN, side === 0 ? -1 : 1) // outward
      const faceCenter = center.clone()
      faceCenter.setComponent(aN, faceVal)
      walls.push({ obj, normal, faceCenter })
      group.add(obj)
    }
  }

  // axes: thin rods through the origin, small arrowheads, inset off the walls
  const r = maxExt * 0.0009
  const inset = 0.96
  addAxis(group, bounds, 0, palette.axisX, r, inset)
  addAxis(group, bounds, 1, palette.axisY, r, inset)
  addAxis(group, bounds, 2, palette.axisZ, r, inset)

  const toCam = new THREE.Vector3()
  function updateWalls(camera) {
    // Show a wall only where we're seeing its INTERIOR face -- identical to the BackSide
    // room. A wall whose OUTWARD normal points toward the camera (we'd see its outside)
    // is hidden. From any angle that leaves exactly the interior-facing walls, matching
    // the room color.
    for (const w of walls) {
      toCam.copy(camera.position).sub(w.faceCenter)
      w.obj.visible = w.normal.dot(toCam) < 0
    }
  }

  return { group, updateWalls }
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
