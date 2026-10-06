# RoboCute Electron Demo

在 Electron 窗口里跑 LuisaCompute 渲染,Node.js 等价物 of `samples/app_graphics_scene.py`。

## 两种显示模式

### `shared`(默认,交互式)— GPU 直显,零 CPU 回读

```
renderer (Chromium, 工具栏+侧栏 UI; 中央区留给原生视口)
main (Electron browser process)                    ← main.js
   ^ 控制 IPC(start/stop/截图/转盘/统计);不传帧
engine-host (ELECTRON_RUN_AS_NODE 纯 Node 运行时)   ← engine-host.js
   ^ rbc_ext_node.node → rbc_core / LuisaCompute (DX12)
        │  tick(): render → blit → DXGI swapchain present
        ▼
原生子窗口 (WS_CHILD, 覆盖客户区减去 UI 内边距, HWND 跨进程借用)
```

**应用布局**(布局契约:`main.js UI_LAYOUT = { top: 56, right: 320 }`,同一组
数字传给 addon(视口定位)和 renderer(CSS 变量)):

- 顶部工具栏(56px):品牌 / 模式徽章 / 旋转动画开关 [T] / 保存截图 [S]
- 右侧侧栏(320px):实时状态卡(fps 大字、分辨率、呈现模式、引擎状态)、
  操作卡、交互提示卡
- 中央:原生 3D 视口(拖拽环绕相机 / 滚轮推拉,原生 WndProc 直接处理)

**截图管线**(shared 模式):UI → main 发 `{cmd:'screenshot'}` → 引擎下一帧做
一次性 CPU 回读 → TSFN 以 `kind='screenshot'` 回调 → engine-host 转发
`{type:'screenshot'}` → renderer 用 2D canvas 编码 PNG → main 落盘到
`screenshots/screenshot-<时间戳>.png`。readback 模式则直接抓 WebGL canvas。

**统计管线**:engine-host 每 1s 轮询 `native.getStats()`
(`{fps,width,height,shared,turntable}`)转发给侧栏;另有 5s 心跳看门狗。

- 渲染目标 `_dst_image` 经 GPU blit 到 swapchain backbuffer,flip-model 直显,
  **每帧 0 字节过 CPU**(对比 readback 模式 110fps 时 ~235 MiB/s 的下载+上传)。
- 相机输入由子窗口原生 WndProc 处理(拖拽旋转/滚轮缩放),不经过 IPC。
- 窗口 resize:引擎线程轮询父客户区 → `RBCContext::reset_view` 重建 swapchain。
- 模式选择:`start(options)` 传 `present:'shared'` + `parentHwnd`(十进制字符串,
  见 main.js `nativeWindowHandle`)。

### `readback`(`--smoke` / 无 HWND 时)— CPU 回读 + IPC 帧流

```
engine 线程 TextureDownloadCommand → RGBA8 → TSFN → IPC Buffer → WebGL canvas
```

用于无头冒烟/截图(`electron . --smoke` 输出 `smoke_screenshot.png`),以及
旧的鼠标手势路径(renderer.js → preload → ipcMain → 引擎)。

## 运行

```bash
cd samples/electron
pnpm install                 # 首次:electron + node-api-headers
xmake build rbc_ext_node     # 仓库根目录;产物在 native/build/
uv run pre-pack              # 同步新 DLL 到 src/robocute/rbc_ext/_C(改了 rbc 后必须!)
pnpm start                   # shared 模式:可见窗口 + GPU 直显视口,原生鼠标交互
pnpm smoke                   # addon 直跑冒烟(回读)
pnpm smoke:host              # engine-host IPC 链冒烟
pnpm smoke:electron          # 完整 Electron 离屏冒烟(截图 + 自退)
node scripts/camera_stress.js # 相机 API 压力测试(曾用于复现 fail-fast)
```

环境变量:`RBC_PROGRAM_DIR`、`RBC_PROJECT`、`RBC_BACKEND`、`RBC_AUTOCLOSE=<ms>`
(自动关窗,验证退出路径)、`RBC_DISABLE_HW_ACCEL=1`。

## 验收方法(shared 模式)

```bash
cd samples/electron
RBC_AUTOCLOSE=120000 node node_modules/electron/cli.js . > acc_run.txt 2>&1 &
sleep 15   # 干净系统上 ~1-3s 出首帧;若刚 kill 过上实例,GPU 清理可能耗时 30s+
# 1. 整体 UI 截屏(自动从日志读 viewport hwnd、置顶父窗、按父窗矩形截屏)
powershell -ExecutionPolicy Bypass -File scripts/capture_app.ps1 -Log acc_run.txt -Out cap.png
# 2. 仅视口截屏
powershell -ExecutionPolicy Bypass -File scripts/capture_rect.ps1 -Log acc_run.txt -Out vp.png
# 3. UI 快捷键:先点一下侧栏聚焦 Chromium(原生视口会吃键盘),再发按键
powershell -ExecutionPolicy Bypass -File scripts/drag.ps1 -X1 2200 -Y1 400 -X2 2200 -Y2 400
powershell -ExecutionPolicy Bypass -File scripts/send_key.ps1 -Key S -Log acc_run.txt   # 截图落盘
powershell -ExecutionPolicy Bypass -File scripts/send_key.ps1 -Key T -Log acc_run.txt   # 转盘开关
# 4. 相机:合成鼠标拖拽 / 滚轮,前后截屏对比视角
powershell -ExecutionPolicy Bypass -File scripts/drag.ps1 -X1 1400 -Y1 700 -X2 1750 -Y2 560
# 5. resize:父窗缩放到 1600x1000,stats 里 viewport 尺寸应跟随(1264x905)
powershell -ExecutionPolicy Bypass -File scripts/resize_parent.ps1 -W 1600 -H 1000 -Log acc_run.txt
# 6. 性能: 日志 "stats #N: fps=.. WxH"; CPU 采样 scripts/cpu_sample.ps1
# 7. 退出:等 autoclose,tasklist 无 electron 残留
```

验收基线(2026-10-06,RTX 5070 Ti):~55 fps(60fps 目标 pacing,PT preview 负载)、
引擎子进程 ~1 核、WS ~1.8GB(readback 模式 ~3.2GB,另加 235 MiB/s 帧流量)。

## 已知坑(全部实测踩过)

1. **shutdown 重入死锁**:shutdown 时 Chromium 销毁窗口 → `window-all-closed`
   再触发 → handler 里又 `app.quit()` → browser 进程死锁且 `taskkill /F` 都杀不掉。
   解法:单一 `shuttingDown` 保护的 stopAll,只走 deferred `process.exit`;
   Electron 37 里 `process.exit()` 会**立即返回**(延迟生效),其后不要放清理代码。
2. **Chromium "Intermediate D3D Window" 遮挡**:客户区实际由 Chromium 的
   合成器子窗口(与我们的 WS_CHILD 同级)覆盖,z-order 垫底时看不到直显画面。
   解法:创建时 + 每次 resize + 每 60 帧 `SetWindowPos(HWND_TOP)` 重断言。
3. **headless 下 `control_camera_add_*` 必然崩**:`cam_controller` 仅在
   `enable_camera_control` 且有 LC 窗口时创建;未启用时 `LUISA_ERROR` 抛异常
   穿过 N-API 边界 → `std::terminate` → fail-fast(0xC0000409),fail-fast 不
   过 VEH,且线程死亡会连带销毁其创建的 viewport 窗口。解法:rbc 侧允许无窗口
   启用相机控制(tick 用中性输入驱动 `_update`),`CameraController` 增加
   external-change 标记驱动 path-tracing 累积重置。
4. **VEH 要过滤 `DBG_PRINTEXCEPTION_C(0x406d1388)`**:否则 OutputDebugString
   也会触发崩溃转储(全内存 dump 3.5GB)。
5. **contextIsolation 丢 TypedArray**:IPC 传帧必须 ArrayBuffer。
6. **IPC 必须 `serialization:'advanced'`**:JSON 模式把 Buffer 炸成数字数组。
7. **MSYS bash 里 taskkill 用单斜杠**(`/F /IM`);`//F` 被原样传递并报错。
8. **刚 kill 掉引擎实例后立即重启,init 可能耗时 30s+**(GPU/驱动清理);
   干净系统上 init→首帧 ~1-3s。测量时注意 stdout 重定向缓冲会扭曲读数。
9. addon 用 `/delayload:node.exe` + hook,一份二进制同时适配 Node/Electron;
   N-API 是 stable ABI。
10. **PowerShell 变量不区分大小写**:`$H` 和 `$h` 是同一个变量,写工具脚本时
    命名注意(曾因此把 hwnd 传给 MoveWindow 的高度参数,Chromium 尝试创建
    千万像素表面导致 GPU 进程崩溃循环,并连锁 DXGI device-removed 崩掉
    引擎)。
11. **原生视口会抢键盘焦点**:UI 快捷键要先确保点击的是 HTML 区域(工具栏/
    侧栏),否则按键进了原生 wndproc。
12. **布局契约是三个副本**:addon C++(`computeViewportRect`)、main.js
    (`UI_LAYOUT`)、renderer(CSS 变量)必须用同一组 inset 数字;改窗口
    结构时三处同步。

## 文件

| 路径 | 说明 |
| --- | --- |
| `../../rbc/extensions/ext_node/` | N-API addon(两种 present 模式;平台无关) |
| `../../rbc/extensions/ext_node/src/platform/` | 视口平台层:`viewport.h` 接口 + win32 实现 + mac/linux 空桩(见 FUTURE.md) |
| `../../rbc/sample_graphics/` | embedding 补丁:`external_display.h`、无窗口相机控制(tick/enable_camera_control) |
| `../../rbc/runtime/` | `CameraController::notify_external_change/any_changed` |
| `engine-host.js` | 引擎宿主子进程(init 计时探针 + 1Hz stats 轮询) |
| `main.js` | Electron main:fork 引擎、传 HWND/UI_LAYOUT、截图落盘、shutdown 加固 |
| `preload.js` / `renderer/` | contextBridge + 工具栏/侧栏 UI + readback WebGL 画布 + 截图编码 |
| `scripts/` | 冒烟(smoke/smoke_host/camera_stress/stats_test)+ 验收工具(capture_app/capture_rect/drag/wheel/send_key/resize_parent/cpu_sample/dump_frame) |
| `scripts/debug/` | 调试期探针(tree/inspect_child/probe_hwnd/list_windows/capture_window/capture_screen) |
| `native/deps/` | node.lib(N-API import lib,已追踪);headers 用 `fetch-deps.ps1` 下载 |

Git 忽略策略见根 `.gitignore`:addon 构建产物、node_modules、headers 压缩包、
截图/日志产物全部忽略;`node.lib` 因难以重新生成而被追踪。

## 应用价值

这个结构(原生 GPU 视口 + 周围 Web UI 控件)就是游戏引擎/工具软件嵌入
Electron 的通用骨架:所有业务 UI(菜单/属性面板/状态栏/脚本控制台)
都是普通 Web 技术,而重渲染由原生 swapchain 直显承担,二者通过一条
细控制信道(N-API + IPC)通信。新增一个业务控件 = 加一个 HTML 按钮 +
一条 IPC 消息 + (可选)一个引擎侧命令,无需碰渲染路径。
