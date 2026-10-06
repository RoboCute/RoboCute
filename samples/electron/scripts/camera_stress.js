// Deterministic repro: does feeding camera deltas crash the engine child?
const path = require('path');
const REPO_ROOT = path.resolve(__dirname, '..', '..', '..');
const PROGRAM_DIR = path.join(REPO_ROOT, 'src', 'robocute', 'rbc_ext', '_C');
const PROJECT_PATH = path.resolve(REPO_ROOT, '..', 'rbc-project-default');
process.env.PATH = PROGRAM_DIR + path.delimiter + process.env.PATH;
const native = require(path.join(__dirname, '..', 'native', 'build', 'rbc_ext_node.node'));

let frames = 0;
native.start(
  { projectPath: PROJECT_PATH, backend: 'dx', programPath: PROGRAM_DIR, width: 960, height: 540 },
  (frame, error) => {
    if (error) { console.error('ENGINE ERROR:', error); process.exit(2); }
    frames++;
  }
);
setTimeout(() => {
  console.log('feeding camera deltas... frames so far:', frames);
  for (let i = 0; i < 50; i++) {
    native.cameraRotate(0.02, 0.01);
    native.cameraZoom(i % 2 ? 0.15 : -0.15);
  }
  console.log('deltas fed');
}, 6000);
setTimeout(() => {
  console.log('survived. frames:', frames, 'lastError:', native.getLastError());
  native.stop();
  setTimeout(() => process.exit(0), 400);
}, 14000);
setTimeout(() => { console.error('TIMEOUT frames=', frames); process.exit(3); }, 30000);
