// Renderer: RVP UI endpoint.
//   FRAME   imported shared texture -> VideoFrame -> Canvas2D drawImage
//   CONTROL pointer/wheel coalesced per animation frame (INPUT, with seq),
//           canvas device-pixel size (VIEWPORT), RPC buttons (CALL)
//   EVENT   session / surface / stats -> overlay, badges, sidebar
// Protocol: ../protocol.js. No CPU pixel path exists: if the session is
// 'unsupported' the overlay lists the reasons and the viewport stays dark.

'use strict';

const api = window.rvp;
const M = api.method;
const SMOKE = new URLSearchParams(location.search).has('smoke');
const SMOKE_FRAMES = 120;

const $ = (id) => document.getElementById(id);
const canvas = $('viewport');

// ---------------------------------------------------------------------------
// toast
// ---------------------------------------------------------------------------
let toastTimer = null;
function toast(msg, isPath) {
  const el = $('toast');
  el.textContent = '';
  if (isPath) {
    const span = document.createElement('span');
    span.className = 'path';
    span.textContent = msg;
    el.appendChild(span);
  } else {
    el.textContent = msg;
  }
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove('show'), 4000);
}

// ---------------------------------------------------------------------------
// presenter: VideoFrame -> WebGPU importExternalTexture (zero-copy sampling).
// This is the path Electron's sharedTexture is designed and tested against:
// the VideoFrame's release sync token is produced by the WebGPU submit, so
// the renderer's release callback fires exactly when the GPU is done.
// (Canvas2D/WebGL VideoFrame paths were observed to lose the GPU context.)
// ---------------------------------------------------------------------------
let presenter = null;

async function initPresenter() {
  if (!navigator.gpu) return 'navigator.gpu 不可用';
  const adapter = await navigator.gpu.requestAdapter({ powerPreference: 'high-performance' });
  if (!adapter) return 'WebGPU requestAdapter 失败';
  const device = await adapter.requestDevice();
  const context = canvas.getContext('webgpu');
  if (!context) return 'canvas.getContext("webgpu") 失败';
  const format = navigator.gpu.getPreferredCanvasFormat();
  context.configure({ device, format, alphaMode: 'opaque' });

  const module = device.createShaderModule({
    code: `
struct VsOut { @builtin(position) pos: vec4<f32>, @location(0) uv: vec2<f32> };
@vertex fn vs(@builtin(vertex_index) i: u32) -> VsOut {
  let p = array<vec2<f32>, 3>(vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
  var o: VsOut;
  o.pos = vec4(p[i], 0.0, 1.0);
  o.uv = vec2(p[i].x * 0.5 + 0.5, 0.5 - p[i].y * 0.5);
  return o;
}
@group(0) @binding(0) var tex: texture_external;
@group(0) @binding(1) var smp: sampler;
@fragment fn fs(v: VsOut) -> @location(0) vec4<f32> {
  return vec4(textureSampleBaseClampToEdge(tex, smp, v.uv).rgb, 1.0);
}`,
  });
  const layout = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.FRAGMENT, externalTexture: {} },
      { binding: 1, visibility: GPUShaderStage.FRAGMENT, sampler: {} },
    ],
  });
  const pipeline = device.createRenderPipeline({
    layout: device.createPipelineLayout({ bindGroupLayouts: [layout] }),
    vertex: { module, entryPoint: 'vs' },
    fragment: { module, entryPoint: 'fs', targets: [{ format }] },
    primitive: { topology: 'triangle-list' },
  });
  const sampler = device.createSampler({ magFilter: 'linear', minFilter: 'linear' });

  device.lost.then((info) => {
    presenter = null;
    session = { state: 'error', reasons: [], message: `WebGPU device lost: ${info.reason} ${info.message}` };
    renderSession();
    api.report(session.message);
  });

  presenter = {
    // Draw synchronously: the VideoFrame must be closed before the handler
    // returns, which is what lets the renderer release the texture lease.
    draw(videoFrame, width, height) {
      if (canvas.width !== width || canvas.height !== height) {
        canvas.width = width;
        canvas.height = height;
      }
      const bindGroup = device.createBindGroup({
        layout,
        entries: [
          { binding: 0, resource: device.importExternalTexture({ source: videoFrame }) },
          { binding: 1, resource: sampler },
        ],
      });
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [{
          view: context.getCurrentTexture().createView(),
          clearValue: { r: 0, g: 0, b: 0, a: 1 },
          loadOp: 'clear',
          storeOp: 'store',
        }],
      });
      pass.setPipeline(pipeline);
      pass.setBindGroup(0, bindGroup);
      pass.draw(3);
      pass.end();
      device.queue.submit([encoder.finish()]);
    },
    // valid right after draw() in the same task (current texture not yet presented)
    capture() { return canvas.toDataURL('image/png'); },
  };
  return null;
}

// smoke content check: mean RGB of the presented canvas (diagnostic readback)
function meanRgbOfCanvas() {
  const c = document.createElement('canvas');
  c.width = canvas.width;
  c.height = canvas.height;
  const g = c.getContext('2d');
  g.drawImage(canvas, 0, 0);
  const px = g.getImageData(0, 0, c.width, c.height).data;
  let sum = 0;
  for (let i = 0; i < px.length; i += 4) sum += px[i] + px[i + 1] + px[i + 2];
  return sum / (3 * c.width * c.height);
}

// ---------------------------------------------------------------------------
// session / overlay
// ---------------------------------------------------------------------------
let session = { state: 'booting', reasons: [] };
const SESSION_TEXT = {
  'booting': ['正在启动…', ''],
  'waiting-renderer': ['正在连接引擎…', ''],
  'probing': ['正在检测引擎能力…', ''],
  'initializing': ['引擎初始化中…', '首次加载着色器与场景可能需要数秒'],
  'running': ['', ''],
  'stopped': ['引擎已停止', ''],
  'unsupported': ['当前环境不支持 shared texture 视口', '引擎画面需要 GPU 纹理共享;本示例不提供 CPU 回读回退。'],
  'error': ['引擎错误', ''],
};

function renderSession() {
  const s = session.state;
  const badge = $('sessionBadge');
  badge.textContent = s;
  badge.className = 'badge ' + (s === 'running' ? 'ok' : (s === 'unsupported' || s === 'error') ? 'bad' : 'warn');
  const t = $('statSession');
  t.textContent = s;
  t.className = 'v ' + (s === 'running' ? 'ok' : (s === 'unsupported' || s === 'error' || s === 'stopped') ? 'bad' : '');
  const engine = session.engine;
  $('transportBadge').textContent = engine && engine.handleType
    ? `sharedTexture · ${engine.handleType} · ${engine.pixelFormat}`
    : 'sharedTexture';

  const overlay = $('overlay');
  const running = s === 'running';
  if (running && hasFrame) overlay.style.display = 'none';
  else overlay.style.display = 'flex';
  overlay.classList.toggle('bad', s === 'unsupported' || s === 'error' || s === 'stopped');
  const [title, text] = SESSION_TEXT[s] || [s, ''];
  $('overlayTitle').textContent = running ? '等待首帧…' : title;
  $('overlayText').textContent = s === 'error' ? (session.message || '') : text;
  const ul = $('overlayReasons');
  ul.textContent = '';
  for (const r of session.reasons || []) {
    const li = document.createElement('li');
    li.textContent = r;
    ul.appendChild(li);
  }
  for (const id of ['btnTurntable', 'btnTurn2', 'btnScreenshot', 'btnShot2']) $(id).disabled = !running;
}

api.onEvent((evt) => {
  switch (evt.type) {
    case 'session':
      session = evt;
      renderSession();
      if (evt.state === 'running') reportViewport(true);
      break;
    case 'surface':
      $('statSurface').textContent = `${evt.width}×${evt.height} · epoch ${evt.epoch} · ${evt.slots} 槽`;
      break;
    case 'stats':
      onStats(evt.engine, evt.transport);
      break;
  }
});

// ---------------------------------------------------------------------------
// FRAME plane
// ---------------------------------------------------------------------------
let hasFrame = false;
let displayed = 0;
let displayTally = 0, displayT0 = performance.now(), displayFps = 0;
let gpuMs = 0;
let latencyMs = null;
let captureRequest = null; // 'save' | 'smoke'
let lastMeta = null;

api.onFrame(async (imported, meta) => {
  if (!presenter) return;
  const frame = imported.getVideoFrame();
  try {
    presenter.draw(frame, meta.width, meta.height);
  } finally {
    frame.close();
  }
  displayed++;
  if (SMOKE && displayed === 20) {
    await api.call(M.SET_TURNTABLE, { enabled: false });
    const pkt = { seq: ++inputSeq, t: performance.now(), yaw: 0.3, pitch: 0.1, dolly: 0.1 };
    pendingInputs.push({ seq: pkt.seq, t: pkt.t });
    api.input(pkt);
  }
  if (SMOKE && displayed === 40) api.viewport(meta.width - 64, meta.height - 32, window.devicePixelRatio);
  displayTally++;
  lastMeta = meta;
  gpuMs = gpuMs ? gpuMs * 0.9 + meta.gpuMs * 0.1 : meta.gpuMs;
  measureLatency(meta.inputSeq);
  if (!hasFrame) {
    hasFrame = true;
    renderSession();
  }
  if (captureRequest) {
    const kind = captureRequest;
    captureRequest = null;
    const dataUrl = presenter.capture();
    if (kind === 'save') {
      api.call(M.SAVE_CAPTURE, { dataUrl })
        .then((r) => toast(r.path, true))
        .catch((e) => toast('截图保存失败: ' + e.message));
    } else {
      // content check: a black/empty viewport must FAIL the smoke run
      const meanRgb = meanRgbOfCanvas();
      api.call(M.SMOKE_DONE, { dataUrl, summary: { displayed, meanRgb, inputSeq: meta.inputSeq, width: meta.width, height: meta.height, epoch: meta.epoch } });
    }
  }
  if (SMOKE && displayed === SMOKE_FRAMES) captureRequest = 'smoke';
  updateHud();
});

function takeScreenshot() {
  if (session.state !== 'running' || !hasFrame) {
    toast('视口尚未就绪');
    return;
  }
  captureRequest = 'save'; // fulfilled on the next displayed frame
}

// ---------------------------------------------------------------------------
// CONTROL plane: INPUT (coalesced per animation frame, sequenced)
// ---------------------------------------------------------------------------
const ROTATE_PER_PX = 0.005;
const DOLLY_PER_NOTCH = 0.15;
let inputSeq = 0;
let acc = { yaw: 0, pitch: 0, dolly: 0 };
const pendingInputs = []; // [{seq, t}] awaiting a frame that reflects them
let dragging = false, lastX = 0, lastY = 0;

canvas.addEventListener('pointerdown', (e) => {
  if (e.button !== 0) return;
  dragging = true;
  lastX = e.clientX;
  lastY = e.clientY;
  canvas.setPointerCapture(e.pointerId);
  canvas.classList.add('dragging');
  canvas.focus();
});
canvas.addEventListener('pointermove', (e) => {
  if (!dragging) return;
  acc.yaw += (e.clientX - lastX) * ROTATE_PER_PX;
  acc.pitch += (e.clientY - lastY) * ROTATE_PER_PX;
  lastX = e.clientX;
  lastY = e.clientY;
});
const endDrag = (e) => {
  if (!dragging) return;
  dragging = false;
  canvas.classList.remove('dragging');
  if (canvas.hasPointerCapture(e.pointerId)) canvas.releasePointerCapture(e.pointerId);
};
canvas.addEventListener('pointerup', endDrag);
canvas.addEventListener('pointercancel', endDrag);
canvas.addEventListener('wheel', (e) => {
  e.preventDefault();
  acc.dolly += Math.sign(e.deltaY) * DOLLY_PER_NOTCH;
}, { passive: false });

function flushInput() {
  if (acc.yaw || acc.pitch || acc.dolly) {
    if (session.state === 'running') {
      const pkt = { seq: ++inputSeq, t: performance.now(), yaw: acc.yaw, pitch: acc.pitch, dolly: acc.dolly };
      api.input(pkt);
      pendingInputs.push({ seq: pkt.seq, t: pkt.t });
      if (pendingInputs.length > 256) pendingInputs.shift();
    }
    acc = { yaw: 0, pitch: 0, dolly: 0 };
  }
  requestAnimationFrame(flushInput);
}
requestAnimationFrame(flushInput);

// input-to-photon: the oldest unanswered packet the frame now reflects
function measureLatency(frameInputSeq) {
  const now = performance.now();
  let sample = null;
  while (pendingInputs.length && pendingInputs[0].seq <= frameInputSeq) {
    sample = now - pendingInputs.shift().t;
  }
  if (sample !== null) latencyMs = latencyMs === null ? sample : latencyMs * 0.8 + sample * 0.2;
}

// ---------------------------------------------------------------------------
// CONTROL plane: VIEWPORT (device pixels; engine re-allocates the ring)
// ---------------------------------------------------------------------------
let vpSize = { width: 0, height: 0 };
let vpReported = { width: 0, height: 0 };
let vpTimer = null;

function measureViewport(entry) {
  const dpr = window.devicePixelRatio || 1;
  const box = entry && entry.devicePixelContentBoxSize && entry.devicePixelContentBoxSize[0];
  if (box) return { width: box.inlineSize, height: box.blockSize, dpr };
  const r = canvas.getBoundingClientRect();
  return { width: Math.round(r.width * dpr), height: Math.round(r.height * dpr), dpr };
}

function reportViewport(force) {
  if (!vpSize.width || !vpSize.height) return;
  if (!force && vpSize.width === vpReported.width && vpSize.height === vpReported.height) return;
  vpReported = { ...vpSize };
  api.viewport(vpSize.width, vpSize.height, vpSize.dpr);
}

new ResizeObserver(([entry]) => {
  vpSize = measureViewport(entry);
  // trailing debounce: every resize re-allocates GPU textures engine-side
  clearTimeout(vpTimer);
  vpTimer = setTimeout(() => reportViewport(false), vpReported.width ? 120 : 0);
}).observe(canvas, { box: 'device-pixel-content-box' });

// ---------------------------------------------------------------------------
// stats / HUD
// ---------------------------------------------------------------------------
function fmt(n, d = 1) { return Number.isFinite(n) ? n.toFixed(d) : '--'; }

function onStats(engine, transport) {
  const now = performance.now();
  displayFps = displayTally * 1000 / (now - displayT0);
  displayTally = 0;
  displayT0 = now;
  $('fpsDisplay').textContent = fmt(displayFps);
  $('fpsEngine').textContent = fmt(engine.fps);
  $('statLatency').textContent = latencyMs === null ? '--' : `${fmt(latencyMs)} ms`;
  $('statGpu').textContent = gpuMs ? `${fmt(gpuMs, 2)} ms` : '--';
  $('statSlots').textContent = `${engine.slotsFree} / ${engine.slots}`;
  $('statDelivered').textContent = `${transport.delivered} / ${engine.published}`;
  $('statSkipped').textContent = String(engine.skipped);
  $('statDropped').textContent = `${transport.dropped + transport.importFailed} / ${transport.superseded}`;
  $('statAck').textContent = `${fmt(transport.holdMs)} ms · 超时 ${transport.ackTimeout}`;
  $('statExpired').textContent = String(engine.leaseExpired);
  setTurntableBtn(engine.turntable);
  updateHud();
}

let hudOn = false;
function updateHud() {
  const hud = $('hud');
  hud.style.display = hudOn && hasFrame ? 'block' : 'none';
  if (!hudOn || !lastMeta) return;
  hud.textContent = `frame ${lastMeta.frameId}  ${lastMeta.width}×${lastMeta.height}  epoch ${lastMeta.epoch}/slot ${lastMeta.slot}\n` +
    `display ${fmt(displayFps)} fps  input→photon ${latencyMs === null ? '--' : fmt(latencyMs) + ' ms'}  inputSeq ${lastMeta.inputSeq}/${inputSeq}`;
  hud.style.whiteSpace = 'pre';
}

// ---------------------------------------------------------------------------
// RPC buttons + shortcuts
// ---------------------------------------------------------------------------
let turntableOn = true;
function setTurntableBtn(on) {
  turntableOn = on;
  $('btnTurntable').classList.toggle('on', on);
  $('btnTurn2').classList.toggle('on', on);
}
function toggleTurntable() {
  if (session.state !== 'running') return;
  const want = !turntableOn;
  api.call(M.SET_TURNTABLE, { enabled: want })
    .then((r) => setTurntableBtn(r.enabled))
    .catch((e) => toast('切换失败: ' + e.message));
}
function toggleHud() {
  hudOn = !hudOn;
  $('btnHud').classList.toggle('on', hudOn);
  updateHud();
}

$('btnTurntable').addEventListener('click', toggleTurntable);
$('btnTurn2').addEventListener('click', toggleTurntable);
$('btnScreenshot').addEventListener('click', takeScreenshot);
$('btnShot2').addEventListener('click', takeScreenshot);
$('btnHud').addEventListener('click', toggleHud);
window.addEventListener('keydown', (e) => {
  if (e.repeat || e.target.tagName === 'INPUT') return;
  const k = e.key.toLowerCase();
  if (k === 's') takeScreenshot();
  else if (k === 't') toggleTurntable();
  else if (k === 'h') toggleHud();
});

// ---------------------------------------------------------------------------
// handshake: receiver is registered (preload) -> report viewport -> HELLO
// ---------------------------------------------------------------------------
vpSize = measureViewport(null);
reportViewport(true);
initPresenter().catch((e) => e.message).then((reason) => api.hello({
  protocol: api.protocol, sharedTexture: api.sharedTextureAvailable, webgpu: !!presenter, reason,
})).then((s) => { session = s; renderSession(); })
  .catch((e) => {
    session = { state: 'error', reasons: [], message: '连接失败: ' + e.message };
    renderSession();
  });
renderSession();
