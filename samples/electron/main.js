// Electron main process: RVP session authority + sharedTexture frame broker.
//
//   renderer ══IPC══ main ══child_process IPC══ engine-host (RUN_AS_NODE + addon)
//
// main never touches pixels: the engine exports GPU textures (handles already
// duplicated into THIS process), main imports them with sharedTexture and
// forwards them to the renderer's compositor. Protocol: protocol.js.

'use strict';

const path = require('path');
const fs = require('fs');
const { fork } = require('child_process');
const { app, BrowserWindow, ipcMain, sharedTexture } = require('electron');
const P = require('./protocol');
const { IPC, E, EVT, SESSION, METHOD } = P;

// ---------------------------------------------------------------------------
// configuration
// ---------------------------------------------------------------------------
const REPO_ROOT = path.resolve(__dirname, '..', '..');
const PROGRAM_DIR = process.env.RBC_PROGRAM_DIR ||
  path.join(REPO_ROOT, 'src', 'robocute', 'rbc_ext', '_C');
const PROJECT_PATH = process.env.RBC_PROJECT ||
  path.resolve(REPO_ROOT, '..', 'rbc-project-default');
const BACKEND = process.env.RBC_BACKEND || 'dx';
const NATIVE_PATH = path.join(__dirname, 'native', 'build', 'rbc_ext_node.node');
const SCREENSHOT_DIR = path.join(__dirname, 'screenshots');
const SMOKE = process.argv.includes('--smoke');
const SMOKE_TIMEOUT_MS = parseInt(process.env.RBC_SMOKE_TIMEOUT || '120000', 10);
// RBC_AUTOCLOSE=<ms>: close the window after N ms (exit-path testing)
const AUTO_CLOSE_MS = parseInt(process.env.RBC_AUTOCLOSE || '0', 10);
// failing imports in a row before the transport is declared unsupported
const IMPORT_FAILURE_LIMIT = 10;

let win = null;
let engine = null;
let shuttingDown = false;
let statsCount = 0;
let gpuProcessGone = 0;

// ---------------------------------------------------------------------------
// session
// ---------------------------------------------------------------------------
const session = {
  protocol: P.PROTOCOL_VERSION,
  state: SESSION.BOOTING,
  reasons: [],
  message: '',
  host: {
    electron: process.versions.electron,
    chrome: process.versions.chrome,
    platform: process.platform,
    backend: BACKEND,
  },
  renderer: null,
  engine: null,
  surface: null,
};

function emit(evt) {
  if (win && !win.isDestroyed()) win.webContents.send(IPC.EVENT, evt);
}

function setSession(state, { reasons, message } = {}) {
  session.state = state;
  if (reasons) session.reasons = reasons;
  if (message !== undefined) session.message = message;
  console.log(`[rvp] session -> ${state}${session.reasons.length ? ' (' + session.reasons.join('; ') + ')' : ''}${session.message ? ' ' + session.message : ''}`);
  emit({ type: EVT.SESSION, ...session });
  if (SMOKE && (state === SESSION.UNSUPPORTED || state === SESSION.ERROR)) {
    console.error('[rvp] smoke: FAILED, session', state);
    stopAll(2);
  }
}

// Host-side requirements, checked before anything is spawned.
async function hostReasons() {
  const reasons = [];
  if (!sharedTexture || typeof sharedTexture.importSharedTexture !== 'function') {
    reasons.push(`Electron ${process.versions.electron} 不提供 sharedTexture API (需要 Electron >= 40)`);
  }
  // feature status is only meaningful once the GPU process has initialized
  let gpuInfo = null;
  try { gpuInfo = await app.getGPUInfo('complete'); } catch (_) {}
  const gpu = app.getGPUFeatureStatus();
  if (!String(gpu.gpu_compositing || '').startsWith('enabled')) {
    reasons.push(`GPU 合成不可用 (gpu_compositing=${gpu.gpu_compositing}),shared texture 需要硬件加速`);
  }
  const active = gpuInfo && (gpuInfo.gpuDevice || []).find((d) => d.active);
  session.host.gpu = active ? active.deviceString : (gpuInfo?.auxAttributes?.glRenderer || 'unknown');
  if (!fs.existsSync(NATIVE_PATH)) {
    reasons.push(`原生扩展未构建: ${NATIVE_PATH} (xmake build rbc_ext_node)`);
  }
  if (!fs.existsSync(path.join(PROGRAM_DIR, `shader_build_${BACKEND}`))) {
    reasons.push(`引擎运行时缺失: ${path.join(PROGRAM_DIR, 'shader_build_' + BACKEND)} (uv run pre-pack)`);
  }
  return reasons;
}

function rendererReasons(caps) {
  const reasons = [];
  if (!caps) return ['renderer 未上报能力'];
  if (caps.protocol !== P.PROTOCOL_VERSION) reasons.push(`renderer 协议版本 ${caps.protocol} != ${P.PROTOCOL_VERSION}`);
  if (!caps.sharedTexture) reasons.push('renderer 不支持 sharedTexture.subtle.finishTransferSharedTexture');
  if (!caps.webgpu) reasons.push(`renderer 不支持 WebGPU: ${caps.reason || '初始化失败'}`);
  return reasons;
}

// ---------------------------------------------------------------------------
// FRAME plane: lease broker (see protocol.js "the lease protocol")
// ---------------------------------------------------------------------------
const RING_SLOTS = 4;
const MAX_IN_FLIGHT = 2;
const ACK_TIMEOUT_MS = 2000;
const transport = {
  received: 0, delivered: 0, released: 0, holdMs: 0,
  dropped: 0, superseded: 0, importFailed: 0, ackTimeout: 0,
};
let rendererReady = false;
const inFlight = new Map();   // leaseKey -> {lease, imported, t, timer}
let queued = null;
let importFailStreak = 0;

const leaseKey = (l) => `${l.epoch}:${l.slot}:${l.frameId}`;

function returnLease(lease) {
  engineSend({ t: E.RELEASE, epoch: lease.epoch, slot: lease.slot, frameId: lease.frameId });
}

function dropLease(lease, counter) {
  transport[counter]++;
  returnLease(lease);
}

function offerFrame(lease) {
  transport.received++;
  if (!rendererReady || session.state !== SESSION.RUNNING || !win || win.isDestroyed()) {
    dropLease(lease, 'dropped');
    return;
  }
  if (inFlight.size >= MAX_IN_FLIGHT) {
    if (queued) dropLease(queued, 'superseded');
    queued = lease;
    return;
  }
  deliver(lease);
}

function onImportFailure(lease, e) {
  dropLease(lease, 'importFailed');
  importFailStreak++;
  if (importFailStreak === 1) console.error('[rvp] importSharedTexture failed:', e && e.message);
  if (importFailStreak >= IMPORT_FAILURE_LIMIT && session.state === SESSION.RUNNING) {
    setSession(SESSION.UNSUPPORTED, {
      reasons: [`Chromium 无法导入引擎纹理: ${e && e.message} (引擎与 Chromium 是否运行在同一 GPU 适配器?)`],
    });
    engineSend({ t: E.STOP });
  }
}

function deliver(lease) {
  let imported = null;
  let transfer = null;
  try {
    imported = sharedTexture.subtle.importSharedTexture({
      pixelFormat: session.engine.pixelFormat,
      codedSize: { width: lease.width, height: lease.height },
      timestamp: Math.round(lease.timestampUs),
      handle: P.textureHandle(session.engine.handleType, lease.handle),
    });
    transfer = imported.startTransferSharedTexture();
    importFailStreak = 0;
  } catch (e) {
    try { imported?.release(); } catch (_) {}
    onImportFailure(lease, e);
    return;
  }
  const key = leaseKey(lease);
  const timer = setTimeout(() => {
    transport.ackTimeout++;
    rendererReady = false;
    setSession(SESSION.ERROR, { message: 'renderer GPU 未归还帧租约，连接已停止；不复用仍被读取的纹理。' });
    engineSend({ t: E.STOP });
  }, ACK_TIMEOUT_MS);
  inFlight.set(key, { lease, imported, t: Date.now(), timer });
  win.webContents.send(IPC.FRAME, {
    epoch: lease.epoch,
    slot: lease.slot,
    frameId: lease.frameId,
    width: lease.width,
    height: lease.height,
    inputSeq: lease.inputSeq,
    gpuMs: lease.gpuMs,
  }, transfer);
  transport.delivered++;
}

// renderer GPU done (FRAME_DONE), ack timeout, or renderer gone
function completeFrame(key, counter) {
  const rec = inFlight.get(key);
  if (!rec) return;
  inFlight.delete(key);
  clearTimeout(rec.timer);
  try { rec.imported.release(); } catch (_) {}
  if (counter) {
    transport[counter]++;
  } else {
    transport.released++;
    const hold = Date.now() - rec.t;
    transport.holdMs = transport.holdMs ? transport.holdMs * 0.9 + hold * 0.1 : hold;
  }
  returnLease(rec.lease);
  if (queued && inFlight.size < MAX_IN_FLIGHT && rendererReady) {
    const next = queued;
    queued = null;
    deliver(next);
  }
}

function abortAllFrames() {
  if (queued) dropLease(queued, 'dropped');
  queued = null;
  if (inFlight.size && !shuttingDown) {
    setSession(SESSION.ERROR, { message: 'renderer 已断开，shared texture 连接停止。' });
    engineSend({ t: E.STOP });
  }
}

ipcMain.on(IPC.FRAME_DONE, (_e, ack) => {
  if (ack) completeFrame(leaseKey(ack));
});
// ---------------------------------------------------------------------------
// engine-host process
// ---------------------------------------------------------------------------
const pendingCalls = new Map();
let nextCallId = 1;

function engineSend(msg) {
  if (!engine || !engine.connected) return false;
  try {
    engine.send(msg);
    return true;
  } catch (_) {
    return false;
  }
}

function engineCall(method, params) {
  return new Promise((resolve, reject) => {
    if (session.state !== SESSION.RUNNING) {
      reject(new Error(`engine not running (session ${session.state})`));
      return;
    }
    const id = nextCallId++;
    const timer = setTimeout(() => {
      pendingCalls.delete(id);
      reject(new Error(`engine call '${method}' timed out`));
    }, 5000);
    pendingCalls.set(id, { resolve, reject, timer });
    if (!engineSend({ t: E.CALL, id, method, params })) {
      clearTimeout(timer);
      pendingCalls.delete(id);
      reject(new Error('engine pipe closed'));
    }
  });
}

function spawnEngine() {
  setSession(SESSION.PROBING);
  // ELECTRON_RUN_AS_NODE: pure Node runtime, no Chromium GPU stack in the
  // engine process. 'advanced' serialization keeps numbers/strings intact.
  engine = fork(path.join(__dirname, 'engine-host.js'), [], {
    env: { ...process.env, ELECTRON_RUN_AS_NODE: '1', RBC_BACKEND: BACKEND, RBC_PROGRAM_DIR: PROGRAM_DIR },
    stdio: ['ignore', 'inherit', 'inherit', 'ipc'],
    serialization: 'advanced',
  });
  engine.on('message', onEngineMessage);
  engine.on('exit', (code) => {
    console.error(`[rvp] engine host exited (code ${code})`);
    for (const rec of inFlight.values()) clearTimeout(rec.timer);
    inFlight.clear();
    queued = null;
    for (const { reject, timer } of pendingCalls.values()) { clearTimeout(timer); reject(new Error('engine exited')); }
    pendingCalls.clear();
    if (shuttingDown) return;
    if (session.state !== SESSION.UNSUPPORTED && session.state !== SESSION.ERROR) {
      setSession(SESSION.ERROR, { message: `engine host exited with code ${code}` });
    }
  });
}

function onEngineMessage(msg) {
  if (!msg || typeof msg.t !== 'string') return;
  switch (msg.t) {
    case E.FRAME:
      offerFrame(msg);
      break;
    case E.HELLO: {
      session.engine = msg.caps;
      const reasons = [];
      if (msg.protocol !== P.PROTOCOL_VERSION) reasons.push(`engine 协议版本 ${msg.protocol} != ${P.PROTOCOL_VERSION}`);
      if (!msg.caps.supported) reasons.push(`引擎不支持纹理导出: ${msg.caps.reason}`);
      if (reasons.length) {
        setSession(SESSION.UNSUPPORTED, { reasons });
        return;
      }
      setSession(SESSION.INITIALIZING);
      engineSend({
        t: E.START,
        options: {
          projectPath: PROJECT_PATH,
          backend: BACKEND,
          programPath: PROGRAM_DIR,
          width: session.viewport?.width || 1280,
          height: session.viewport?.height || 720,
          hostPid: process.pid,
          slots: RING_SLOTS,
          maxFps: 60,
          leaseTimeoutMs: 3000,
        },
      });
      break;
    }
    case E.STATE:
      if (msg.state === 'running') setSession(SESSION.RUNNING, { message: '' });
      else if (msg.state === 'initializing') setSession(SESSION.INITIALIZING);
      else if (msg.state === 'stopped') { if (session.state === SESSION.RUNNING) setSession(SESSION.STOPPED); }
      else if (msg.state === 'error') {
        const unsupported = /^unsupported:/.test(msg.message || '');
        setSession(unsupported ? SESSION.UNSUPPORTED : SESSION.ERROR,
          unsupported ? { reasons: [msg.message] } : { message: msg.message });
      }
      break;
    case E.SURFACE:
      session.surface = { epoch: msg.epoch, width: msg.width, height: msg.height, slots: msg.slots };
      emit({ type: EVT.SURFACE, ...session.surface });
      break;
    case E.STATS:
      if (++statsCount <= 3 || statsCount % 10 === 0) {
        console.log(`[rvp] stats #${statsCount}: engine ${msg.fps.toFixed(1)} fps, published ${msg.published}, skipped ${msg.skipped}, ` +
          `free ${msg.slotsFree}/${msg.slots}, expired ${msg.leaseExpired} | transport ${JSON.stringify(transport)}`);
      }
      emit({ type: EVT.STATS, engine: msg, transport: { ...transport } });
      break;
    case E.RESULT: {
      const call = pendingCalls.get(msg.id);
      if (!call) return;
      pendingCalls.delete(msg.id);
      clearTimeout(call.timer);
      if (msg.ok) call.resolve(msg.value);
      else call.reject(new Error(msg.error));
      break;
    }
    case E.LOG:
      console.log('[engine]', msg.message);
      break;
  }
}

// ---------------------------------------------------------------------------
// renderer IPC (CONTROL plane entry)
// ---------------------------------------------------------------------------
ipcMain.handle(IPC.HELLO, async (_e, hello) => {
  session.renderer = hello && hello.caps;
  const reasons = [...await hostReasons(), ...rendererReasons(hello && hello.caps)];
  if (reasons.length) {
    setSession(SESSION.UNSUPPORTED, { reasons });
    return { ...session };
  }
  rendererReady = true;
  if (!engine) spawnEngine();
  return { ...session };
});

ipcMain.on(IPC.INPUT, (_e, pkt) => {
  if (session.state !== SESSION.RUNNING || !pkt) return;
  engineSend({ t: E.INPUT, seq: pkt.seq, yaw: pkt.yaw, pitch: pkt.pitch, dolly: pkt.dolly });
});

ipcMain.on(IPC.VIEWPORT, (_e, vp) => {
  if (!vp || !(vp.width > 0) || !(vp.height > 0)) return;
  session.viewport = { width: Math.round(vp.width), height: Math.round(vp.height), dpr: vp.dpr };
  engineSend({ t: E.RESIZE, width: session.viewport.width, height: session.viewport.height });
});

ipcMain.on(IPC.REPORT, (_e, line) => console.log('[renderer]', line));

function timestampName(prefix) {
  const d = new Date();
  const pad = (n) => String(n).padStart(2, '0');
  return `${prefix}-${d.getFullYear()}${pad(d.getMonth() + 1)}${pad(d.getDate())}-` +
    `${pad(d.getHours())}${pad(d.getMinutes())}${pad(d.getSeconds())}.png`;
}

function writeDataUrl(file, dataUrl) {
  const png = Buffer.from(String(dataUrl).split(',')[1] || '', 'base64');
  if (!png.length) throw new Error('empty capture');
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, png);
  return png.length;
}

const hostMethods = {
  [METHOD.SAVE_CAPTURE]: ({ dataUrl }) => {
    const out = path.join(SCREENSHOT_DIR, timestampName('screenshot'));
    const bytes = writeDataUrl(out, dataUrl);
    console.log('[rvp] capture saved:', out, bytes, 'bytes');
    return { path: out, bytes };
  },
  [METHOD.SMOKE_DONE]: ({ dataUrl, summary }) => {
    const out = path.join(__dirname, 'smoke_screenshot.png');
    const bytes = writeDataUrl(out, dataUrl);
    const ok = summary && summary.meanRgb > 2 && summary.inputSeq >= 1 && summary.epoch >= 2 &&
      transport.ackTimeout === 0 && gpuProcessGone === 0 && session.state === SESSION.RUNNING;
    console.log(`[rvp] smoke: ${ok ? 'OK' : 'FAILED (viewport is black)'}`, out, bytes, 'bytes',
      JSON.stringify(summary), JSON.stringify(transport));
    setTimeout(() => stopAll(ok ? 0 : 3), 50);
    return {};
  },
};

ipcMain.handle(IPC.CALL, async (_e, { method, params } = {}) => {
  const host = hostMethods[method];
  if (host) return host(params || {});
  return engineCall(method, params || {});
});

// ---------------------------------------------------------------------------
// window + lifecycle
// ---------------------------------------------------------------------------
function createWindow() {
  win = new BrowserWindow({
    width: 1380,
    height: 860,
    minWidth: 900,
    minHeight: 560,
    show: !SMOKE || process.env.RBC_SMOKE_SHOW === '1',
    title: 'RoboCute Electron Demo (LuisaCompute · sharedTexture)',
    backgroundColor: '#16181d',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      // preload requires ./protocol.js
      sandbox: false,
      nodeIntegration: false,
      // the viewport must keep receiving frames while hidden (smoke) or occluded
      backgroundThrottling: false,
    },
  });
  const hwnd = win.getNativeWindowHandle();
  console.log(`[rvp] app window hwnd=${(hwnd.length >= 8 ? hwnd.readBigUInt64LE() : BigInt(hwnd.readUInt32LE(0))).toString(16).padStart(16, '0')}`);
  win.webContents.on('did-start-navigation', (details) => {
    // renderer reload: its frame handler is gone until the next HELLO
    if (details.isMainFrame && !details.isSameDocument) {
      rendererReady = false;
      abortAllFrames();
    }
  });
  win.webContents.on('render-process-gone', () => { rendererReady = false; abortAllFrames(); });
  win.webContents.on('preload-error', (_e, file, err) => {
    console.error('[rvp] preload failed:', file, err && err.stack || err);
    setSession(SESSION.ERROR, { message: `preload failed: ${err && err.message}` });
  });
  win.webContents.on('console-message', (e) => {
    if (e.level === 'error' || e.level === 'warning') console.log(`[renderer:${e.level}] ${e.message} (${e.sourceId}:${e.lineNumber})`);
  });
  win.loadFile(path.join(__dirname, 'renderer', 'index.html'), { query: SMOKE ? { smoke: '1' } : {} });
  if (!SMOKE && process.env.RBC_DEVTOOLS === '1') win.webContents.openDevTools({ mode: 'detach' });
  setSession(SESSION.WAITING_RENDERER);
}

function stopAll(exitCode = 0) {
  if (shuttingDown) return;
  shuttingDown = true;
  engineSend({ t: E.STOP });
  try { engine?.kill(); } catch (_) {}
  // NOTE: never app.quit() from here. Chromium destroys windows during
  // shutdown, re-triggering 'window-all-closed'; a re-entrant app.quit()
  // deadlocks the browser process (README pitfall 1). Deferred hard exit.
  setTimeout(() => process.exit(exitCode), 100);
}

app.whenReady().then(() => {
  createWindow();
  if (SMOKE) {
    setTimeout(() => {
      console.error(`[rvp] smoke: FAILED, timeout (session ${session.state})`, JSON.stringify(transport));
      stopAll(1);
    }, SMOKE_TIMEOUT_MS);
  }
  if (AUTO_CLOSE_MS > 0) {
    setTimeout(() => { console.log('[rvp] autoclose'); win?.close(); }, AUTO_CLOSE_MS);
  }
});

app.on('window-all-closed', () => stopAll(0));
app.on('child-process-gone', (_e, d) => {
  console.error(`[rvp] chromium ${d.type} process gone: ${d.reason} (exit ${d.exitCode})`);
  if (d.type === 'GPU') gpuProcessGone++;
  if (d.type === 'GPU' && !shuttingDown) {
    rendererReady = false;
    setSession(SESSION.ERROR, { message: `Chromium GPU 进程退出 (${d.exitCode})，shared texture 连接已停止。` });
    engineSend({ t: E.STOP });
  }
});
