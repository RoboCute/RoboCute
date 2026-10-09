// Print Chromium's GPU feature status / active GPU for this Electron build:
//   electron scripts/gpu_probe.js
// shared texture requires gpu_compositing = enabled and the SAME adapter the
// engine selects (compare with the engine log "Select device: ...").
'use strict';

const { app } = require('electron');

app.on('child-process-gone', (_e, d) => console.log(`child process gone: ${d.type} ${d.reason} exit=${d.exitCode}`));
app.on('gpu-info-update', () => console.log('gpu-info-update'));

app.whenReady().then(async () => {
  try {
    const info = await app.getGPUInfo('complete');
    console.log('gpu devices:', JSON.stringify(info.gpuDevice, null, 2));
    if (info.auxAttributes) console.log('aux:', JSON.stringify(info.auxAttributes, null, 2));
  } catch (e) {
    console.log('getGPUInfo failed:', e.message);
  }
  console.log('feature status:', JSON.stringify(app.getGPUFeatureStatus(), null, 2));
  app.exit(0);
});
