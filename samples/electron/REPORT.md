# RoboCute Electron Demo 整理与 UI 应用化改造报告

> **历史文档(2026-10-06)。** 文中的 HWND 子窗口直显、readback 模式与布局契约已于
> 2026-10-09 被 sharedTexture 视口 + RVP 协议取代,现行设计见 `README.md` 与 `protocol.js`。

日期:2026-10-06 · 状态:✅ 完成并全量验证

---

## 1. 本次交付总览

| 目标 | 状态 | 说明 |
| --- | --- | --- |
| 项目目录整理 + git 忽略策略 | ✅ | 38 个文件入暂存(+3274/−49),产物全部排除 |
| 原生 3D Viewport 居中 + 前端控件环绕 | ✅ | 工具栏(56px)+ 右侧栏(320px)+ 中央 GPU 直显视口 |
| 截图保存功能 | ✅ | 快捷键 S / 按钮,引擎一次性回读 → PNG 落盘 `screenshots/` |
| 实时帧率展示 | ✅ | 侧栏大字 fps + 分辨率 + 呈现模式 + 引擎状态(1Hz 刷新) |
| 完整 markdown 报告 | ✅ | 本文件 |

### 应用布局(最终形态)

```
┌──────────────────────────────────────────────────────────────┐
│  RoboCute Electron │ GPU直显·shared │ LuisaCompute·dx │ [旋转动画 T] [保存截图 S] │  ← 工具栏 56px
├──────────────────────────────────────────────┬───────────────┤
│                                              │  实时状态      │
│          原生 3D Viewport (DXGI swapchain    │  55.5 fps     │
│          GPU 直显, 左键拖拽/滚轮交互)          │  1044 × 765   │
│                                              │  shared 直显   │
│                                              │  引擎 运行中   │
│                                              ├───────────────┤
│                                              │  操作 / 提示   │
└──────────────────────────────────────────────┴───────────────┘
                                 ↑ 侧栏 320px
```

布局契约:`main.js UI_LAYOUT = { top: 56, right: 320 }` 是唯一真源,同一组
数字传给 addon C++(`computeViewportRect`,原生窗口定位)和 renderer
(CSS 变量,HTML 网格)。改窗口结构时只需改这一处 + 两处消费点。

## 2. 架构:游戏/工具软件的通用 Electron 嵌入骨架

```
renderer (Chromium)          工具栏/侧栏/状态卡 = 普通 Web 技术
  ↑↓ IPC (contextBridge)     细控制信道:截图/转盘/统计/手势
main (Electron browser)      生命周期、HWND 借用、落盘、shutdown 加固
  ↑↓ child_process IPC       start/stop/截图/统计(engine-host.js)
engine-host (RUN_AS_NODE)    纯 Node 运行时宿主 N-API addon
  ↓ 每帧 tick()              LuisaCompute DX12:render → blit → swapchain present
原生 WS_CHILD 视口            DXGI flip-model, GPU 直显, 零 CPU 帧拷贝
```

**应用价值**:所有业务 UI(菜单、属性面板、状态栏、未来的脚本控制台/资源
浏览器)都是普通 HTML/JS;重渲染由原生 swapchain 承担;新增业务控件 =
一个按钮 + 一条 IPC 消息 + (可选)一个引擎命令,渲染路径零改动。

## 3. 新增/变更清单

### 原生 addon(`rbc/extensions/ext_node/src/addon.cpp`)
- `Options.viewportTop/Right` + `computeViewportRect()`:视口让出 UI 区域
- `requestScreenshot()` + `captureScreenshotOnce()`:一次性回读,经 TSFN 以
  `kind='screenshot'` 发出(帧流与截图复用一条信道,第五参数区分)
- `cameraTurntable(bool)`:转盘动画开关(默认开)
- `getStats()`:`{fps,width,height,shared,turntable}` 全原子量,JS 线程安全
- 帧回调签名扩展为 `(frame, error, width, height, kind)`(向后兼容)

### IPC 协议
- main→engine:`{cmd:'screenshot'}`、`{cmd:'turntable',enabled}`
- engine→main:`{type:'stats'(1Hz)}`、`{type:'screenshot',buffer}`
- main→renderer:`stats` / `screenshot-frame` / `screenshot-saved` / `engine-status`
- renderer→main:`ui-screenshot` / `ui-turntable` / `save-screenshot`(PNG dataURL)

### UI(renderer/index.html + renderer.js,纯手写无框架)
- 深色主题三件套:工具栏 / 侧栏卡片 / 中央舞台;CSS 变量消费布局契约
- 截图编码:2D canvas `putImageData`/`drawImage` → `toDataURL` → main 落盘
- 心跳看门狗:stats 断流 5s 显示"无响应";引擎错误/退出实时上屏
- readback 模式保留 WebGL 画布 + JS 手势 + smoke 截图协议

### 项目整理
- 根 `.gitignore` 新增:addon 构建产物、headers 压缩包与解包头、截图/日志
  产物、pre-pack 临时目录;以及三条**否定规则**救出被全局 `debug`/`* .ps1`
  规则误伤的验收脚本
- `native/fetch-deps.ps1`:一键下载 Node headers(v22.18.0 官方 tarball);
  `node.lib`(2KB,难以重新生成)保留 git 追踪
- `scripts/` 分层:冒烟与验收工具留主目录,调试期探针收进 `scripts/debug/`
- 删除遗留产物(frame.png/rgba、各种运行日志);修复旧
  `samples/electron/.gitignore` 误忽略整个 `native/` 的问题

## 4. 验收结果(逐项实测)

| # | 验收项 | 方法 | 结果 |
| --- | --- | --- | --- |
| 1 | 整体 UI 渲染 | `scripts/capture_app.ps1` 整窗截屏 | ✅ 工具栏/侧栏/视口三区正确,55.5fps 实时显示 |
| 2 | 截图保存 | 点击侧栏聚焦 → 按 S | ✅ `screenshots/screenshot-20261006-090648.png`(418KB,1044×765 纯净渲染帧) |
| 3 | 转盘开关 | 按 T → 4s 间隔双帧像素 diff | ✅ diff 0.74/255(≈0.3%,只剩 PT 噪声收敛);开关前旋转明显 |
| 4 | 相机交互 | 视口内合成拖拽 350px | ✅ 视角显著变化(diff 14.3/255 ≈ 5.6%) |
| 5 | resize 跟随 | 父窗 1600×1000 | ✅ stats 变为 1264×905(减 320 侧栏/56 顶栏),fps 稳定 |
| 6 | readback 回归 | `pnpm smoke` / `smoke:electron` | ✅ 792KB 截图,干净退出 |
| 7 | 相机压力回归 | `pnpm smoke:camera` | ✅ 50 轮旋转+缩放,引擎存活 |
| 8 | 干净退出 | `RBC_AUTOCLOSE` + tasklist | ✅ 两种模式均无进程残留 |

### 一次"假崩溃"的排查记录(值得留档)

验收中出现过引擎 fail-fast + Chromium GPU 进程崩溃循环,最终定位到
**测试脚本自身**:PowerShell 变量不区分大小写,`$h`(hwnd IntPtr)覆盖了
`$H`(高度参数),MoveWindow 收到 1278 万像素的天文高度 → Chromium GPU
进程崩溃 → DXGI device removed 连锁反应带崩引擎。合法尺寸下 resize
路径无任何问题。教训已记入 README 已知坑 #10。

## 5. 如何运行 / 复验

```bash
cd D:\ws\repos\RoboCute-repo\RoboCute
xmake build rbc_ext_node && uv run pre-pack     # 改了 rbc 才需要 pre-pack
cd samples/electron
pnpm install                                     # 首次
pnpm start          # 交互:视口拖拽/滚轮 + S 截图 + T 转盘 + 侧栏实时状态
pnpm smoke          # 三层冒烟:addon → IPC → 完整 Electron 离屏截图
```

自动化复验:`samples/electron/README.md` 第"验收方法"节有逐条命令
(capture_app / send_key / drag / wheel / resize_parent / cpu_sample)。

## 6. 遗留事项(非阻塞)

- 引擎线程仍是 ~1 核 100%(PT preview 60fps pacing 内的工作量),后续可按
  需降载(降分辨率/关 denoise)
- 相机复位按钮需要 rbc 侧"绝对设置相机"API(现为相对增量接口)
- HTML 控件叠加在原生视口之上(悬浮工具提示等)需要 DirectComposition
  或透明 layered 窗口,当前未做
- `process.exit` 硬退出对 demo 够用;产品化应回归测试 graceful quit
