import { parseNdjson } from './ndjson.js'
import { buildTimeline } from './timeline.js'

// Ingest sim_out, which is now organised as one FOLDER per simulation run (a partitioning
// trial). A run holds one controller.ndjson (the run-level meta: world bounds + sectors)
// plus one file per federate (its truth frames). See vite.config.js for the dev endpoints.

// The names of every run available (folder names, newest layout) -- for the run selector.
export async function listRuns() {
  const res = await fetch('/sim_out/index.json')
  if (!res.ok) throw new Error(`index.json ${res.status}`)
  const { runs } = await res.json()
  return runs || []
}

// Load ONE run into a Timeline. The file name minus ".ndjson" is the federate name
// (One.ndjson -> "One") unless the file's meta declares one; the controller file is
// recognised by its meta and contributes geometry, not tracks (handled in buildTimeline).
export async function loadRun(run) {
  const base = '/sim_out/' + encodeURIComponent(run)
  const idxRes = await fetch(base + '/index.json')
  if (!idxRes.ok) throw new Error(`${run}/index.json ${idxRes.status}`)
  const { files } = await idxRes.json()
  if (!files || files.length === 0) throw new Error(`no .ndjson files in run "${run}"`)

  const sources = []
  for (const file of files) {
    const res = await fetch(base + '/' + encodeURIComponent(file))
    if (!res.ok) throw new Error(`${file} ${res.status}`)
    const { meta, frames } = parseNdjson(await res.text())
    const federate = (meta && meta.federate) || file.replace(/\.ndjson$/i, '')
    sources.push({ federate, frames, meta: meta || null, file })
  }
  return buildTimeline(sources)
}

// Convenience: pick the first available run (used for the initial load).
export async function loadSimOut() {
  const runs = await listRuns()
  if (runs.length === 0) throw new Error('no runs in sim_out/')
  return loadRun(runs[0])
}
