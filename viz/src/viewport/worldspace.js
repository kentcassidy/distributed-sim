import * as THREE from 'three'
import { PALETTE, hexToCss } from '../config.js'

// The worldspace furniture: the volumetric room, the arrowed RGB axes with tick
// numbers, and three flat grids that live on the FAR walls and follow the camera
// (matplotlib/plotly-style). buildWorldspace() returns a group plus an update(camera)
// that re-parks the wall grids as you orbit.
//
// Everything is Z-up and in world units (metres).

export function buildWorldspace(bounds) {
  const group = new THREE.Group()
  const size = [
    bounds.max[0] - bounds.min[0],
    bounds.max[1] - bounds.min[1],
    bounds.max[2] - bounds.min[2],
  ]
  const center = [
    (bounds.min[0] + bounds.max[0]) / 2,
    (bounds.min[1] + bounds.max[1]) / 2,
    (bounds.min[2] + bounds.max[2]) / 2,
  ]
  const maxExt = Math.max(...size)

  // --- room: BackSide box, only far walls render (they never occlude) ---
  const roomGeom = new THREE.BoxGeometry(size[0], size[1], size[2])
  const room = new THREE.Mesh(
    roomGeom,
    new THREE.MeshBasicMaterial({ color: PALETTE.walls, side: THREE.BackSide }),
  )
  room.position.set(...center)
  group.add(room)

  // --- three wall grids (one per plane), parked on the far wall each frame ---
  const wallXY = buildWallGrid(bounds, 0, 1, 2) // spans X,Y; normal = Z
  const wallXZ = buildWallGrid(bounds, 0, 2, 1) // spans X,Z; normal = Y
  const wallYZ = buildWallGrid(bounds, 1, 2, 0) // spans Y,Z; normal = X
  group.add(wallXY.obj, wallXZ.obj, wallYZ.obj)

  // --- axes: rods through the origin, arrowheads at the + tips, letters ---
  const r = maxExt * 0.0016
  addAxis(group, bounds, 0, PALETTE.axisX, r, 'X')
  addAxis(group, bounds, 1, PALETTE.axisY, r, 'Y')
  addAxis(group, bounds, 2, PALETTE.axisZ, r, 'Z')

  // --- tick numbers along each axis (major ticks) ---
  const pad = maxExt * 0.02
  addTicks(group, bounds, 0, [0, -pad, 0]) // X ticks, nudged in -Y
  addTicks(group, bounds, 1, [-pad, 0, 0]) // Y ticks, nudged in -X
  addTicks(group, bounds, 2, [-pad, -pad, 0]) // Z ticks, nudged off the vertical

  const eps = maxExt * 0.0015 // inset so grids don't z-fight the walls
  function update(camera) {
    const p = camera.position
    wallXY.setN(p.z > center[2] ? bounds.min[2] + eps : bounds.max[2] - eps)
    wallXZ.setN(p.y > center[1] ? bounds.min[1] + eps : bounds.max[1] - eps)
    wallYZ.setN(p.x > center[0] ? bounds.min[0] + eps : bounds.max[0] - eps)
  }

  return { group, update }
}

// --- wall grid ---------------------------------------------------------------

// aU, aV = the two in-plane world-axis indices; aN = the perpendicular (normal) axis.
// Built at N=0 locally; setN() slides the whole grid to the chosen wall.
function buildWallGrid(bounds, aU, aV, aN) {
  const uMin = bounds.min[aU]
  const uMax = bounds.max[aU]
  const vMin = bounds.min[aV]
  const vMax = bounds.max[aV]
  const majU = niceStep(uMax - uMin)
  const majV = niceStep(vMax - vMin)

  const minorPts = []
  const majorPts = []
  const P = (u, v) => {
    const c = [0, 0, 0]
    c[aU] = u
    c[aV] = v
    return c
  }
  const lineU = (u, arr) => arr.push(...P(u, vMin), ...P(u, vMax))
  const lineV = (v, arr) => arr.push(...P(uMin, v), ...P(uMax, v))

  forEachTick(uMin, uMax, majU / 5, (u) => lineU(u, minorPts))
  forEachTick(vMin, vMax, majV / 5, (v) => lineV(v, minorPts))
  forEachTick(uMin, uMax, majU, (u) => lineU(u, majorPts))
  forEachTick(vMin, vMax, majV, (v) => lineV(v, majorPts))

  const obj = new THREE.Group()
  obj.add(lineSegments(minorPts, PALETTE.gridMinor))
  obj.add(lineSegments(majorPts, PALETTE.gridMajor))

  return {
    obj,
    setN(n) {
      obj.position.setComponent(aN, n)
    },
  }
}

function lineSegments(points, color) {
  const geom = new THREE.BufferGeometry()
  geom.setAttribute('position', new THREE.Float32BufferAttribute(points, 3))
  return new THREE.LineSegments(geom, new THREE.LineBasicMaterial({ color }))
}

// --- axes --------------------------------------------------------------------

function addAxis(group, bounds, axis, color, radius, letter) {
  const from = [0, 0, 0]
  const to = [0, 0, 0]
  from[axis] = bounds.min[axis]
  to[axis] = bounds.max[axis]

  const a = new THREE.Vector3(...from)
  const b = new THREE.Vector3(...to)
  const dir = new THREE.Vector3().subVectors(b, a)
  const len = dir.length() || 1e-6
  dir.normalize()

  // rod
  const rod = new THREE.Mesh(
    new THREE.CylinderGeometry(radius, radius, len, 12),
    new THREE.MeshBasicMaterial({ color }),
  )
  rod.position.copy(a).add(b).multiplyScalar(0.5)
  rod.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir)
  group.add(rod)

  // arrowhead at the + tip
  const headLen = radius * 12
  const head = new THREE.Mesh(
    new THREE.ConeGeometry(radius * 3.2, headLen, 16),
    new THREE.MeshBasicMaterial({ color }),
  )
  head.position.copy(b)
  head.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir)
  group.add(head)

  // axis letter just past the tip
  const label = makeTextSprite(letter, hexToCss(color), radius * 46)
  const lp = b.clone().addScaledVector(dir, headLen * 2)
  label.position.copy(lp)
  group.add(label)
}

function addTicks(group, bounds, axis, offset) {
  const min = bounds.min[axis]
  const max = bounds.max[axis]
  const step = niceStep(max - min)
  const height = Math.max(max - min, 1) * 0.035
  forEachTick(min, max, step, (t) => {
    if (Math.abs(t) < step * 1e-6) return // skip 0 (origin clutter)
    const pos = [0, 0, 0]
    pos[axis] = t
    const spr = makeTextSprite(formatTick(t), PALETTE.tick, height)
    spr.position.set(pos[0] + offset[0], pos[1] + offset[1], pos[2] + offset[2])
    group.add(spr)
  })
}

// --- text sprites ------------------------------------------------------------

function makeTextSprite(text, cssColor, worldHeight) {
  const canvas = document.createElement('canvas')
  canvas.width = 256
  canvas.height = 128
  const ctx = canvas.getContext('2d')
  ctx.font = 'bold 72px system-ui, sans-serif'
  ctx.fillStyle = cssColor
  ctx.textAlign = 'center'
  ctx.textBaseline = 'middle'
  ctx.fillText(text, 128, 64)

  const tex = new THREE.CanvasTexture(canvas)
  tex.minFilter = THREE.LinearFilter
  const spr = new THREE.Sprite(
    new THREE.SpriteMaterial({ map: tex, transparent: true, depthTest: false }),
  )
  // canvas is 2:1, so width = 2 * height
  spr.scale.set(worldHeight * 2, worldHeight, 1)
  return spr
}

// --- tick math ---------------------------------------------------------------

function niceStep(range, target = 6) {
  const raw = (range || 1) / target
  const mag = Math.pow(10, Math.floor(Math.log10(raw)))
  const norm = raw / mag
  const step = norm < 1.5 ? 1 : norm < 3 ? 2 : norm < 7 ? 5 : 10
  return step * mag
}

function forEachTick(min, max, step, cb) {
  if (!(step > 0)) return
  const start = Math.ceil(min / step - 1e-9) * step
  for (let x = start; x <= max + 1e-9; x += step) cb(x)
}

function formatTick(v) {
  const r = Math.round(v)
  return Math.abs(r) >= 1000 ? (r / 1000).toFixed(r % 1000 ? 1 : 0) + 'k' : String(r)
}
