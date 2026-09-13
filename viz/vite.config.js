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

// Dev middleware. Each SUBFOLDER of sim_out/ is one simulation RUN (a partitioning
// trial); the .ndjson files inside it are that run's federates + a controller.ndjson.
//   GET /sim_out/index.json         -> { "runs": ["threeway", "invariance_k=2", ...] }
//   GET /sim_out/<run>/index.json   -> { "files": ["controller.ndjson", "One.ndjson", ...] }
//   GET /sim_out/<run>/<file>       -> the raw NDJSON file
// Back-compat: any .ndjson sitting directly in sim_out/ (the old flat layout) is exposed
// as a run named "(root)". This is a DEV convenience only (a static build has no server);
// the federate never gets a server of its own -- it only writes files (ADR-0018).
const ROOT_RUN = '(root)'

function listRunDirs() {
  if (!fs.existsSync(SIM_OUT)) return []
  const entries = fs.readdirSync(SIM_OUT, { withFileTypes: true })
  const runs = entries
    .filter((e) => e.isDirectory() && hasNdjson(path.join(SIM_OUT, e.name)))
    .map((e) => e.name)
    .sort()
  // old flat layout: loose .ndjson at the root become the "(root)" run
  if (entries.some((e) => e.isFile() && e.name.endsWith('.ndjson'))) runs.unshift(ROOT_RUN)
  return runs
}
function hasNdjson(dir) {
  try {
    return fs.readdirSync(dir).some((f) => f.endsWith('.ndjson'))
  } catch {
    return false
  }
}
function runDir(run) {
  return run === ROOT_RUN ? SIM_OUT : path.join(SIM_OUT, path.basename(run))
}

function serveSimOut() {
  return {
    name: 'serve-sim-out',
    configureServer(server) {
      server.middlewares.use('/sim_out', (req, res, next) => {
        try {
          const urlPath = decodeURIComponent((req.url || '/').split('?')[0])
          // split into [] | ["index.json"] | [run] | [run, "index.json"] | [run, file]
          const seg = urlPath.split('/').filter(Boolean)
          const json = (obj) => {
            res.setHeader('Content-Type', 'application/json')
            res.end(JSON.stringify(obj))
          }

          // list all runs
          if (seg.length === 0 || (seg.length === 1 && seg[0] === 'index.json')) {
            return json({ runs: listRunDirs() })
          }
          // list one run's files
          if (seg.length === 2 && seg[1] === 'index.json') {
            const dir = runDir(seg[0])
            const files = fs.existsSync(dir)
              ? fs.readdirSync(dir).filter((f) => f.endsWith('.ndjson')).sort()
              : []
            return json({ files })
          }
          // serve one file from a run
          if (seg.length === 2) {
            const file = path.join(runDir(seg[0]), path.basename(seg[1]))
            if (fs.existsSync(file)) {
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
