// Print the native addon's texture-export capabilities without Electron:
//   node scripts/probe_engine.js            (RBC_BACKEND / RBC_PROGRAM_DIR honored)
'use strict';

const path = require('path');

const REPO_ROOT = path.resolve(__dirname, '..', '..', '..');
const PROGRAM_DIR = process.env.RBC_PROGRAM_DIR ||
  path.join(REPO_ROOT, 'src', 'robocute', 'rbc_ext', '_C');
process.env.PATH = PROGRAM_DIR + path.delimiter + process.env.PATH;

const native = require(path.join(__dirname, '..', 'native', 'build', 'rbc_ext_node.node'));
const caps = native.capabilities(process.env.RBC_BACKEND || 'dx');
console.log(JSON.stringify(caps, null, 2));
process.exit(caps.supported ? 0 : 1);
