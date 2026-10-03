/**
 * One-shot desktop dev: Vite on :5173 + Electron --dev.
 * If Vite is already running, reuse it (don't fail on EADDRINUSE).
 * Ctrl+C stops processes this runner started (not a pre-existing Vite).
 */
import { spawn } from "node:child_process";
import http from "node:http";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const DEV_URL = "http://127.0.0.1:5173";

/** @type {import('node:child_process').ChildProcess[]} */
const children = [];
let startedVite = false;

function run(command, args) {
  const child = spawn(command, args, {
    cwd: root,
    stdio: "inherit",
    env: { ...process.env, BROWSER: "none" },
    shell: process.platform === "win32",
  });
  children.push(child);
  return child;
}

function shutdown(code = 0) {
  for (const child of children) {
    if (!child.killed) child.kill("SIGTERM");
  }
  process.exit(code);
}

function viteReady(timeoutMs = 400) {
  return new Promise((resolve) => {
    const req = http.get(DEV_URL, (res) => {
      res.resume();
      resolve(true);
    });
    req.setTimeout(timeoutMs, () => {
      req.destroy();
      resolve(false);
    });
    req.on("error", () => resolve(false));
  });
}

async function waitForVite(attempts = 60) {
  for (let i = 0; i < attempts; i++) {
    if (await viteReady()) return true;
    await new Promise((r) => setTimeout(r, 250));
  }
  return false;
}

process.on("SIGINT", () => shutdown(0));
process.on("SIGTERM", () => shutdown(0));

if (await viteReady()) {
  console.log(`[desktop] Reusing existing Vite at ${DEV_URL}`);
} else {
  startedVite = true;
  const vite = run("pnpm", [
    "--filter",
    "easyinput-beatbox-app",
    "exec",
    "vite",
    "--host",
    "127.0.0.1",
    "--port",
    "5173",
    "--strictPort",
  ]);
  vite.on("exit", (code) => {
    if (code && code !== 0) shutdown(code);
  });
  const ok = await waitForVite();
  if (!ok) {
    console.error(`[desktop] Vite failed to become ready at ${DEV_URL}`);
    shutdown(1);
  }
}

const electron = run("pnpm", ["--filter", "easyinput-beatbox-desktop", "dev"]);
electron.on("exit", (code) => {
  /* Keep a pre-existing Vite alive when Electron closes. */
  if (!startedVite) process.exit(code ?? 0);
  shutdown(code ?? 0);
});
