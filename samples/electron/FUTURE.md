# FUTURE:跨平台显示路线调研与迁移评估

> 状态:调研完成,**暂不实现**。
> 当前原型阶段 Windows 平台层已足够;迁移全平台时基于本文综合评估后重构。
> 调研日期:2026-10-06(Electron 37 实测,sharedTexture API 文档 v44+)

---

## 1. 现状的边界:Windows-only 的精确位置

当前 shared 模式的"窗口嵌入层"是 Win32 专属,**架构其余部分均平台无关**:

| 组件 | 平台相关性 |
| --- | --- |
| UI 壳 / 工具栏 / 侧栏(HTML + contextBridge) | ✅ 跨平台 |
| 引擎子进程模型(`ELECTRON_RUN_AS_NODE` + fork) | ✅ 跨平台 |
| 控制 IPC(start/stop/截图/统计) | ✅ 跨平台 |
| `rbc::set_external_display_handles(uint64, uint64)` | ✅ 两个 uint64 不预设平台语义 |
| addon 视口线程(`CreateWindowExA` + 跨进程 `SetParent`) | ❌ Win32 |
| `computeViewportRect` / `MoveWindow` / `SetWindowPos` / WndProc | ❌ Win32 |
| LC 底层 `CreateSwapChainForHwnd`(DXApi/LCSwapChain.cpp:56) | ❌ DXGI |

结论:移植 = 重写 addon 的平台窗口层(~300 行)+ 选渲染后端,引擎与 UI 零改动。

## 2. 备选路线 A:平台原生窗口直显(当前路线的同构移植)

核心思想不变:**跨进程借用一个原生窗口句柄,引擎在本机图形栈上为它建交换链,每帧 GPU 直显**。

### 2.1 已核实的可行性事实(本仓库源码)

LC 的 **vk 后端 swapchain 原生内置多平台 surface**(`thirdparty/LuisaCompute/src/backends/vk/swapchain.cpp`):

```cpp
// Windows(现状):传 HWND
VkWin32SurfaceCreateInfoKHR ... create_info.hwnd = (HWND)window_handle;

// macOS:传 NSWindow*,LC 自己取 contentView
#elif defined(LUISA_PLATFORM_APPLE)
    VkMacOSSurfaceCreateInfoMVK create_info{};
    create_info.pView = cocoa_window_content_view(window_handle);

// Linux:传 Xlib Window
    VkXlibSurfaceCreateInfoKHR ... vkCreateXlibSurfaceKHR(...);

// Android:ANativeWindow*
```

- mac 分支与 Windows **完全同构**:Electron 侧取 `BrowserWindow` 的 `NSWindow*`(`win.getNativeWindowHandle()` 在 mac 返回 NSView*/NSWindow*),addon 建一个子 `NSView` 挂进视图树,或直接借 NSWindow 的 contentView 几何,把 `NSWindow*` 经 `set_external_display_handles` 传给 rbc → LC vk backend 建 surface → `vkQueuePresentKHR`。
- 渲染后端需切到 **vk**(mac 上经 MoltenVK;LC 已处理)。
- 输入:mac 用 `NSView` 的 `acceptsFirstResponder` + 鼠标事件替代 WndProc。

### 2.2 各平台移植工作量估算

| 平台 | 窗口层 | 渲染后端 | 预计代码 | 风险 |
| --- | --- | --- | --- | --- |
| Windows | ✅ 已完成 | dx | — | — |
| macOS | ObjC++ 子 NSView(~200 行) | vk(MoltenVK) | 中 | MoltenVK 特性差异(PT 渲染管线需回归) |
| Linux | X11 子窗口(~150 行) | vk | 低-中 | Wayland 不兼容(Xlib surface),Wayland 需 dmabuf 路线 |

### 2.3 优点 / 代价

- ✅ 每帧开销 ~0(GPU blit + present),性能上限最高,API 稳定(平台原生栈)
- ✅ 引擎侧(rbc/LC)零改动
- ❌ 每平台一份窗口层代码;UI 叠加原生视口仍需内边距契约/layered 技巧

## 3. 备选路线 B:Electron `sharedTexture` 实验 API(真·纹理共享)

### 3.1 API 形态(Electron v44+,experimental)

"原生纹理句柄 → `VideoFrame`" 的桥,纹理进入 Chromium Web 合成器:

```
engine 产生共享句柄 ─▶ main: sharedTexture.importSharedTexture({textureInfo})
                          ─▶ sharedTexture.sendSharedTexture(frame)
                               ─▶ renderer: setSharedTextureReceiver 回调
                                    ─▶ 作 VideoFrame → canvas drawImage
```

`SharedTextureHandle` 平台三件套:

```cpp
{ ntHandle: Buffer }     // Windows:D3D 共享纹理 NT HANDLE(CreateSharedHandle 产物)
{ ioSurface: Buffer }    // macOS:IOSurfaceRef
{ nativePixmap: { planes: [{fd, stride, offset, size}], modifier } }  // Linux:dmabuf
```

格式:`rgba / bgra / rgbaf16 / nv12 / nv16 / p010le`。
约束:import 在主进程、receiver 在渲染进程、传输 1000ms 超时、
`allReferencesReleased` 回调前必须保持纹理存活(所有权协议)。

### 3.2 已核实的对接点(本仓库源码)

- **LC `Image::native_handle()` 返回后端原生资源**(`luisa/runtime/image.h:174/199`);
  DX 后端即 `ID3D12Resource*`(类型需一次性代码验证);Image 另有
  `external_native_handle` 构造函数 → 双向互操作的门是开的。
- **显示图 `_dst_image` 是 FLOAT4 = RGBA16F**,恰好命中 `rgbaf16`,免格式转换。
- Windows 路径:每帧 `CreateSharedHandle`(轮换 2~3 张)+ 命名共享/跨进程
  `DuplicateHandle` → main import。D3D12 资源可被 Chromium D3D11 设备经
  `OpenSharedResource1` 打开(NT handle 跨 API 共享是系统能力)。

### 3.3 优点 / 代价

- ✅ 一套代码跨三平台(仅句柄分支不同)
- ✅ 纹理是网页元素:CSS 随意叠加/裁剪/变换,不需要布局契约,无 z-order 问题
- ❌ **experimental**(文档明示可能移除)
- ❌ 每帧主进程 import + IPC 传输 + VideoFrame 包装 + canvas 合成,CPU 开销
  显著高于直显;帧率预期 60fps 量级且未实测
- ❌ mac 侧需 LC vk 接 IOSurface external memory(`VK_MVK_iosurface` 等),
  引擎侧改造量大

## 4. 两条路线对比

| | A:原生窗口直显 | B:Electron sharedTexture |
| --- | --- | --- |
| 每帧 CPU 开销 | ~0 | import + IPC + 合成(主进程参与) |
| 帧率上限 | 高(flip-model / vk present) | 受 IPC/合成器限制(未实测) |
| UI 叠加 | 内边距契约 / layered | 天然自由(DOM 合成树) |
| API 稳定性 | 平台原生栈,极稳 | experimental |
| 引擎侧改动 | 无(LC 已内置各平台 surface) | Windows 小改;mac 中-大改 |
| 跨平台代码量 | 每平台 ~200 行窗口层 | 一套 + 平台句柄分支 |
| 维护风险 | 低 | API 报废风险 |

## 5. 建议(供迁移期决策)

**原型阶段(现在):维持 Windows 单平台。** ~~但应立即做一件低成本的事~~
**已完成(2026-10-06)**:addon 的窗口层已收进平台目录,抽象缝就绪——

```
rbc/extensions/ext_node/src/
  addon.cpp               ← 平台无关:引擎线程/循环/IPC/N-API(句柄一律 uint64)
  platform/
    viewport.h            ← 平台无关接口:Viewport / ViewportRect / CameraSink
    win32_viewport.cpp    ← ✅ HWND 子窗口 + WndProc 相机输入(DXGI swapchain 由 RBC 建)
    cocoa_viewport.cpp    ← 空桩(返回 false 并打印提示;实现时改名为 .mm)
    x11_viewport.cpp      ← 空桩(同上)
```

接口集在所有平台上保持可链接(stub 返回 false → addon 报
"platform unsupported",自动引导用户走 readback 模式)。将来移植 macOS 时
只需用 Objective-C++ 填充 `cocoa_viewport.mm` 的 7 个函数。

**迁移期的决策顺序**(届时按优先级评估):

1. **macOS 优先走路线 A 同构移植**(NSWindow* 借用 + LC vk)。理由:LC 已内置
   MVK surface(事实见 §2.1),引擎零改动,性能最好,无实验依赖。
   前置验证:MoltenVK 下 rbc 的 PT 渲染管线/着色器是否全特性通过。
2. **需要"深度 UI 融合"场景**时,在 Windows 上先做路线 B 原型:轮换纹理导出
   NT handle → `importSharedTexture({rgbaf16})` → `drawImage`,与现有 55fps
   直显同机对比帧率/CPU,数据说话再定是否推广。
3. **Linux 桌面**先限定 X11;Wayland 需要 dmabuf 合成器路线,届时单独评估。
4. 路线 B 若要上生产,先向 Electron 上游确认 API 稳定化计划(v44+ 仍标
   experimental),避免绑定一个会被移除的接口。

## 6. 若走路线 B 的 Windows 原型要点(备忘,非承诺)

- 轮换 2~3 张 `_dst_image` 尺寸的 D3D12 committed resource
  (`D3D12_HEAP_FLAG_SHARED`, `ALLOW_RENDER_TARGET`);
- 每帧(或每 N 帧)对当前资源 `CreateSharedHandle`(命名,如
  `Local\rbc-frame-N`),引擎子进程持有,主进程 `OpenSharedHandleByName`
  或 `DuplicateHandle`;
- `textureInfo = { pixelFormat: 'rgbaf16', codedSize: {w,h}, handle: {ntHandle} }`;
  收到 `allReferencesReleased` 才复用该槽位;
- renderer 端 `drawImage(videoFrame)` 到 WebGL/2D canvas;
- **对照指标**:fps、引擎子进程 CPU、主进程 CPU、端到端延迟(鼠标拖拽→画面)。
