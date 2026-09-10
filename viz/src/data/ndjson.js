// Parse one NDJSON file (newline-delimited JSON) into frames.
//
// Two line shapes are understood:
//   - a META line   {"meta":{...}}   (optional, at most one; not emitted yet)
//   - a FRAME line  {"t":.., "aircraft":[{id,pos,vel,quat}, ...]}
// Malformed / blank lines are skipped rather than throwing, so a truncated last line
// (a crash mid-write) costs at most that one frame -- the NDJSON crash-safety property.
export function parseNdjson(text) {
  let meta = null
  const frames = []
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim()
    if (!line) continue
    let obj
    try {
      obj = JSON.parse(line)
    } catch {
      continue // skip a partial/corrupt line
    }
    if (obj.meta) meta = obj.meta
    else if (obj.t !== undefined) frames.push(obj)
  }
  return { meta, frames }
}
