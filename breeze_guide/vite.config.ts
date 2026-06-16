import { defineConfig } from "vite";
import { svelte } from "@sveltejs/vite-plugin-svelte";
import { svelteTesting } from "@testing-library/svelte/vite";
import { VitePWA } from "vite-plugin-pwa";
import EnvironmentPlugin from "vite-plugin-environment";

// https://vitejs.dev/config/
export default defineConfig({
  base: "/",
  build: {
    sourcemap: true,
  },
  server: {
    port: 3000,
  },
  plugins: [
    EnvironmentPlugin({
      GITHUB_SHA: null,
      SENTRY_DSN: null,
    }),
    svelte(),
    svelteTesting(),
    VitePWA({
      devOptions: {
        enabled: true,
      },
      includeAssets: ["favicon.png"],
      manifest: {
        short_name: "Breeze Guide",
        name: "Breeze Guide",
        description: "Cataclysm: Dark Days Ahead 游戏数据指南 — 搜索物品、怪物、家具等详细信息",
        icons: [
          {
            src: "icon-192.png",
            type: "image/png",
            sizes: "192x192",
          },
          {
            src: "icon-512.png",
            type: "image/png",
            sizes: "512x512",
          },
        ],
        start_url: "./",
        theme_color: "#202020",
        background_color: "#1c1c1c",
        display: "standalone",
      },
      workbox: {
        navigateFallback: "index.html",
        runtimeCaching: [
          {
            urlPattern:
              /\/data\/(all|zh_CN)\.json(\?.*)?$/,
            handler: "CacheFirst",
          },
        ],
        // Without this, a stale service worker can be alive for a long time,
        // and get out of date with the server.
        skipWaiting: true,
      },
    }),
  ],
});
