import { defineConfig } from "vite";

export default defineConfig({
  /* Relative base so Electron can load app/dist via loadFile. */
  base: "./",
  server: {
    port: 5173,
    host: "127.0.0.1",
    /* Desktop runner sets BROWSER=none; keep Chrome open for plain `pnpm dev`. */
    open: process.env.BROWSER !== "none",
  },
});
