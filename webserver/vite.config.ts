import { defineConfig } from 'vite';

// Followup captive-portal build.
//
// Emits a small, fixed set of files so the ESP32 firmware can embed them with stable names and
// serve them from the Wi-Fi provisioning httpd:
//   dist/index.html   -> served at "/"
//   dist/index.js     -> served at "/index.js"
//   dist/index.css    -> served at "/index.css"
//
// `base: ''` keeps asset references relative so the page works regardless of the mount path the
// device serves it from. The API calls in src/portal are absolute ("/api/...") and match the
// Followup firmware routes (wifi_service / timezone_service / gemini_service).
export default defineConfig({
  base: '',
  build: {
    assetsDir: '.',
    rollupOptions: {
      output: {
        entryFileNames: 'index.js',
        chunkFileNames: 'index.js',
        assetFileNames: 'index.[ext]',
      },
    },
  },
});
