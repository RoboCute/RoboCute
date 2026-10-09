// Engine host: child process (ELECTRON_RUN_AS_NODE, plain Node runtime) that
// owns the rbc_ext_node addon. Keeps the engine's D3D12 stack out of every
// Chromium process, and a native crash cannot take the UI down.
//
// Bridge only — protocol semantics live in protocol.js (RVP v1):
//   main -> host : E.START / E.INPUT / E.RESIZE / E.RELEASE / E.CALL / E.STOP
//   host -> main : E.HELLO / E.STATE / E.SURFACE / E.FRAME / E.STATS / E.RESULT / E.LOG

'use strict';

const path = require('path');
const fs = require('fs');
const P = require('./protocol');
const { E, METHOD } = P;

const REPO_ROOT = path.resolve(__dirname, '..', '..');
const PROGRAM_DIR = process.env.RBC_PROGRAM_DIR ||
  path.join(REPO_ROOT, 'src', 'robocute', 'rbc_ext', '_C');
const BACKEND = process.env.RBC_BACKEND || 'dx';
const NATIVE_PATH = path.join(__dirname, 'native', 'build', 'rbc_ext_node.node');

// DLL search path for the engine runtime (rbc/luisa dlls live here).
process.env.PATH = PROGRAM_DIR + path.delimiter + process.env.PATH;

const t0 = Date.now();
function send(msg) {
  try {
    process.send?.(msg);
    return true;
  } catch (e) {
    console.error('[engine-host] send failed:', e && e.message);
    return false;
  }
}
const log = (message) => send({ t: E.LOG, message });

process.on('disconnect', () => {
  // main is gone: nobody can return leases or display frames
  try { native?.stop(); } catch (_) {}
  process.exit(0);
});

let native = null;
let loadError = null;
if (!fs.existsSync(NATIVE_PATH)) {
  loadError = 'native addon not built: ' + NATIVE_PATH;
} else {
  try {
    native = require(NATIVE_PATH);
    log(`addon loaded @${Date.now() - t0}ms`);
  } catch (e) {
    loadError = 'addon load failed: ' + (e && e.message || e);
  }
}

function capabilities() {
  if (!native) {
    return { supported: false, reason: loadError, platform: process.platform, handleType: '', pixelFormat: '', protocol: P.PROTOCOL_VERSION };
  }
  return native.capabilities(BACKEND);
}

let started = false;
let statsTimer = null;

function onEngineEvent(evt) {
  switch (evt.type) {
    case 'frame': {
      const lease = { t: E.FRAME, ...evt };
      delete lease.type;
      // a lease that cannot reach main must be returned right here
      if (!send(lease)) native.releaseFrame(evt.epoch, evt.slot, evt.frameId);
      break;
    }
    case 'surface':
      send({ t: E.SURFACE, epoch: evt.epoch, width: evt.width, height: evt.height, slots: evt.slots });
      break;
    case 'state':
      send({ t: E.STATE, state: evt.state, message: evt.message });
      break;
  }
}

const methods = {
  [METHOD.SET_TURNTABLE]: ({ enabled }) => {
    native.setTurntable(!!enabled);
    return { enabled: !!enabled };
  },
  [METHOD.GET_STATS]: () => native.getStats(),
};

process.on('message', (msg) => {
  if (!msg || typeof msg.t !== 'string') return;
  switch (msg.t) {
    case E.START: {
      if (started || !native) return;
      started = true;
      try {
        native.start(msg.options, onEngineEvent);
      } catch (e) {
        send({ t: E.STATE, state: 'error', message: String(e && e.message || e) });
        return;
      }
      statsTimer = setInterval(() => {
        try { send({ t: E.STATS, ...native.getStats() }); } catch (_) {}
      }, 1000);
      break;
    }
    case E.INPUT:
      native?.input(msg.seq, msg.yaw || 0, msg.pitch || 0, msg.dolly || 0);
      break;
    case E.RESIZE:
      native?.resize(msg.width, msg.height);
      break;
    case E.RELEASE:
      native?.releaseFrame(msg.epoch, msg.slot, msg.frameId);
      break;
    case E.CALL: {
      const fn = methods[msg.method];
      if (!fn || !native) {
        send({ t: E.RESULT, id: msg.id, ok: false, error: `unknown method '${msg.method}'` });
        return;
      }
      try {
        send({ t: E.RESULT, id: msg.id, ok: true, value: fn(msg.params || {}) });
      } catch (e) {
        send({ t: E.RESULT, id: msg.id, ok: false, error: String(e && e.message || e) });
      }
      break;
    }
    case E.STOP:
      if (statsTimer) { clearInterval(statsTimer); statsTimer = null; }
      try { native?.stop(); } catch (_) {}
      started = false;
      break;
  }
});

send({ t: E.HELLO, protocol: P.PROTOCOL_VERSION, caps: capabilities() });
