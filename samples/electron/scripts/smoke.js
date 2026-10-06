// Headless smoke test for the native addon (no Electron required).
// Usage: node scripts/smoke.js [projectPath]
// Verifies: engine init on the addon thread, frame readback, TSFN delivery.

const path = require('path');
const fs = require('fs');

const REPO_ROOT = path.resolve(__dirname, '..', '..', '..');
const PROGRAM_DIR = process.env.RBC_PROGRAM_DIR ||
  path.join(REPO_ROOT, 'src', 'robocute', 'rbc_ext', '_C');
const PROJECT_PATH = process.argv[2] || process.env.RBC_PROJECT ||
  path.resolve(REPO_ROOT, '..', 'rbc-project-default');
const BACKEND = process.env.RBC_BACKEND || 'dx';

process.env.PATH = PROGRAM_DIR + path.delimiter + process.env.PATH;

const NATIVE_PATH = path.join(__dirname, '..', 'native', 'build', 'rbc_ext_node.node');
if (!fs.existsSync(NATIVE_PATH)) {
  console.error('native addon not built:', NATIVE_PATH);
  console.error('run: pnpm run build:native');
  process.exit(1);
}

console.log('loading addon:', NATIVE_PATH);
const native = require(NATIVE_PATH);

const TOTAL = 90;
let received = 0;
let bytes = 0;
const t0 = Date.now();

native.start(
  { projectPath: PROJECT_PATH, backend: BACKEND, programPath: PROGRAM_DIR, width: 960, height: 540 },
  (frame, error) => {
    if (error) {
      console.error('ENGINE ERROR:', error);
      process.exit(2);
    }
    received++;
    bytes += frame.byteLength;
    if (received === 1) {
      console.log(`first frame after ${Date.now() - t0} ms, ${frame.byteLength} bytes`);
    }
    if (received === TOTAL) {
      const dt = (Date.now() - t0) / 1000;
      console.log(`OK: ${received} frames in ${dt.toFixed(2)}s ` +
        `(${(received / dt).toFixed(1)} fps engine-side, ${(bytes / dt / 1048576).toFixed(0)} MiB/s)`);
      native.stop();
      // give the engine thread a moment, then hard-exit (engine teardown is process exit)
      setTimeout(() => process.exit(0), 500);
    }
  }
);

setTimeout(() => {
  console.error(`TIMEOUT: received ${received}/${TOTAL} frames`);
  console.error('last engine error:', native.getLastError());
  process.exit(3);
}, 120000);
