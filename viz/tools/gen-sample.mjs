// Generate NEW-SCHEMA sample NDJSON (meta line + per-aircraft role) so the viewer's
// meta/role path can be exercised before the C++ emitter is patched. This is synthetic,
// plausible cruise data -- NOT the real integrator output -- and doubles as a concrete
// reference for the shape the federate should emit (see ../NDJSON_SCHEMA.md).
//
// Run:  node viz/tools/gen-sample.mjs
// Writes viz/sample-data/F1.ndjson and F2.ndjson. To view them, copy over sim_out/
// (they replace the current owned-truth files), or regenerate from C++ once patched.

import { mkdirSync, writeFileSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = dirname(fileURLToPath(import.meta.url))
const OUT = resolve(__dirname, '../sample-data')
mkdirSync(OUT, { recursive: true })

const DT = 0.1
const STEPS = 100
const TRIM = 200 // m/s

function file(federate, id, laneY) {
  const lines = []
  lines.push(JSON.stringify({ meta: { federate, dt: DT, sectors: [] } }))
  for (let i = 0; i < STEPS; i++) {
    const t = +(i * DT).toFixed(4)
    const x = +(TRIM * DT * (i + 1)).toFixed(6) // ~20 m per step
    const z = +(0.0053 * t * t).toFixed(6) // gentle climb, ~0.5 m by t=9.9
    const qy = +(0.02 - 0.0008 * t).toFixed(9) // small, slowly relaxing pitch
    const qw = +Math.sqrt(1 - qy * qy).toFixed(9)
    lines.push(
      JSON.stringify({
        t,
        aircraft: [
          {
            id,
            role: 'owned',
            pos: [x, laneY, z],
            vel: [TRIM, 0, +(0.0106 * t).toFixed(6)],
            quat: [0, qy, 0, qw],
          },
        ],
      }),
    )
  }
  writeFileSync(resolve(OUT, `${federate}.ndjson`), lines.join('\n') + '\n')
}

file('F1', 1, 500)
file('F2', 2, 1000)
console.log('wrote sample-data/F1.ndjson and F2.ndjson (new schema: meta + role)')
