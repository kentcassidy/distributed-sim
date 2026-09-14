import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
// The C++ federates write their NDJSON here (repo-root/sim_out). It lives OUTSIDE
// this Vue app, so we expose it through a tiny dev-only endpoint rather than moving
// files around. Drop new .ndjson in there and reload -- it auto-ingests.
const SIM_OUT = path.resolve(__dirname, '../sim_out')

// Dev middleware. A RUN is a folder holding .ndjson (federate files + controller.ndjson). Runs
// may sit directly under sim_out/ OR be grouped one level deep in a ROOT folder (a way to keep
// many runs tidy). No recursion beyond that.
//   GET /sim_out/roots.json          -> { "roots": ["", "Archive", "split_slabs"] }  ("" = top)
//   GET /sim_out/runs.json?root=R    -> { "runs": ["lots1", "lots2", ...] } within root R
//   GET /sim_out/<path>/index.json   -> { "files": [...] } for the run dir at <path> (root/run)
//   GET /sim_out/<path>/<file>       -> the raw NDJSON file
// DEV convenience only (a static build has no server); the federate never gets a server of its
// own -- it only writes files (ADR-0018).

function isRunDir(dir) {
  try {
    return fs.readdirSync(dir).some((f) => f.endsWith('.ndjson'))
  } catch {
    return false
  }
}
function hasRunChild(dir) {
  try {
    return fs.readdirSync(dir, { withFileTypes: true }).some((e) => e.isDirectory() && isRunDir(path.join(dir, e.name)))
  } catch {
    return false
  }
}
// Resolve a relative path under SIM_OUT, rejecting traversal.
function safeJoin(rel) {
  const parts = String(rel || '').split('/').filter(Boolean)
  if (parts.some((p) => p === '.' || p === '..')) return null
  const p = path.join(SIM_OUT, ...parts)
  const r = path.relative(SIM_OUT, p)
  if (r.startsWith('..') || path.isAbsolute(r)) return null
  return p
}
function listRoots() {
  if (!fs.existsSync(SIM_OUT)) return []
  const entries = fs.readdirSync(SIM_OUT, { withFileTypes: true })
  const roots = []
  if (entries.some((e) => e.isDirectory() && isRunDir(path.join(SIM_OUT, e.name)))) roots.push('') // runs directly under sim_out
  for (const e of entries) if (e.isDirectory() && hasRunChild(path.join(SIM_OUT, e.name))) roots.push(e.name)
  return [...new Set(roots)].sort()
}
function listRuns(root) {
  const dir = root ? safeJoin(root) : SIM_OUT
  if (!dir || !fs.existsSync(dir)) return []
  return fs
    .readdirSync(dir, { withFileTypes: true })
    .filter((e) => e.isDirectory() && isRunDir(path.join(dir, e.name)))
    .map((e) => e.name)
    .sort()
}

function serveSimOut() {
  return {
    name: 'serve-sim-out',
    configureServer(server) {
      server.middlewares.use('/sim_out', (req, res, next) => {
        try {
          const u = new URL(req.url || '/', 'http://localhost')
          const seg = decodeURIComponent(u.pathname).split('/').filter(Boolean)
          const json = (obj) => {
            res.setHeader('Content-Type', 'application/json')
            res.end(JSON.stringify(obj))
          }

          if (seg.length === 1 && seg[0] === 'roots.json') return json({ roots: listRoots() })
          if (seg.length === 1 && seg[0] === 'runs.json') return json({ runs: listRuns(u.searchParams.get('root') || '') })

          // <path...>/index.json  -> list the run dir's files
          if (seg.length >= 2 && seg[seg.length - 1] === 'index.json') {
            const dir = safeJoin(seg.slice(0, -1).join('/'))
            const files = dir && fs.existsSync(dir) ? fs.readdirSync(dir).filter((f) => f.endsWith('.ndjson')).sort() : []
            return json({ files })
          }
          // <path...>/<file>  -> serve the file
          if (seg.length >= 2) {
            const file = safeJoin(seg.join('/'))
            if (file && fs.existsSync(file) && fs.statSync(file).isFile()) {
              res.setHeader('Content-Type', 'application/x-ndjson')
              fs.createReadStream(file).pipe(res)
              return
            }
          }
          res.statusCode = 404
          res.end('not found')
        } catch (e) {
          next(e)
        }
      })
    },
  }
}

export default defineConfig({
  plugins: [vue(), serveSimOut()],
})
