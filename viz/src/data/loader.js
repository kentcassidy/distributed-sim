import { parseNdjson } from './ndjson.js'
import { buildTimeline } from './timeline.js'

// Auto-ingest everything in repo-root/sim_out (served by the dev plugin). The file
// name minus ".ndjson" is taken as the federate name (F1.ndjson -> "F1"), which is
// how each aircraft gets attributed to a federate for coloring + (later) viewpoints.
export async function loadSimOut() {
  const idxRes = await fetch('/sim_out/index.json')
  if (!idxRes.ok) throw new Error(`index.json ${idxRes.status}`)
  const { files } = await idxRes.json()
  if (!files || files.length === 0) throw new Error('no .ndjson files in sim_out/')

  const sources = []
  for (const file of files) {
    const res = await fetch('/sim_out/' + file)
    if (!res.ok) throw new Error(`${file} ${res.status}`)
    const { meta, frames } = parseNdjson(await res.text())
    // federate name: meta.federate if the file declares it, else the filename
    const federate = (meta && meta.federate) || file.replace(/\.ndjson$/i, '')
    sources.push({ federate, frames, meta: meta || null })
  }
  return buildTimeline(sources)
}
