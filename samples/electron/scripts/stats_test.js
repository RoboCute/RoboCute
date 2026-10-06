const path = require('path');
const REPO_ROOT = path.resolve(__dirname, '..', '..', '..');
const PROGRAM_DIR = path.join(REPO_ROOT, 'src', 'robocute', 'rbc_ext', '_C');
process.env.PATH = PROGRAM_DIR + path.delimiter + process.env.PATH;
const native = require(path.join(__dirname, '..', 'native', 'build', 'rbc_ext_node.node'));
native.start(
  { projectPath: path.resolve(REPO_ROOT, '..', 'rbc-project-default'), backend: 'dx', programPath: PROGRAM_DIR, width: 640, height: 360 },
  () => {}
);
setTimeout(() => { console.log('stats@6s:', JSON.stringify(native.getStats())); }, 6000);
setTimeout(() => { console.log('stats@9s:', JSON.stringify(native.getStats())); native.stop(); setTimeout(()=>process.exit(0),300); }, 9000);
