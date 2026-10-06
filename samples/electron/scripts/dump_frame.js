// Dump one rendered frame to a raw RGBA file for visual inspection.
const path = require('path');
const fs = require('fs');
const REPO_ROOT = path.resolve(__dirname, '..', '..', '..');
const PROGRAM_DIR = path.join(REPO_ROOT, 'src', 'robocute', 'rbc_ext', '_C');
const PROJECT_PATH = path.resolve(REPO_ROOT, '..', 'rbc-project-default');
process.env.PATH = PROGRAM_DIR + path.delimiter + process.env.PATH;
const native = require(path.join(__dirname, '..', 'native', 'build', 'rbc_ext_node.node'));

let n = 0;
native.start(
  { projectPath: PROJECT_PATH, backend: 'dx', programPath: PROGRAM_DIR, width: 1280, height: 720 },
  (frame, error) => {
    if (error) { console.error('ENGINE ERROR:', error); process.exit(2); }
    if (++n === 30) {  // let the path tracer converge a bit
      const out = path.join(__dirname, 'frame.rgba');
      fs.writeFileSync(out, Buffer.from(frame));
      console.log('saved', out, frame.byteLength, 'bytes');
      native.stop();
      setTimeout(() => process.exit(0), 300);
    }
  }
);
setTimeout(() => { console.error('timeout'); process.exit(3); }, 120000);
