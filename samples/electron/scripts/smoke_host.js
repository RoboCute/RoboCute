// Drive engine-host.js as a child process (same as main.js does, but under
// plain Node) to validate the IPC protocol chain without Electron.
const { fork } = require('child_process');
const path = require('path');

const REPO_ROOT = path.resolve(__dirname, '..', '..', '..');
const PROGRAM_DIR = path.join(REPO_ROOT, 'src', 'robocute', 'rbc_ext', '_C');
const PROJECT_PATH = path.resolve(REPO_ROOT, '..', 'rbc-project-default');

const t0 = Date.now();
let frames = 0, bytes = 0;
const child = fork(path.join(__dirname, '..', 'engine-host.js'), [], { silent: false });

child.on('message', (msg) => {
  if (msg.type === 'error') { console.error('ENGINE ERROR:', msg.message); process.exit(2); }
  if (msg.type === 'ready') {
    child.send({ cmd: 'start', options: {
      projectPath: PROJECT_PATH, backend: 'dx', programPath: PROGRAM_DIR,
      width: 960, height: 540,
    }});
  }
  if (msg.type === 'frame') {
    frames++; bytes += msg.buffer.length;
    if (frames === 1) console.log(`first frame after ${Date.now() - t0} ms`);
    if (frames === 60) {
      const dt = (Date.now() - t0) / 1000;
      console.log(`OK: ${frames} frames, ${(bytes / dt / 1048576).toFixed(0)} MiB/s via IPC`);
      child.send({ cmd: 'stop' });
      setTimeout(() => process.exit(0), 400);
    }
  }
});
child.on('exit', (c) => { console.log('host exited', c); process.exit(frames >= 60 ? 0 : 3); });
setTimeout(() => { console.error(`TIMEOUT frames=${frames}`); process.exit(3); }, 90000);
