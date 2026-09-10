import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// The viewer is a standalone frontend app. `npm run dev` is a DEVELOPMENT server
// only; it never sits near the C++ federate. The federate emits NDJSON; this reads
// it. The shippable form is the static `npm run build` output (or a hosted page).
export default defineConfig({
  plugins: [vue()],
})
