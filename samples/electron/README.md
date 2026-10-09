# RoboCute Electron shared texture 视口

唯一画面路径:引擎 GPU 纹理 → Electron `sharedTexture.subtle` → `VideoFrame` → WebGPU `importExternalTexture` → DOM canvas。
无 HWND 子窗口直显，无 CPU readback 画面流，无降级回退。能力缺失时显示 `unsupported` 和原因；运行中 GPU 丢失或租约超时显示 `error` 并停止连接。

## 运行

仓库根目录执行:

```cmd
uv run prepare --electron-ext -y
xmake f -m releasedbg -c
xmake build rbc_ext_node
uv run pre-pack releasedbg
pnpm --dir samples/electron start
pnpm --dir samples/electron smoke
pnpm --dir samples/electron probe
```

若终端继承 `ELECTRON_RUN_AS_NODE=1`，启动 UI 前执行 `set "ELECTRON_RUN_AS_NODE="`。
`RBC_PROGRAM_DIR` 指引擎 DLL/着色器目录，`RBC_PROJECT` 指项目，`RBC_BACKEND` 指后端。
Windows 当前只支持 `dx`；macOS IOSurface、Linux dmabuf 导出尚未实现，明确报告不支持。
`RBC_AUTOCLOSE=<ms>` 用于退出测试，`RBC_SMOKE_TIMEOUT=<ms>` 调整冒烟超时，`RBC_DEVTOOLS=1` 打开开发工具。

左键拖拽相机，滚轮推拉，`T` 切换旋转，`S` 截图，`H` 切换 HTML HUD。
截图从已呈现 canvas 编码 PNG，不是引擎 readback 回退。冒烟测试显示 121 帧，检查非黑画面并保存 `smoke_screenshot.png`，失败返回非零状态。

## 进程与协议

```
renderer/UI + WebGPU
    ↕ contextBridge / Electron IPC
main/session authority + sharedTexture broker
    ↕ child_process IPC
engine-host/ELECTRON_RUN_AS_NODE + N-API
    ↕ command queue / TSFN
engine thread/RBC + LuisaCompute
```

完整通道、消息形状与状态定义在 `protocol.js` (RVP v1)。

| 平面 | 消息 | 同步语义 |
| --- | --- | --- |
| 控制 | HELLO | renderer 先初始化 WebGPU 与帧接收器，主进程验证 GPU 能力后再启动引擎 |
| 控制 | CALL / RESULT | 请求 ID 配对，5 秒超时；旋转开关与引擎统计 RPC |
| 控制 | INPUT | rAF 合并相机增量，递增 `seq`，帧返回已应用 `inputSeq` |
| 控制 | VIEWPORT / RESIZE | 设备像素尺寸，120ms 去抖；重建纹理环时递增 `epoch` |
| 画面 | FRAME | `{epoch,slot,frameId,width,height,inputSeq,gpuMs}` 与 shared texture transfer |
| 画面 | FRAME_DONE / RELEASE | renderer GPU 完成读取后归还对应租约；重复/过期 ticket 不复用新帧 |
| 事件 | SESSION / SURFACE / STATS | 会话状态、纹理尺寸/代际、引擎与传输统计 |

会话: `booting → waiting-renderer → probing → initializing → running → stopped`，失败进入 `unsupported` 或 `error`。

## 帧所有权

1. 引擎从 4 槽纹理环选择 FREE 槽，blit 显示图像，提交 timeline fence。
2. fence 完成后，GPU → LEASED，发送句柄和帧元数据。
3. 主进程 `subtle.importSharedTexture`、`startTransferSharedTexture`，保留引用直到 renderer 确认。
4. preload `finishTransferSharedTexture`，页面调用 `getVideoFrame`、WebGPU 采样并提交，随后 `VideoFrame.close()`。
5. preload `imported.release(callback)`；callback 在 renderer GPU 完成后发送 FRAME_DONE。
6. 主进程释放引用并发送 RELEASE，引擎验证 `{epoch,slot,frameId}` 后 LEASED → FREE。

主进程最多 2 帧在途 + 1 排队；更新帧替换未导入的排队帧。引擎无空槽时跳过发布，不阻塞 UI。
**已发送租约超时不能强制复用。** 主进程 2 秒确认超时、引擎 3 秒租约超时均停止连接。
resize 保留仍有租约的旧环，最后一个租约归还后销毁。renderer 按 FRAME 尺寸绘制，避免布局尺寸与旧帧尺寸混用。

输入延迟统计是发包到 renderer 提交绘制的耗时，不是显示器真实 photon 时间。
`gpuMs` 是引擎提交到 fence 被轮询观察的耗时，含 pacing/轮询延迟，不等于纯 GPU blit 时长。

## Windows 资源互操作

每个槽位两张纹理:
- **staging**:LC 创建的 RGBA8 图像,引擎 `TextureUploader::blit` 写入(LC 管理其屏障)。
- **exported**:在引擎同一适配器(LUID)上用 **D3D11** 创建
  (`BIND_SHADER_RESOURCE|RENDER_TARGET`,`MISC_SHARED|SHARED_NTHANDLE`,无 keyed mutex)
  ——即 Chromium `D3DImageBacking` 自身使用的布局;`IDXGIResource1::CreateSharedHandle`
  得到 NT HANDLE,D3D12 `OpenSharedHandle` 仅供写入,`DuplicateHandle` 进 Electron 主进程。

每帧:LC 主流 blit → signal LC fence → **私有 D3D12 COPY 队列** `Wait(LC fence)` →
`CopyResource(staging → exported)` → signal 拷贝 fence;拷贝 fence 完成后才发布租约。
exported 纹理**从不注册给 LC**,隐式提升/衰减到 COMMON,不录制任何屏障。

> 排障结论(实测):D3D12 直接分配的共享纹理(无论是否 `SIMULTANEOUS_ACCESS`)交给 LC
> 写入/或被 Chromium 采样时,出现黑帧或 NVIDIA Xid 13 GPU 异常 → Chromium GPU 进程
> `exit_code=34`(context lost)。改为 D3D11 分配 + 私有拷贝队列后,可见窗口连续
> 1500+ 帧 0 驱动异常、0 租约超时。

销毁时关闭引擎本地句柄及主进程副本(`DUPLICATE_CLOSE_SOURCE`)。

实测 Canvas2D/WebGL 的 VideoFrame 路径会丢失 GPU context;当前仅 WebGPU
`importExternalTexture`(Electron 官方测试覆盖的路径)。`sharedTexture` 仍为 experimental。

## 验证

`pnpm smoke` 要求:非黑画面、`inputSeq ≥ 1`(输入已反映到帧)、`epoch ≥ 2`(尺寸重建)、
0 次租约超时、0 次 GPU 进程退出,否则非零退出。交互脚本在 `scripts/`(按窗口标题定位):
`capture_app.ps1`、`drag.ps1`、`wheel.ps1`、`send_key.ps1`、`resize_parent.ps1`、`cpu_sample.ps1`;
`probe_engine.js` 打印 addon 导出能力,`gpu_probe.js`(`electron scripts/gpu_probe.js`)打印 Chromium GPU 状态。

平台扩展见 `FUTURE.md`；旧 HWND 实验记录见 `REPORT.md` (历史文档)。

## API 来源

- https://www.electronjs.org/docs/latest/api/shared-texture
- https://www.electronjs.org/docs/latest/api/structures/shared-texture-imported-subtle
- https://www.electronjs.org/docs/latest/api/structures/shared-texture-handle
- https://github.com/electron/electron/blob/main/spec/fixtures/api/shared-texture/common.js
