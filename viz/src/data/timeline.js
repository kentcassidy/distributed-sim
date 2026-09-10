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
  const tracks = new Map() // id -> {id, federate, times[], pos[], quat[]}
  const federateNames = []

  for (const src of sources) {
    if (!federateNames.includes(src.federate)) federateNames.push(src.federate)
    for (const frame of src.frames) {
      const t = frame.t
      for (const ac of frame.aircraft || []) {
        let tr = tracks.get(ac.id)
        if (!tr) {
          tr = { id: ac.id, federate: src.federate, times: [], pos: [], quat: [] }
          tracks.set(ac.id, tr)
        }
        tr.times.push(t)
        tr.pos.push(ac.pos)
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
    color: colorOf.get(tr.federate),
  }))

  return new Timeline({ tracks, aircraft, federates, tMin, tMax, bounds: { min, max } })
}

export class Timeline {
  constructor({ tracks, aircraft, federates, tMin, tMax, bounds }) {
    this.tracks = tracks
    this.aircraft = aircraft
    this.federates = federates
    this.tMin = Number.isFinite(tMin) ? tMin : 0
    this.tMax = Number.isFinite(tMax) ? tMax : 0
    this.bounds = bounds
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
      let quat
      if (t <= tr.times[0]) {
        pos = tr.pos[0]
        quat = tr.quat[0]
      } else if (t >= tr.times[n - 1]) {
        pos = tr.pos[n - 1]
        quat = tr.quat[n - 1]
      } else {
        const i = bisect(tr.times, t)
        const t0 = tr.times[i]
        const t1 = tr.times[i + 1]
        const a = (t - t0) / (t1 - t0)
        pos = lerp3(tr.pos[i], tr.pos[i + 1], a)
        quat = nlerp4(tr.quat[i], tr.quat[i + 1], a)
      }
      out.push({ id: tr.id, federate: tr.federate, pos, quat })
    }
    return out
  }
}

// --- helpers ---

function sortTrack(tr) {
  const order = tr.times.map((_, i) => i).sort((a, b) => tr.times[a] - tr.times[b])
  tr.times = order.map((i) => tr.times[i])
  tr.pos = order.map((i) => tr.pos[i])
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
