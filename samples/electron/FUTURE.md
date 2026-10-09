# FUTURE:sharedTexture 视口的跨平台路线

> 状态(2026-10-09):**路线 B(Electron sharedTexture)已落地为唯一显示路径**,
> Windows/D3D12 已实现;旧路线 A(HWND 子窗口 + DXGI swapchain 直显)与
> readback 回退均已删除。本文记录各平台剩余工作与风险。
> API 参考:https://www.electronjs.org/docs/latest/api/shared-texture
> (Electron v40 引入,v41 加 nv12,v42.10/v43.4.1/v44.0 有崩溃修复;本示例锁 44)

---

## 1. 平台相关性边界

| 组件 | 平台相关性 |
| --- | --- |
| RVP 协议、会话状态机、帧租约(`protocol.js`/`main.js`) | ✅ 跨平台 |
| renderer(WebGPU + VideoFrame)、preload receiver | ✅ 跨平台 |
| 引擎子进程模型(`ELECTRON_RUN_AS_NODE` + fork) | ✅ 跨平台 |
| addon 引擎线程、纹理环、fence、租约(`addon.cpp`) | ✅ 跨平台(仅用 LC 抽象 + `NativeResourceExt`) |
| `platform/shared_surface_*.cpp`:分配可导出纹理 + 把句柄放进主进程 | ❌ 每平台一份 |
| `protocol.js textureHandle()`:句柄字符串 → `SharedTextureHandle` | 每平台一个分支 |

**移植一个平台 = 实现 `shared_surface.h`(纹理导出 5 个 + 拷贝器 5 个函数)+ `textureHandle` 一个分支。**
其余代码零改动。未实现的平台由 `surfaceCaps()` 返回原因,UI 显示 `unsupported`。

## 2. Windows(已完成)

- D3D11(同 LUID)创建 `MISC_SHARED|SHARED_NTHANDLE` RGBA8 纹理 → `CreateSharedHandle`
  → D3D12 `OpenSharedHandle`(仅私有 COPY 队列写入)→ `DuplicateHandle` 进主进程 → `{ ntHandle }`。
- 引擎 blit 到 LC 自有 staging 纹理;COPY 队列等待 LC fence 后 `CopyResource` 到导出纹理。
  导出纹理不进入 LC 屏障跟踪(D3D12 直接分配 / 交给 LC 写入会触发黑帧或 Xid 13,见 README)。
- 槽位销毁时 `DuplicateHandle(..., DUPLICATE_CLOSE_SOURCE)` 远程关闭主进程中的副本。
- 约束:引擎与 Chromium GPU 进程必须在同一适配器(LUID)。可选改进:主进程从
  `app.getGPUInfo('complete')` 取 Chromium 适配器 LUID 经 `E.START` 下发,引擎
  按 LUID 选卡。

## 3. macOS(待实现)

- 句柄:`IOSurfaceRef`,`{ ioSurface: Buffer(8) }`。Electron import 时 RETAIN,
  **IOSurface 也是进程本地的**:非 global 的 IOSurface 跨进程需要 mach port 传递。
  两个可选方案:
  1. 引擎子进程创建 IOSurface,用 `IOSurfaceCreateMachPort` + 一条 mach IPC
     (或 XPC)送到主进程,主进程侧需要一个极小的 N-API helper 做
     `IOSurfaceLookupFromMachPort`;
  2. 把引擎改为在主进程内的 utility 线程运行(放弃进程隔离)。
  倾向方案 1(保留崩溃隔离)。
- 渲染后端:LC metal 后端(若可用)直接 `newTextureWithDescriptor:iosurface:plane:`;
  或 vk(MoltenVK)+ `VK_EXT_metal_objects` 导入 IOSurface。
- `pixelFormat: 'bgra'` 可能是 IOSurface 的更自然格式(`'BGRA'` fourcc)。

## 4. Linux(待实现)

- 句柄:dmabuf,`{ nativePixmap: { planes:[{fd,stride,offset,size}], modifier } }`。
  fd 同样是进程本地:引擎子进程需经 `SCM_RIGHTS`(unix socket)把 fd 送到主进程;
  Node `child_process` 的 IPC 通道不传 fd,需要额外 socket + 小型 N-API helper。
- LC vk 后端 + `VK_EXT_external_memory_dma_buf` + `VK_EXT_image_drm_format_modifier`
  导出;modifier 需与 Chromium(GBM)协商,先用 `DRM_FORMAT_MOD_LINEAR` 保底。
- Wayland/X11 无差异(纹理进合成器,不涉及窗口系统)——这是相对旧路线 A 的
  一大优势。

## 5. 性能与风险

| 项 | 现状 / 说明 |
| --- | --- |
| 每帧 CPU | 主进程:subtle import/transfer + release;renderer:WebGPU external texture 采样,不上传 CPU 像素 |
| 帧率 | 受引擎 pacing(60)与 renderer 合成限制;侧栏显示引擎/显示 fps、跳帧、合并 |
| 延迟 | 侧栏统计输入发包→绘制提交;4 槽纹理环、2 帧在途 + 1 排队 |
| API 稳定性 | **experimental**,可能变更;所有调用集中在 `main.js deliver()` 与 `preload.js`,替换面小 |
| 可优化 | 引擎側 blit 与 tick 合并到同一 cmdlist;适配器显式协商 |
