import { federateColor } from '../config.js'

// A Timeline merges several federate files into one queryable object:
//   - tracks:    per-aircraft time series (times, positions, quaternions)
//   - aircraft:  [{id, federate, color}]  (for the panel + mesh creation)
//   - federates: [{name, color}]
//   - bounds:    data-space AABB over all positions
//   - sample(t): interpolated state of every aircraft at time t
//
// It's framework-agnostic (no Three.js) so the same data can feed N viewports.

export function buildTimeline(sources) {
  const tracks = new Map() // id -> {id, federate, role, times[], pos[], vel[], quat[]}
  let dt = null
  let world = null // authoritative {min,max}, from the controller meta (else null)
  const sectors = [] // [{id, owner, min, max}], from the controller meta

  // The CONTROLLER source is the one whose meta declares `world` -- it carries the run
  // geometry (world bounds + owner-tagged sectors) and NO tracks. Federates never emit a
  // `world`, so this is an unambiguous test. We take its geometry and drop it from the
  // fleet (it is not an aircraft-bearing federate).
  const frameSources = []
  for (const src of sources) {
    const m = src.meta
    if (m) {
      if (dt == null && m.dt != null) dt = m.dt
      if (m.world) world = { min: [...m.world.min], max: [...m.world.max] }
      if (Array.isArray(m.sectors)) {
        for (const s of m.sectors) sectors.push({ id: s.id, owner: s.owner || src.federate, min: s.min, max: s.max })
      }
    }
    if (m && m.world) continue // controller: geometry only, contributes no tracks
    frameSources.push(src)
  }

  // Federates = those that carry frames, PLUS any sector owner (so a federate that owns an
  // empty slab still shows in the fleet + gets a partition box). Sorted for stable colors.
  const federateNames = []
  for (const src of frameSources) if (!federateNames.includes(src.federate)) federateNames.push(src.federate)
  for (const s of sectors) if (s.owner && !federateNames.includes(s.owner)) federateNames.push(s.owner)
  federateNames.sort()

  for (const src of frameSources) {
    for (const frame of src.frames) {
      const t = frame.t
      for (const ac of frame.aircraft || []) {
        const role = ac.role || 'owned'
        // The god/truth timeline is built from OWNED reports only. Ghost rows (the Live
        // experiment) are understood by the schema but rendered in the later per-federate
        // viewpoint feature, not in this shared view.
        if (role !== 'owned') continue
        let tr = tracks.get(ac.id)
        if (!tr) {
          tr = { id: ac.id, federate: src.federate, role, times: [], pos: [], vel: [], quat: [] }
          tracks.set(ac.id, tr)
        }
        tr.times.push(t)
        tr.pos.push(ac.pos)
        tr.vel.push(ac.vel || [0, 0, 0])
        tr.quat.push(ac.quat)
      }
    }
  }

  for (const tr of tracks.values()) sortTrack(tr)

  // time range + data bounds
  let tMin = Infinity
  let tMax = -Infinity
  const min = [Infinity, Infinity, Infinity]
  const max = [-Infinity, -Infinity, -Infinity]
  for (const tr of tracks.values()) {
    for (let k = 0; k < tr.times.length; k++) {
      tMin = Math.min(tMin, tr.times[k])
      tMax = Math.max(tMax, tr.times[k])
      const p = tr.pos[k]
      for (let i = 0; i < 3; i++) {
        if (p[i] < min[i]) min[i] = p[i]
        if (p[i] > max[i]) max[i] = p[i]
      }
    }
  }

  const federates = federateNames.map((name, i) => ({ name, color: federateColor(i) }))
  const colorOf = new Map(federates.map((f) => [f.name, f.color]))
  const aircraft = [...tracks.values()].map((tr) => ({
    id: tr.id,
    federate: tr.federate,
    role: tr.role,
    color: colorOf.get(tr.federate),
  }))

  return new Timeline({ tracks, aircraft, federates, tMin, tMax, bounds: { min, max }, dt, sectors, world })
}

export class Timeline {
  constructor({ tracks, aircraft, federates, tMin, tMax, bounds, dt, sectors, world }) {
    this.tracks = tracks
    this.aircraft = aircraft
    this.federates = federates
    this.tMin = Number.isFinite(tMin) ? tMin : 0
    this.tMax = Number.isFinite(tMax) ? tMax : 0
    this.bounds = bounds // data-space AABB over aircraft positions (fallback world)
    this.dt = dt ?? null
    this.sectors = sectors || [] // [{id, owner, min, max}]
    this.world = world || null // authoritative world AABB from the controller, or null
  }

  get duration() {
    return Math.max(0, this.tMax - this.tMin)
  }

  // State of every aircraft at time t (linear pos, normalized-lerp quat).
  // Clamps to the ends -- no extrapolation (this is the constructive TRUTH replay).
  sample(t) {
    const out = []
    for (const tr of this.tracks.values()) {
      const n = tr.times.length
      if (n === 0) continue
      let pos
      let vel
      let quat
      if (t <= tr.times[0]) {
        pos = tr.pos[0]
        vel = tr.vel[0]
        quat = tr.quat[0]
      } else if (t >= tr.times[n - 1]) {
        pos = tr.pos[n - 1]
        vel = tr.vel[n - 1]
        quat = tr.quat[n - 1]
      } else {
        const i = bisect(tr.times, t)
        const t0 = tr.times[i]
        const t1 = tr.times[i + 1]
        const a = (t - t0) / (t1 - t0)
        pos = lerp3(tr.pos[i], tr.pos[i + 1], a)
        vel = lerp3(tr.vel[i], tr.vel[i + 1], a)
        quat = nlerp4(tr.quat[i], tr.quat[i + 1], a)
      }
      out.push({ id: tr.id, federate: tr.federate, role: tr.role, pos, vel, quat })
    }
    return out
  }
}

// --- helpers ---

function sortTrack(tr) {
  const order = tr.times.map((_, i) => i).sort((a, b) => tr.times[a] - tr.times[b])
  tr.times = order.map((i) => tr.times[i])
  tr.pos = order.map((i) => tr.pos[i])
  tr.vel = order.map((i) => tr.vel[i])
  tr.quat = order.map((i) => tr.quat[i])
}

// last index i with times[i] <= t (assumes times[0] < t < times[last])
function bisect(times, t) {
  let lo = 0
  let hi = times.length - 1
  while (lo < hi) {
    const mid = (lo + hi + 1) >> 1
    if (times[mid] <= t) lo = mid
    else hi = mid - 1
  }
  return lo
}

function lerp3(a, b, s) {
  return [a[0] + (b[0] - a[0]) * s, a[1] + (b[1] - a[1]) * s, a[2] + (b[2] - a[2]) * s]
}

// Normalized lerp of a quaternion (x,y,z,w). For the small step-to-step rotations
// here it's visually identical to slerp and has no external dependency.
function nlerp4(a, b, s) {
  // take the shorter arc
  let dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]
  const sign = dot < 0 ? -1 : 1
  let x = a[0] + (b[0] * sign - a[0]) * s
  let y = a[1] + (b[1] * sign - a[1]) * s
  let z = a[2] + (b[2] * sign - a[2]) * s
  let w = a[3] + (b[3] * sign - a[3]) * s
  const len = Math.hypot(x, y, z, w) || 1
  return [x / len, y / len, z / len, w / len]
}
