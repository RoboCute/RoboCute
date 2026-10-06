// Engine host: runs in a dedicated Node.js child process (Electron
// utilityProcess). Hosts the rbc_ext_node addon so the render engine lives in
// a clean Node runtime — no Chromium GPU / D3D12 / dxil interference, and a
// native crash cannot take down the UI process.
//
// Protocol (IPC messages over the utility process pipe):
//   parent -> host: { cmd: 'start', options: {...} }
//   parent -> host: { cmd: 'cameraRotate' | 'cameraZoom', ... }
//   parent -> host: { cmd: 'turntable', enabled: bool }
//   parent -> host: { cmd: 'screenshot' }          (shared mode one-shot capture)
//   parent -> host: { cmd: 'stop' }
//   host -> parent: { type: 'frame', width, height, index, buffer: Buffer }
//   host -> parent: { type: 'screenshot', width, height, buffer: Buffer }
//   host -> parent: { type: 'stats', fps, width, height, shared, turntable }
//   host -> parent: { type: 'error', message }

const path = require('path');
const fs = require('fs');

const t0 = Date.now();
const stamp = (what) => process.send?.({ type: 'log', message: `timing: ${what} @${Date.now() - t0}ms` });

const REPO_ROOT = path.resolve(__dirname, '..', '..');
const PROGRAM_DIR = process.env.RBC_PROGRAM_DIR ||
  path.join(REPO_ROOT, 'src', 'robocute', 'rbc_ext', '_C');

// DLL search path for the engine runtime (rbc/luisa dlls live here).
process.env.PATH = PROGRAM_DIR + path.delimiter + process.env.PATH;

const NATIVE_PATH = path.join(__dirname, 'native', 'build', 'rbc_ext_node.node');
process.send?.({ type: 'log', message: 'engine-host: boot, native=' + NATIVE_PATH });
if (!fs.existsSync(NATIVE_PATH)) {
  process.send?.({ type: 'error', message: 'native addon not built: ' + NATIVE_PATH });
  process.exit(1);
}
// In this child process there is no Chromium, so no dxil.dll name conflict;
// skip the preload (it is only needed when the addon shares a process with
// Chromium, e.g. when loaded directly in the Electron main process).
let native;
try {
  stamp('require() begin');
  native = require(NATIVE_PATH);
  stamp('require() done');
  process.send?.({ type: 'log', message: 'engine-host: addon loaded' });
} catch (e) {
  process.send?.({ type: 'error', message: 'addon load failed: ' + (e && e.stack || e) });
  process.exit(1);
}

let running = false;
let streamWidth = 0, streamHeight = 0, streamIndex = 0;
let statsTimer = null;

function sendFrame(frame, error, w, h, kind) {
  if (error) {
    safeSend({ type: 'error', message: error });
    return;
  }
  try {
    if (kind === 'screenshot') {
      // one-shot capture (shared mode): payload for the screenshot pipeline
      safeSend({ type: 'screenshot', width: w, height: h, buffer: Buffer.from(frame) });
      return;
    }
    // Transfer as Buffer; child_process IPC supports Buffer payloads.
    safeSend({
      type: 'frame',
      width: w || streamWidth,
      height: h || streamHeight,
      index: streamIndex++,
      buffer: Buffer.from(frame),
    });
  } catch (e) {
    safeSend({ type: 'error', message: 'send failed: ' + (e && e.stack || e) });
  }
}

function safeSend(obj) {
  try {
    const ok = process.send?.(obj);
    if (obj.type === 'frame' && ok === false) {
      droppedFrames++;
      if (droppedFrames % 100 === 1) {
        console.error(`[engine-host] IPC backpressure, dropped=${droppedFrames} (parent too slow)`);
      }
    }
  } catch (e) {
    console.error('[engine-host] process.send failed:', e);
  }
}
let droppedFrames = 0;
process.on('disconnect', () => {
  console.error('[engine-host] IPC channel disconnected, exiting');
  process.exit(0);
});

process.send?.({ type: 'log', message: 'engine-host: registering message handler' });
process.on('message', (msg) => {
  if (!msg || typeof msg.cmd !== 'string') return;
  switch (msg.cmd) {
    case 'start': {
      if (running) return;
      running = true;
      stamp('start cmd received');
      const o = msg.options;
      streamWidth = o.width; streamHeight = o.height; streamIndex = 0;
      native.start(
        {
          projectPath: o.projectPath,
          backend: o.backend,
          programPath: o.programPath,
          width: o.width,
          height: o.height,
          present: o.present,
          parentHwnd: o.parentHwnd,
          viewportTop: o.viewportTop,
          viewportRight: o.viewportRight,
        },
        sendFrame
      );
      stamp('native.start returned');
      // stats feed for the UI sidebar (1 Hz)
      statsTimer = setInterval(() => {
        try {
          safeSend({ type: 'stats', ...native.getStats() });
        } catch (e) { /* engine may be stopping */ }
      }, 1000);
      break;
    }
    case 'cameraRotate':
      native.cameraRotate(msg.yaw, msg.pitch);
      break;
    case 'cameraZoom':
      native.cameraZoom(msg.dz);
      break;
    case 'turntable':
      native.cameraTurntable(!!msg.enabled);
      break;
    case 'screenshot':
      native.requestScreenshot();
      break;
    case 'stop':
      try { native.stop(); } catch (_) {}
      if (statsTimer) { clearInterval(statsTimer); statsTimer = null; }
      running = false;
      break;
  }
});

process.send?.({ type: 'log', message: 'engine-host: ready' });
process.send?.({ type: 'ready' });
