// Preload: minimal bridge between the sandboxed renderer and the main process.
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('rbcDemo', {
  // readback mode: streamed frames
  onFrame(callback) {
    ipcRenderer.on('frame', (_e, buffer, width, height, frameIndex) => {
      callback(buffer, width, height, frameIndex);
    });
  },
  // engine stats feed (1 Hz): {fps, width, height, shared, turntable}
  onStats(callback) {
    ipcRenderer.on('stats', (_e, msg) => callback(msg));
  },
  // shared mode: one-shot capture payload (ArrayBuffer RGBA8)
  onScreenshotFrame(callback) {
    ipcRenderer.on('screenshot-frame', (_e, buffer, width, height) => {
      callback(buffer, width, height);
    });
  },
  onScreenshotSaved(callback) {
    ipcRenderer.on('screenshot-saved', (_e, msg) => callback(msg));
  },
  onEngineStatus(callback) {
    ipcRenderer.on('engine-status', (_e, msg) => callback(msg));
  },
  // UI -> main
  uiScreenshot() {
    ipcRenderer.send('ui-screenshot');
  },
  uiTurntable(enabled) {
    ipcRenderer.send('ui-turntable', enabled);
  },
  saveScreenshot(dataUrl) {
    ipcRenderer.send('save-screenshot', dataUrl);
  },
  // readback mode camera (JS gesture path)
  cameraRotate(yaw, pitch) {
    ipcRenderer.send('camera-rotate', yaw, pitch);
  },
  cameraZoom(dz) {
    ipcRenderer.send('camera-zoom', dz);
  },
  report(msg) {
    ipcRenderer.send('renderer-report', msg);
  },
});
window.addEventListener('error', (e) => {
  ipcRenderer.send('renderer-report', 'window.onerror: ' + e.message + ' @ ' + e.filename + ':' + e.lineno);
});
window.addEventListener('unhandledrejection', (e) => {
  ipcRenderer.send('renderer-report', 'unhandledrejection: ' + (e.reason && e.reason.stack || e.reason));
});
