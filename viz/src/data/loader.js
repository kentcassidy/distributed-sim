import { parseNdjson } from './ndjson.js'
import { buildTimeline } from './timeline.js'

// Ingest sim_out, which is now organised as one FOLDER per simulation run (a partitioning
// trial). A run holds one controller.ndjson (the run-level meta: world bounds + sectors)
// plus one file per federate (its truth frames). See vite.config.js for the dev endpoints.

// Root folders that group runs ("" = runs directly under sim_out/).
export async function listRoots() {
  const res = await fetch('/sim_out/roots.json')
  if (!res.ok) throw new Error(`roots.json ${res.status}`)
  const { roots } = await res.json()
  return roots || []
}

// The runs available within a root folder -- for the run selector.
export async function listRuns(root = '') {
  const res = await fetch('/sim_out/runs.json?root=' + encodeURIComponent(root))
  if (!res.ok) throw new Error(`runs.json ${res.status}`)
  const { runs } = await res.json()
  return runs || []
}

// Path of a run within its root: "run" at top level, or "root/run" when grouped.
function runPath(root, run) {
  return (root ? encodeURIComponent(root) + '/' : '') + encodeURIComponent(run)
}

// Load ONE run into a Timeline. The file name minus ".ndjson" is the federate name
// (One.ndjson -> "One") unless the file's meta declares one; the controller file is
// recognised by its meta and contributes geometry, not tracks (handled in buildTimeline).
export async function loadRun(root, run) {
  const base = '/sim_out/' + runPath(root, run)
  const idxRes = await fetch(base + '/index.json')
  if (!idxRes.ok) throw new Error(`${root}/${run}/index.json ${idxRes.status}`)
  const { files } = await idxRes.json()
  if (!files || files.length === 0) throw new Error(`no .ndjson files in run "${run}"`)

  const sources = []
  for (const file of files) {
    const res = await fetch(base + '/' + encodeURIComponent(file))
    if (!res.ok) throw new Error(`${file} ${res.status}`)
    const { meta, frames, events } = parseNdjson(await res.text())
    const federate = (meta && meta.federate) || file.replace(/\.ndjson$/i, '')
    sources.push({ federate, frames, meta: meta || null, events, file })
  }
  return buildTimeline(sources)
}
