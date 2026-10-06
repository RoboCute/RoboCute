// Electron main process: spawns the engine in a dedicated utility process,
// hosts the app UI (toolbar + sidebar) around a native GPU-direct viewport.
//
// Process model:
//   main (this file) --fork(ELECTRON_RUN_AS_NODE)--> engine-host.js (Node runtime + addon)
//   main --IPC--> renderer (toolbar / sidebar / readback canvas)
// The engine never shares a process with Chromium: no dxil/D3D12 interference,
// and an engine crash cannot kill the UI.

const path = require('path');
const fs = require('fs');
const { fork } = require('child_process');
const { app, BrowserWindow, ipcMain } = require('electron');

// ---------------------------------------------------------------------------
// Locate the engine runtime directory (all rbc/luisa dlls + shader builds).
// In the repo this is src/robocute/rbc_ext/_C (BUILTIN_PROGRAM_PATH in app.py).
// ---------------------------------------------------------------------------
const REPO_ROOT = path.resolve(__dirname, '..', '..');
const PROGRAM_DIR = process.env.RBC_PROGRAM_DIR ||
  path.join(REPO_ROOT, 'src', 'robocute', 'rbc_ext', '_C');
const PROJECT_PATH = process.env.RBC_PROJECT ||
  path.resolve(REPO_ROOT, '..', 'rbc-project-default');
const BACKEND = process.env.RBC_BACKEND || 'dx';

// UI chrome layout: native viewport occupies the client area minus these
// insets; the HTML renderer draws toolbar/sidebar in that space. The SAME
// numbers are passed to the addon (window placement) and the renderer (CSS).
const UI_LAYOUT = { top: 56, right: 320 };
const SCREENSHOT_DIR = path.join(__dirname, 'screenshots');

if (!fs.existsSync(path.join(PROGRAM_DIR, `shader_build_${BACKEND}`))) {
  console.error(`[demo] shader build not found: ${path.join(PROGRAM_DIR, 'shader_build_' + BACKEND)}`);
  console.error('[demo] run the project build (`uv run pre-pack` / xmake) first.');
  process.exit(1);
}

const SMOKE = process.argv.includes('--smoke');
// RBC_DISABLE_HW_ACCEL=1: also useful for interactive runs — engine D3D12
// init is much faster when Chromium is not holding the GPU stack (see README).
const DISABLE_HW = SMOKE || process.env.RBC_DISABLE_HW_ACCEL === '1';
if (DISABLE_HW) {
  // Offscreen rendering: no visible window needed, capturePage still works.
  // no-sandbox: Chromium's sandbox cannot initialize in some automation
  // sessions; fine for a local demo, revisit for production packaging.
  app.disableHardwareAcceleration();
  app.commandLine.appendSwitch('no-sandbox');
}

let win = null;
let engine = null;
let shuttingDown = false;
let frameCount = 0;
let lastFpsTime = Date.now();
let statsCount = 0;

// Windows HWND of the browser window, as a decimal string (pointer-sized int
// exceeds Number's safe-integer range, so never pass it as a JS number).
function nativeWindowHandle(win) {
  try {
    const buf = win.getNativeWindowHandle();
    if (buf.length >= 8) return buf.readBigUInt64LE().toString();
    if (buf.length >= 4) return buf.readUInt32LE(0).toString();
  } catch (_) {}
  return '0';
}

function startEngine() {
  const width = 1280, height = 720;
  // shared: engine presents GPU-direct into a native child window (DXGI
  // swapchain, zero CPU copies). readback: CPU readback + IPC frames (smoke).
  const present = SMOKE ? 'readback' : 'shared';
  const parentHwnd = SMOKE ? '0' : nativeWindowHandle(win);
  console.log(`[demo] spawning engine host: project=${PROJECT_PATH} backend=${BACKEND} present=${present}`);

  // ELECTRON_RUN_AS_NODE gives the child a pure Node.js runtime — no Chromium
  // GPU/D3D12/dxil in the engine process (it crashes inside the main process).
  // utilityProcess was tried first but its stdio/message channel stayed silent
  // in this environment (Electron 37), so use plain child_process IPC.
  engine = fork(path.join(__dirname, 'engine-host.js'), [], {
    env: { ...process.env, ELECTRON_RUN_AS_NODE: '1' },
    stdio: ['ignore', 'inherit', 'inherit', 'ipc'],
    // 'advanced' (V8 serializer) moves binary frames efficiently; the default
    // 'json' mode explodes Buffers into multi-MB number arrays and saturates.
    serialization: 'advanced',
  });
  engine.on('message', (msg) => {
    if (!msg || typeof msg.type !== 'string') return;
    if (msg.type === 'log') {
      console.log('[engine]', msg.message);
      return;
    }
    if (msg.type === 'error') {
      console.error('[demo] engine error:', msg.message);
      win?.webContents.send('engine-status', { state: 'error', message: msg.message });
      return;
    }
    if (msg.type === 'stats') {
      statsCount++;
      if (statsCount <= 3 || statsCount % 10 === 0) console.log(`[demo] stats #${statsCount}: fps=${msg.fps && msg.fps.toFixed ? msg.fps.toFixed(1) : msg.fps} ${msg.width}x${msg.height}`);
      win?.webContents.send('stats', msg);
      return;
    }
    if (msg.type === 'screenshot') {
      // one-shot capture payload (shared mode) → renderer encodes PNG
      const ab = msg.buffer.buffer.slice(msg.buffer.byteOffset, msg.buffer.byteOffset + msg.buffer.byteLength);
      win?.webContents.send('screenshot-frame', ab, msg.width, msg.height);
      return;
    }
    if (msg.type === 'frame') {
      onFrame(msg);
    }
  });
  engine.on('exit', (code) => {
    console.error(`[demo] engine host exited with code ${code}`);
    win?.webContents.send('engine-status', { state: 'exited', code });
    // engine crashed / died on its own: take the UI down with it instead of
    // lingering with a frozen viewport (process-exit hardening, see stopAll)
    if (!shuttingDown) stopAll(code || 1);
  });
  engine.send({
    cmd: 'start',
    options: {
      projectPath: PROJECT_PATH,
      backend: BACKEND,
      programPath: PROGRAM_DIR,
      width,
      height,
      present,
      parentHwnd,
      viewportTop: UI_LAYOUT.top,
      viewportRight: UI_LAYOUT.right,
    },
  });
}

function onFrame(msg) {
  frameCount++;
  if (!SMOKE && (!win || win.isDestroyed())) return;
  if (win) {
    // contextBridge drops TypedArray payloads on the isolation boundary; an
    // ArrayBuffer survives the structured clone intact.
    const ab = msg.buffer.buffer.slice(msg.buffer.byteOffset, msg.buffer.byteOffset + msg.buffer.byteLength);
    win.webContents.send('frame', ab, msg.width, msg.height, msg.index);
  }
  const now = Date.now();
  if (now - lastFpsTime >= 2000) {
    console.log(`[demo] fps ~ ${((frameCount * 1000) / (now - lastFpsTime)).toFixed(1)}`);
    frameCount = 0;
    lastFpsTime = now;
  }
}

function stopAll(exitCode = 0) {
  if (shuttingDown) return;
  shuttingDown = true;
  try { engine?.send({ cmd: 'stop' }); } catch (_) {}
  try { engine?.kill(); } catch (_) {}
  // NOTE: do NOT call app.quit()/app.exit() from here. During shutdown
  // Chromium destroys all windows, which re-triggers 'window-all-closed'; a
  // re-entrant app.quit() deadlocks the browser process (observed on
  // Electron 37 / Windows 11: process lingers forever and even taskkill /F
  // cannot terminate it). The engine child is already gone, so a deferred
  // hard exit is the only reliable teardown for this demo.
  setTimeout(() => process.exit(exitCode), 100);
}

function createWindow() {
  win = new BrowserWindow({
    width: 1380,
    height: 860,
    minWidth: 900,
    minHeight: 560,
    show: !SMOKE,
    title: 'RoboCute Electron Demo (LuisaCompute)',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      sandbox: false,
      nodeIntegration: false,
      offscreen: SMOKE,
    },
  });
  const query = {
    top: String(UI_LAYOUT.top),
    right: String(UI_LAYOUT.right),
    mode: SMOKE ? 'readback' : 'shared',
  };
  if (SMOKE) query.snapshot = '1';
  win.loadFile(path.join(__dirname, 'renderer', 'index.html'), { query });
  if (!SMOKE) win.webContents.openDevTools({ mode: 'detach' });
}

// ---------------------------------------------------------------------------
// UI IPC: screenshot pipeline + turntable toggle
// ---------------------------------------------------------------------------

// shared mode: ask the engine for a one-shot readback; the payload comes back
// as a 'screenshot' message and is forwarded to the renderer as
// 'screenshot-frame' (encoded to PNG there, then returned via 'save-screenshot')
ipcMain.on('ui-screenshot', () => {
  try { engine?.send({ cmd: 'screenshot' }); } catch (e) {
    console.error('[demo] screenshot request failed:', e);
  }
});

ipcMain.on('ui-turntable', (_e, enabled) => {
  try { engine?.send({ cmd: 'turntable', enabled: !!enabled }); } catch (_) {}
});

// readback-mode camera gestures (renderer canvas -> engine)
ipcMain.on('camera-rotate', (_e, yaw, pitch) =>
  engine?.send({ cmd: 'cameraRotate', yaw, pitch }));
ipcMain.on('camera-zoom', (_e, dz) =>
  engine?.send({ cmd: 'cameraZoom', dz }));

// renderer hands back a PNG data URL; save it under screenshots/
ipcMain.on('save-screenshot', (_e, dataUrl) => {
  try {
    fs.mkdirSync(SCREENSHOT_DIR, { recursive: true });
    const png = Buffer.from(String(dataUrl).split(',')[1], 'base64');
    const d = new Date();
    const pad = (n) => String(n).padStart(2, '0');
    const name = `screenshot-${d.getFullYear()}${pad(d.getMonth() + 1)}${pad(d.getDate())}-` +
      `${pad(d.getHours())}${pad(d.getMinutes())}${pad(d.getSeconds())}.png`;
    const out = path.join(SCREENSHOT_DIR, name);
    fs.writeFileSync(out, png);
    console.log('[demo] screenshot saved:', out, png.length, 'bytes');
    win?.webContents.send('screenshot-saved', { ok: true, path: out });
  } catch (e) {
    console.error('[demo] save screenshot failed:', e);
    win?.webContents.send('screenshot-saved', { ok: false, message: String(e) });
  }
});

// Smoke mode keeps its original canvas-snapshot protocol.
ipcMain.on('renderer-report', (_e, msg) => {
  if (typeof msg === 'string' && msg.startsWith('SNAPSHOT:')) {
    const png = Buffer.from(msg.slice('SNAPSHOT:'.length).split(',')[1], 'base64');
    const out = path.join(__dirname, 'smoke_screenshot.png');
    fs.writeFileSync(out, png);
    console.log('[demo] smoke screenshot saved:', out, png.length, 'bytes');
    stopAll();
    return;
  }
  console.log('[renderer]', msg);
});

// Test hook: RBC_AUTOCLOSE=<ms> closes the window after N ms to exercise the
// graceful app.quit() shutdown path without manual interaction.
const AUTO_CLOSE_MS = parseInt(process.env.RBC_AUTOCLOSE || '0', 10);

app.whenReady().then(() => {
  // smoke still needs a (hidden, offscreen) window for the renderer pipeline
  createWindow();
  startEngine();
  if (AUTO_CLOSE_MS > 0) {
    setTimeout(() => { console.log('[demo] autoclose'); win?.close(); }, AUTO_CLOSE_MS);
  }
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => stopAll(0));
