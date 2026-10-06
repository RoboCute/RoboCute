// Renderer: app UI around the native 3D viewport.
//   shared mode   — viewport is a native child window over #stage; this script
//                   only drives the UI (stats / screenshot / turntable).
//   readback mode — #viewport canvas displays the streamed frames (smoke /
//                   no-HWND fallback), screenshot grabs the canvas directly.

const params = new URLSearchParams(location.search);
const MODE = params.get('mode') === 'readback' ? 'readback' : 'shared';
const SNAPSHOT = params.has('snapshot');
const LAYOUT = {
  top: parseInt(params.get('top') || '56', 10),
  right: parseInt(params.get('right') || '320', 10),
};
document.documentElement.style.setProperty('--topbar-h', LAYOUT.top + 'px');
document.documentElement.style.setProperty('--sidebar-w', LAYOUT.right + 'px');

const $ = (id) => document.getElementById(id);
const canvas = $('viewport');
const placeholder = $('placeholder');
const placeholderText = $('placeholderText');

// --------------------------------------------------------------------------
// mode chrome
// --------------------------------------------------------------------------
$('modeBadge').textContent = MODE === 'shared' ? 'GPU 直显 · shared texture' : 'CPU 回读 · readback';
$('modeBadge').classList.add(MODE);
$('backendBadge').textContent = 'LuisaCompute · dx';
$('modeHint').textContent = MODE === 'shared'
  ? '视口为原生子窗口 (DXGI swapchain 直显)，UI 控件位于工具栏与侧栏。'
  : '无原生视口，帧流经 IPC 由 WebGL canvas 呈现。';

let placeholderState = 'wait'; // wait | ok | error
function setPlaceholder(state, text) {
  placeholderState = state;
  placeholder.style.display = state === 'hidden' ? 'none' : 'flex';
  if (text) placeholderText.textContent = text;
  if (state === 'ok') placeholder.querySelector('.spin').style.display = 'none';
}

// --------------------------------------------------------------------------
// toast
// --------------------------------------------------------------------------
let toastTimer = null;
function toast(msg, isPath) {
  const el = $('toast');
  el.innerHTML = '';
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

// --------------------------------------------------------------------------
// stats sidebar
// --------------------------------------------------------------------------
let lastStatsAt = 0;
let localFrames = 0, localFpsT0 = performance.now();
let localFps = 0;

window.rbcDemo.onStats((s) => {
  const now = performance.now();
  lastStatsAt = now;
  $('fpsValue').textContent = s.fps > 0 ? s.fps.toFixed(1) : (localFps > 0 ? localFps.toFixed(1) : '--');
  $('statRes').textContent = s.width > 0 ? `${s.width} × ${s.height}` : '--';
  $('statMode').textContent = s.shared ? 'shared (GPU 直显)' : 'readback (CPU 回读)';
  setTurntableBtn(s.turntable);
  const st = $('engineStatus');
  st.textContent = '运行中';
  st.className = 'ok';
  if (placeholderState === 'wait') setPlaceholder('hidden');
});

window.rbcDemo.onEngineStatus((s) => {
  const el = $('engineStatus');
  if (s.state === 'error') {
    el.textContent = '错误';
    el.className = 'bad';
    setPlaceholder('error', '引擎错误: ' + (s.message || '').slice(0, 120));
    toast('引擎错误: ' + (s.message || '').slice(0, 80));
  } else if (s.state === 'exited') {
    el.textContent = '已退出 (' + s.code + ')';
    el.className = 'bad';
  }
});

// --------------------------------------------------------------------------
// turntable toggle
// --------------------------------------------------------------------------
let turntableOn = true;
function setTurntableBtn(on) {
  turntableOn = on;
  $('btnTurntable').classList.toggle('on', on);
  $('btnTurn2').classList.toggle('on', on);
}
function toggleTurntable() {
  setTurntableBtn(!turntableOn);
  window.rbcDemo.uiTurntable(turntableOn);
}
$('btnTurntable').addEventListener('click', toggleTurntable);
$('btnTurn2').addEventListener('click', toggleTurntable);

// --------------------------------------------------------------------------
// screenshot pipeline
// --------------------------------------------------------------------------
// shared mode: main asks the engine for a one-shot readback → 'screenshot-frame'
// arrives here → encode PNG → hand back to main for saving.
// readback mode: grab the WebGL canvas directly.
const shotCanvas = document.createElement('canvas');
function encodeAndSave(imageSource, width, height) {
  shotCanvas.width = width;
  shotCanvas.height = height;
  const ctx = shotCanvas.getContext('2d');
  ctx.drawImage(imageSource, 0, 0, width, height);
  window.rbcDemo.saveScreenshot(shotCanvas.toDataURL('image/png'));
}

let shotBusy = false;
function takeScreenshot() {
  if (shotBusy) return;
  if (MODE === 'shared') {
    shotBusy = true; // cleared when the capture payload comes back
    window.rbcDemo.uiScreenshot();
  } else if (gl) {
    encodeAndSave(canvas, canvas.width, canvas.height);
  } else {
    toast('视口尚未就绪');
  }
}
window.rbcDemo.onScreenshotFrame((buffer, width, height) => {
  const img = new ImageData(new Uint8ClampedArray(buffer), width, height);
  shotCanvas.width = width;
  shotCanvas.height = height;
  shotCanvas.getContext('2d').putImageData(img, 0, 0);
  shotBusy = false;
  window.rbcDemo.saveScreenshot(shotCanvas.toDataURL('image/png'));
});
window.rbcDemo.onScreenshotSaved((msg) => {
  shotBusy = false;
  if (msg.ok) toast(msg.path, true);
  else toast('截图保存失败: ' + (msg.message || ''));
});

$('btnScreenshot').addEventListener('click', takeScreenshot);
$('btnShot2').addEventListener('click', takeScreenshot);
window.addEventListener('keydown', (e) => {
  if (e.target.tagName === 'INPUT') return;
  if (e.key === 's' || e.key === 'S') takeScreenshot();
  if (e.key === 't' || e.key === 'T') toggleTurntable();
});

// --------------------------------------------------------------------------
// readback mode: WebGL canvas + JS camera gestures + smoke snapshot
// --------------------------------------------------------------------------
let gl = null;
if (MODE === 'readback') {
  gl = canvas.getContext('webgl2', { antialias: false, preserveDrawingBuffer: true });
  if (!gl) {
    setPlaceholder('error', 'WebGL2 not available');
    window.rbcDemo.report('WebGL2 not available');
  } else {
    canvas.style.display = 'block';
    setPlaceholder('hidden');

    const texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    // Fullscreen triangle, flip Y because engine frame is top-down.
    const vs = gl.createShader(gl.VERTEX_SHADER);
    gl.shaderSource(vs, `#version 300 es
layout(location=0) in vec2 p;
out vec2 uv;
void main() { uv = vec2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5); gl_Position = vec4(p, 0.0, 1.0); }`);
    gl.compileShader(vs);
    const fs = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(fs, `#version 300 es
precision mediump float;
in vec2 uv;
out vec4 o;
uniform sampler2D t;
void main() { o = texture(t, uv); }`);
    gl.compileShader(fs);
    const prog = gl.createProgram();
    gl.attachShader(prog, vs);
    gl.attachShader(prog, fs);
    gl.linkProgram(prog);
    gl.useProgram(prog);

    const vao = gl.createVertexArray();
    gl.bindVertexArray(vao);
    const vb = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vb);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);

    let frames = 0;
    window.rbcDemo.onFrame((buffer, width, height, frameIndex) => {
      try {
        if (canvas.width !== width || canvas.height !== height) {
          canvas.width = width;
          canvas.height = height;
          gl.viewport(0, 0, width, height);
        }
        gl.bindTexture(gl.TEXTURE_2D, texture);
        const pixels = buffer instanceof ArrayBuffer ? new Uint8Array(buffer) : buffer;
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        gl.bindVertexArray(vao);
        gl.drawArrays(gl.TRIANGLES, 0, 3);
        frames++;
        localFrames++;
        const now = performance.now();
        if (now - localFpsT0 >= 1000) {
          localFps = localFrames * 1000 / (now - localFpsT0);
          localFrames = 0;
          localFpsT0 = now;
          if (MODE === 'readback') $('fpsValue').textContent = localFps.toFixed(1);
        }
        // smoke mode: capture straight from the WebGL buffer at frame 120
        if (SNAPSHOT && frames === 120) {
          window.rbcDemo.report('SNAPSHOT:' + canvas.toDataURL('image/png'));
        }
      } catch (e) {
        window.rbcDemo.report('onFrame exception: ' + (e && e.stack || e));
      }
    });

    // JS camera gestures (readback mode only; shared mode uses the native wndproc)
    let dragging = false, lastX = 0, lastY = 0;
    canvas.addEventListener('mousedown', (e) => { dragging = true; lastX = e.clientX; lastY = e.clientY; });
    window.addEventListener('mouseup', () => { dragging = false; });
    window.addEventListener('mousemove', (e) => {
      if (!dragging) return;
      const dx = e.clientX - lastX, dy = e.clientY - lastY;
      lastX = e.clientX; lastY = e.clientY;
      window.rbcDemo.cameraRotate(dx * 0.005, dy * 0.005);
    });
    canvas.addEventListener('wheel', (e) => {
      window.rbcDemo.cameraZoom(Math.sign(e.deltaY) * 0.15);
    }, { passive: true });
  }
} else {
  // shared mode: nothing to draw; the native viewport covers #stage.
  setPlaceholder('wait', '等待引擎初始化…');
}

// engine heartbeat watchdog: if no stats for 5s, surface it
setInterval(() => {
  if (lastStatsAt > 0 && performance.now() - lastStatsAt > 5000 && placeholderState === 'hidden') {
    $('engineStatus').textContent = '无响应';
    $('engineStatus').className = 'bad';
  }
}, 1000);
