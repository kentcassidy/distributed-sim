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

// Dev middleware:
//   GET /sim_out/index.json  -> { "files": ["F1.ndjson", "F2.ndjson", ...] }
//   GET /sim_out/<name>      -> the raw NDJSON file
// This is a DEV convenience only (a static build has no server); the federate never
// gets a server of its own -- it only writes files (ADR-0018).
function serveSimOut() {
  return {
    name: 'serve-sim-out',
    configureServer(server) {
      server.middlewares.use('/sim_out', (req, res, next) => {
        try {
          const urlPath = decodeURIComponent((req.url || '/').split('?')[0])
          if (urlPath === '/' || urlPath === '/index.json') {
            const files = fs.existsSync(SIM_OUT)
              ? fs.readdirSync(SIM_OUT).filter((f) => f.endsWith('.ndjson')).sort()
              : []
            res.setHeader('Content-Type', 'application/json')
            res.end(JSON.stringify({ files }))
            return
          }
          const file = path.join(SIM_OUT, path.basename(urlPath))
          if (fs.existsSync(file)) {
            res.setHeader('Content-Type', 'application/x-ndjson')
            fs.createReadStream(file).pipe(res)
            return
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
