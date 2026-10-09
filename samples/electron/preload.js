// Preload: RVP renderer endpoint. Owns the received texture's lifetime (the
// FRAME_DONE ack is sent only once THIS process's GPU work on it completed)
// and exposes a narrow API to the page via contextBridge. Protocol: protocol.js.

'use strict';

const { contextBridge, ipcRenderer, sharedTexture } = require('electron');
const { PROTOCOL_VERSION, IPC, METHOD } = require('./protocol');

let frameHandler = null;
const subtle = sharedTexture && sharedTexture.subtle;
const sharedTextureAvailable = !!(subtle && typeof subtle.finishTransferSharedTexture === 'function');

if (sharedTextureAvailable) {
  // Installed before HELLO, so main never sends to a missing handler.
  ipcRenderer.on(IPC.FRAME, async (_e, meta, transfer) => {
    const ack = { epoch: meta.epoch, slot: meta.slot, frameId: meta.frameId };
    let imported = null;
    try {
      imported = subtle.finishTransferSharedTexture(transfer);
      // handler draws synchronously and closes the VideoFrame before returning
      if (frameHandler) await frameHandler({ getVideoFrame: () => imported.getVideoFrame() }, meta);
    } catch (e) {
      ipcRenderer.send(IPC.REPORT, 'frame handler error: ' + (e && e.stack || e));
    } finally {
      // callback = this renderer's GPU work on the texture has completed
      if (imported) imported.release(() => ipcRenderer.send(IPC.FRAME_DONE, ack));
      else ipcRenderer.send(IPC.FRAME_DONE, ack);
    }
  });
}

contextBridge.exposeInMainWorld('rvp', {
  protocol: PROTOCOL_VERSION,
  method: METHOD,
  sharedTextureAvailable,
  // handler({getVideoFrame}, meta): draw synchronously, then frame.close()
  onFrame(handler) { frameHandler = handler; },
  onEvent(callback) { ipcRenderer.on(IPC.EVENT, (_e, evt) => callback(evt)); },
  hello(caps) { return ipcRenderer.invoke(IPC.HELLO, { protocol: PROTOCOL_VERSION, caps }); },
  call(method, params) { return ipcRenderer.invoke(IPC.CALL, { method, params }); },
  input(packet) { ipcRenderer.send(IPC.INPUT, packet); },
  viewport(width, height, dpr) { ipcRenderer.send(IPC.VIEWPORT, { width, height, dpr }); },
  report(line) { ipcRenderer.send(IPC.REPORT, String(line)); },
});

window.addEventListener('error', (e) => {
  ipcRenderer.send(IPC.REPORT, 'window.onerror: ' + e.message + ' @ ' + e.filename + ':' + e.lineno);
});
window.addEventListener('unhandledrejection', (e) => {
  ipcRenderer.send(IPC.REPORT, 'unhandledrejection: ' + (e.reason && e.reason.stack || e.reason));
});
