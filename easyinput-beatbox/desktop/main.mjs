import { app, BrowserWindow, session } from "electron";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const isDev = process.argv.includes("--dev");
const ESPRESSIF_VID = 0x303a;
const DEV_URL = "http://127.0.0.1:5173";

/** @type {BrowserWindow | null} */
let mainWindow = null;

function wireSerialPermissions() {
  const ses = session.defaultSession;

  ses.setPermissionCheckHandler((_wc, permission) => permission === "serial");

  ses.setDevicePermissionHandler((details) => details.deviceType === "serial");

  ses.on("select-serial-port", (event, portList, _webContents, callback) => {
    event.preventDefault();
    if (!portList.length) {
      callback("");
      return;
    }
    const preferred =
      portList.find((port) => {
        const vid = Number(port.vendorId);
        return vid === ESPRESSIF_VID;
      }) ?? portList[0];
    callback(preferred.portId);
  });
}

async function loadRenderer(win) {
  if (isDev) {
    let lastError = null;
    for (let attempt = 0; attempt < 60; attempt++) {
      try {
        await win.loadURL(DEV_URL);
        return;
      } catch (err) {
        lastError = err;
        await new Promise((r) => setTimeout(r, 250));
      }
    }
    throw lastError ?? new Error(`Dev server not reachable at ${DEV_URL}. Run pnpm dev first.`);
  }

  const indexHtml = path.join(__dirname, "../app/dist/index.html");
  await win.loadFile(indexHtml);
}

async function createWindow() {
  const isMac = process.platform === "darwin";

  /* Fixed fit for the Beatbox layout (header + dual panels); no free resize. */
  const winWidth = 1140;
  const winHeight = 710;

  mainWindow = new BrowserWindow({
    width: winWidth,
    height: winHeight,
    minWidth: winWidth,
    maxWidth: winWidth,
    minHeight: winHeight,
    maxHeight: winHeight,
    resizable: false,
    maximizable: false,
    fullscreenable: false,
    title: "EasyInput Beatbox",
    backgroundColor: "#0e1116",
    /* macOS: content under titlebar, no tall chrome strip. */
    ...(isMac
      ? {
          titleBarStyle: "hiddenInset",
          /* Sit in the reserved spacer row above the title lockup. */
          trafficLightPosition: { x: 14, y: 12 },
        }
      : {}),
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
    show: false,
  });

  mainWindow.once("ready-to-show", () => {
    /* Force exact bounds — main-process edits need a full Electron restart. */
    mainWindow?.setSize(winWidth, winHeight);
    mainWindow?.show();
  });

  mainWindow.webContents.on("did-finish-load", () => {
    void mainWindow?.webContents.executeJavaScript(
      `document.documentElement.classList.add('electron-shell');`,
    );
  });

  await loadRenderer(mainWindow);

  if (isDev) {
    mainWindow.webContents.openDevTools({ mode: "detach" });
  }

  mainWindow.on("closed", () => {
    mainWindow = null;
  });
}

app.whenReady().then(async () => {
  wireSerialPermissions();
  await createWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      void createWindow();
    }
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});
