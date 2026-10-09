// samples/electron/protocol.js
//
// RVP — RoboCute Viewport Protocol, v1.
// Single source of truth for every message crossing a process boundary in
// this sample. Required by main.js, engine-host.js and preload.js.
//
// ---------------------------------------------------------------------------
// Topology
// ---------------------------------------------------------------------------
//
//   renderer ══(Electron IPC)══ main ══(child_process IPC)══ engine-host ══(N-API)══ engine thread
//    (UI, WebGPU)     │        (session authority,              (RUN_AS_NODE,          (RBC/LC, D3D12)
//                     │         sharedTexture broker)            addon bridge)
//                     └─── sharedTexture.sendSharedTexture (GPU texture, no pixels on any pipe)
//
// Three planes share these pipes:
//
//   CONTROL  renderer -> engine   RPC (request/response) + two coalesced streams
//                                 (INPUT, VIEWPORT) that are fire-and-forget
//   FRAME    engine -> renderer   leases on exported GPU textures
//   EVENT    engine -> renderer   session / state / surface / stats notifications
//
// ---------------------------------------------------------------------------
// Session lifecycle (main owns the session state)
// ---------------------------------------------------------------------------
//
//   booting ─► waiting-renderer ─► probing ─► initializing ─► running ─► stopped
//                    │                │             │            │
//                    └────────────────┴─────────────┴────────────┴─► unsupported | error
//
//   1. renderer installs its IPC.FRAME handler, probes WebGPU, then invokes
//      HELLO {protocol, caps}. Frames are never sent before HELLO.
//   2. main merges host caps (Electron sharedTexture API, GPU compositing,
//      runtime files) with renderer caps. Any gap => 'unsupported' with
//      human-readable reasons; the engine is not spawned. THERE IS NO CPU
//      READBACK FALLBACK — unsupported is a terminal, explicit state.
//   3. main forks engine-host, which answers E.HELLO {caps} from the native
//      addon (platform texture export + backend). Gap => 'unsupported'.
//   4. main sends E.START; engine reports E.STATE initializing -> running and
//      E.SURFACE for every (re)allocated texture ring.
//
// ---------------------------------------------------------------------------
// FRAME plane: the lease protocol (the only cross-plane sync point)
// ---------------------------------------------------------------------------
//
//   engine   slot FREE -> GPU     blit render target into slot, signal fence
//            slot GPU  -> LEASED  fence passed: E.FRAME {epoch, slot, frameId,
//                                 handle, width, height, inputSeq, ...}
//   main     imported = sharedTexture.subtle.importSharedTexture({ handle })
//            (handle is already valid in main: the engine DuplicateHandle'd
//            it into this process); transfer = imported.startTransferSharedTexture()
//            IPC.FRAME (meta, transfer) -> renderer
//   renderer finishTransferSharedTexture -> getVideoFrame -> draw -> close
//            imported.release(cb): cb fires when the RENDERER's GPU command
//            buffer is done with the texture -> IPC.FRAME_DONE {epoch, slot, frameId}
//   main     imported.release() ; E.RELEASE {epoch, slot, frameId}
//   engine   slot LEASED -> FREE
//
//   Why the subtle API and not sendSharedTexture/allReferencesReleased: the
//   managed path signals through a sync token on the MAIN process GPU
//   channel, which is only flushed sporadically — measured lease return
//   latency was several seconds. The renderer flushes every frame, so its
//   release callback is the precise "GPU done reading" event.
//
//   Invariants
//   * unsent/superseded frames return immediately; delivered frames return
//     only after renderer GPU completion. A timeout stops the connection,
//     never reuses a texture still being read.
//   * the engine never blocks: no FREE slot => the frame is not published
//     (stats.skipped); a lease timeout is a terminal connection error;
//   * main keeps at most MAX_IN_FLIGHT frames at the renderer and one queued;
//     a newer frame supersedes the queued one ("latest wins").
//   * rgba textures have no keyed mutex: GPU ordering comes from the lease
//     itself (published only after the engine fence; returned only after the
//     renderer's GPU work on it completed).
//
// ---------------------------------------------------------------------------
// CONTROL <-> FRAME synchronization
// ---------------------------------------------------------------------------
//
//   * INPUT packets carry a strictly increasing `seq`. The engine drains all
//     deltas up to the newest seq it saw before a tick and stamps the frame
//     with `inputSeq`. The renderer therefore knows which UI intent a frame
//     reflects and measures input-to-photon latency = displayAt - sentAt(seq).
//   * VIEWPORT reports device-pixel size; the engine resizes and allocates a
//     new ring with epoch+1 (E.SURFACE). Frames carry {epoch, width, height};
//     the renderer sizes its drawing buffer from the FRAME, never from its own
//     layout, so stale-size frames stay correct during a live resize.
//
// ---------------------------------------------------------------------------

'use strict';

const PROTOCOL_VERSION = 1;

// renderer <-> main channels (Electron IPC)
const IPC = Object.freeze({
  HELLO: 'rvp:hello',        // invoke  R->M  {protocol, caps}            => Session
  CALL: 'rvp:call',          // invoke  R->M  {method, params}            => result | throws
  INPUT: 'rvp:input',        // send    R->M  InputPacket
  VIEWPORT: 'rvp:viewport',  // send    R->M  {width, height, dpr}  (device px)
  FRAME: 'rvp:frame',        // send    M->R  (FrameMeta, SharedTextureTransfer)
  FRAME_DONE: 'rvp:frame-done', // send R->M  {epoch, slot, frameId}  renderer GPU done
  EVENT: 'rvp:event',        // send    M->R  {type: EVT.*, ...}
  REPORT: 'rvp:report',      // send    R->M  string (renderer log line)
});

// main <-> engine-host messages, discriminated by `t`
const E = Object.freeze({
  // main -> engine
  START: 'start',            // {options}
  CALL: 'call',              // {id, method, params}
  INPUT: 'input',            // InputPacket
  RESIZE: 'resize',          // {width, height}
  RELEASE: 'release',        // {epoch, slot, frameId}
  STOP: 'stop',              // {}
  // engine -> main
  HELLO: 'hello',            // {protocol, caps: EngineCaps}
  STATE: 'state',            // {state, message}
  SURFACE: 'surface',        // {epoch, width, height, slots}
  FRAME: 'frame',            // FrameLease
  STATS: 'stats',            // EngineStats (1 Hz)
  RESULT: 'result',          // {id, ok, value | error}
  LOG: 'log',                // {message}
});

// main -> renderer events (IPC.EVENT payload.type)
const EVT = Object.freeze({
  SESSION: 'session',        // Session snapshot (state + reasons + caps)
  SURFACE: 'surface',        // {epoch, width, height, slots}
  STATS: 'stats',            // {engine: EngineStats, transport: TransportStats}
  LOG: 'log',                // {level, message}
});

const SESSION = Object.freeze({
  BOOTING: 'booting',
  WAITING_RENDERER: 'waiting-renderer',
  PROBING: 'probing',
  INITIALIZING: 'initializing',
  RUNNING: 'running',
  STOPPED: 'stopped',
  UNSUPPORTED: 'unsupported',
  ERROR: 'error',
});

// RPC methods (IPC.CALL). `host.*` are served by main, everything else is
// forwarded to the engine as E.CALL.
const METHOD = Object.freeze({
  SET_TURNTABLE: 'scene.setTurntable',  // {enabled: bool}           => {enabled}
  GET_STATS: 'engine.getStats',          // {}                        => EngineStats
  SAVE_CAPTURE: 'host.saveCapture',      // {dataUrl, name?}          => {path, bytes}
  SMOKE_DONE: 'host.smokeDone',          // {dataUrl, summary}        => {}
});

// Shapes (documentation only)
//
// InputPacket  { seq: uint, t: ms (renderer clock), yaw: rad, pitch: rad, dolly: units }
// FrameLease   { epoch, slot, frameId, width, height, inputSeq,
//                handle: decimal string (platform handle valid in main),
//                timestampUs: engine clock, gpuMs: submit->fence }
// FrameMeta    { epoch, slot, frameId, width, height, inputSeq, gpuMs }
// EngineCaps   { supported, reason, platform, handleType: 'ntHandle'|'ioSurface'|'nativePixmap',
//                pixelFormat: 'rgba', protocol }
// EngineStats  { fps, publishFps, width, height, epoch, slots, slotsFree, frames,
//                published, skipped, leaseExpired, inputSeq, turntable, running }
// TransportStats { received, delivered, released, holdMs, dropped, superseded,
//                importFailed, ackTimeout }
// Session      { protocol, state, reasons: string[], message, host, renderer, engine }

// Build SharedTextureImportTextureInfo.handle from a FrameLease handle string.
function textureHandle(handleType, handle) {
  const value = BigInt(handle);
  if (handleType === 'ntHandle' || handleType === 'ioSurface') {
    const buf = Buffer.alloc(8); // Electron requires sizeof(uintptr_t)
    buf.writeBigUInt64LE(value);
    return { [handleType]: buf };
  }
  throw new Error(`handle type '${handleType}' is not implemented by this host`);
}

module.exports = {
  PROTOCOL_VERSION,
  IPC,
  E,
  EVT,
  SESSION,
  METHOD,
  textureHandle,
};
