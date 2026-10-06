# RoboCute 作为 DeepSeek Harness Desktop 插件的可行性分析

> 结论先行:**可以写成 dsh 插件,但"Electron 样本同款 HWND 直显嵌入"在纯第三方范围内做不到产品级;
> 有三条路径,推荐"引擎独立进程 + WebSocket 帧流 + 一等公民 Web 面板"(今天就能落地),
> 产品级 HWND 同步嵌入需要 DeepSeek 官方把壳里已有的 PlatformView 机制通用化。**

---

## 1. 两边源码的关键事实

### 1.1 RoboCute 侧(samples/electron + rbc/extensions/ext_node)

| 事实 | 证据 |
| --- | --- |
| addon 导出 9 个函数:`start/stop/cameraRotate/cameraZoom/cameraTurntable/requestScreenshot/getStats/getLastError/preloadRuntimeDlls` | `rbc/extensions/ext_node/src/addon.cpp:827-841` |
| `start(options)`:`present:"shared"` 要求 `parentHwnd`(十进制字符串);`viewportTop/viewportRight` 为布局 inset(参数化,C++ 不硬编码 56/320) | `addon.cpp:698-715`, `win32_viewport.cpp:28-39` |
| 视口层**只有 WS_CHILD 一条路径**,`CreateWindowExA(..., WS_CHILD, (HWND)parent_handle, ...)`,文件头注释明写 "parented (cross-process) to the Electron browser window" | `win32_viewport.cpp:75-112` |
| 跨进程零进程归属检查(无 `GetWindowThreadProcessId` 类校验);WndProc 由 addon 自己的视口线程泵消息,不借用宿主消息循环 | 全目录 grep 零命中;`win32_viewport.cpp:104-111` |
| **纯 Node 宿主已验证**:`scripts/smoke_host.js` 在普通 Node 下 fork engine-host 跑 readback 取帧 | `samples/electron/scripts/smoke_host.js:1-6` |
| addon 明确处理过 "Electron in RUN_AS_NODE" 的 external ArrayBuffer 拒绝回退 | `addon.cpp:650-651` |
| 一份二进制同时适配 Node/Electron:`/delayload:node.exe` + hook 重定向到宿主进程模块;N-API stable ABI | `xmake.lua:20-26`, `win_delay_load_hook.cpp:1-3` |
| 引擎设计原则:"引擎永不与 Chromium 同进程"(crash 隔离 + dxil 冲突);进程模型 main → fork(ELECTRON_RUN_AS_NODE) → engine-host | `main.js:4-8, 78-86` |
| 单例限制:每进程一个视口;shared 模式同进程 stop→start 会因 `RegisterClassA` 不重注册而失败(代码推断) | `viewport.h:8-15`, `win32_viewport.cpp:82` |
| 运行时 DLL 经 `PATH` prepend 从 `RBC_PROGRAM_DIR` 加载;addon C++ 不读环境变量,全经 start options | `engine-host.js:24-28`, `main.js:20-36` |

### 1.2 dsh 插件侧(deepseek-harness)

| 事实 | 证据 |
| --- | --- |
| 插件是 Host 进程内的 Cordis 插件,**无沙箱**,与 Host 同进程运行 | `packages/boot/plugin-manager/README.zh.md:27` |
| 插件可注册具名 HTTP 前缀路由和 WebSocket upgrade 路由(`ctx.webServer.register/registerUpgrade`) | `packages/host/webserver/README.zh.md:43-45` |
| 插件可携带前端:`package.json` 声明 `dsh.client:{platform:'web'}`,产物 `lib/client.js` 由 Host 以 `/plugins/` 前缀服务,经 `dsh-app://app/plugins/...` 进入主文档(Desktop 转发层把其 cache-control 改为 no-store) | `packages/util/package-manifest/src/types.ts:81-94`; `packages/client/modules/src/index.ts:649`; `apps/desktop/src/web-document.ts:65-91` |
| Web Client 有一等公民扩展点:主列整页(`sidebar.panellist` + `main` slot)、右侧栏 tab、聊天节点(`ConversationNodeDefinition`)、设置卡,共 92 个 slot | `packages/extensions/cordis-client-runner/src/client/slot-catalog.ts:1734,3122`; 第一方案例 `packages/client/ui-schedule/src/client/index.ts:168,178` |
| **原生模块无专门闸门**:pnpm 11 只拦生命周期脚本,纯预编译 .node 直接可用;脚本型包走 `approvedBuilds` 批准流 | 根 `pnpm-workspace.yaml` allowBuilds 注释; `packages/boot/plugin-manager/src/build-approval.ts:18-48` |
| 有"运行时下载"先例:Desktop 用 lock.json 固定 URL+哈希首用下载 Python/Node 运行时 | `apps/desktop/README.zh.md:65-73` |

### 1.3 dsh Desktop 壳侧(apps/desktop/src)

| 事实 | 证据 |
| --- | --- |
| 主窗口 `sandbox:true, contextIsolation:true, nodeIntegration:false`;`<webview>` 仅主窗口 | `main.ts:229-237` |
| **主窗口 HWND 零暴露**(全目录无 `getNativeWindowHandle`) | grep 零命中 |
| preload 暴露面只有 `dshDesktop{browser,deviceInfo,keyboard,shortcuts,updates}`、`dshPlatform{open,setBounds,close}`、`__DSH_HOST_PATHS__` 等窄桥 | `preload-app.ts:13-107` |
| 壳内**已存在**"DOM 占位矩形 + 主进程叠加原生视图 + 坐标/生命周期同步"的完整机制(`DesktopPlatformView`),但写死为 DeepSeek 账户页专用:page 白名单 usage/top-up、origin 来自私有凭证通道、导航锁死、存储擦洗 | `platform-view.ts:79-183`; `main.ts:723-742` |
| Host↔主进程 IPC 是封闭消息集(ready/fatal/platform-session/shutdown-complete/update-tasks/quit-inspection),违规消息直接 fail + SIGTERM;**没有任何 UI 创建/操作通道** | `host-process.ts:26-37, 206-211` |
| webview guest 被主进程强制重写 preferences(sandbox/contextIsolation),**禁止访问 Host origin** | `browser-guests.ts:72-93, 156-169` |
| 壳全量签名(Windows 硬件令牌/ macOS notarize + hardenedRuntime),打包文件白名单无第三方入口;第三方改壳必然破坏签名与更新链 | `electron-builder.config.mjs:74-230` |

---

## 2. 可行性判定:三条路径

### 路径 A(推荐,纯第三方,今天可落地):引擎独立进程 + 帧流 + 一等公民 Web 面板

**形态**:dsh 插件 = Host 半(Cordis 服务)+ Client 半(`dsh.client` bundle)。插件 fork 一个引擎子进程(与样本相同的隔离理由:GPU crash 不拖垮 Host),readback 模式取帧,经 `ctx.webServer.registerUpgrade` 开的 WebSocket 流给前端;前端在主列注册一个"3D 视口"整页面板(侧边栏图标),canvas + WebCodecs 解码渲染;指针事件回传引擎做相机控制(即样本里"renderer.js → IPC → 引擎"的旧手势路径)。

- **加载 addon 无硬障碍**:N-API stable ABI + `/delayload:node.exe` hook,正是为 Node/Electron 双宿主设计的;addon 代码里已点名处理 RUN_AS_NODE 的 external buffer 回退(`addon.cpp:650-651`)。纯预编译二进制包不触发 pnpm 脚本批准。
- **UI 是一等公民**:`main`/`sidebar.panellist` slot 组合就是 ui-schedule 普通插件在用的机制。
- **跨平台**:readback 模式不依赖 win32 视口层,macOS/Linux 桌面端同样成立。
- **代价**:失去 GPU 直显零拷贝。样本实测 readback 在 110fps 时约 235 MiB/s;面板上限分辨率/帧率(如 720p30)并优先做 H264/AV1 软/硬编码后再流式传输,可把带宽降一个数量级,这是 v2 优化项。

### 路径 B(可行但脆,Windows-only):跨进程 WS_CHILD 寄生

**形态**:插件在 Host 进程(无沙箱 Node)里跑引擎,`start(present:'shared', parentHwnd:<dsh 主窗口 HWND>)`。由于 addon 本来就是为跨进程借 HWND 设计的(样本中 HWND 就是从 Electron main 进程经 env 传给子进程的),机制完全复用;区别只在 HWND 来源:壳不提供,只能用 Win32 枚举按 ppid(=Electron 主进程 pid)+ 尺寸/可见性启发式找到主窗口。

- **必须补的 addon 改动**:`computeRect` 只有 top/right 两个 inset(`win32_viewport.cpp:28-39`),视口占满"客户区减右上边距"——会把 dsh 左侧边栏压在本机窗口下面,且 HTML 无法浮在原生视口之上(样本的解法是 UI 只活在留出的边距里)。要适配 dsh 布局需增加 `viewportLeft`(可能还有 bottom)inset,小改动但要重编 addon。
- **已知坑全部适用**:Chromium "Intermediate D3D Window" z-order 每 60 帧重断言、原生视口抢键盘焦点(dsh 全局快捷键失效)、shutdown 重入、30s GPU 清理、单例/重启限制。
- **脆弱点**:HWND 发现是启发式(标题随语言变、多窗口 welcome/update/隐藏态);壳窗口结构一变就碎;与更新遮罩、Platform 视图等壳级 UI 的 z 序关系不受控。
- **判定**:demo 级可行,产品级不可接受;但它证明了"嵌入"在 Win32 层面并无能力边界,只差壳的一个接口。

### 路径 C(产品级,需 DeepSeek 官方):通用化 PlatformView 为"原生表面租约"

壳里已经有完美的机制骨架:`DesktopPlatformView` = DOM 占位矩形 ↔ 主进程原生视图叠加 ↔ bounds/生命周期同步(`platform-view.ts:79-183`)。如果上游把它通用化——插件经 Host→主进程 IPC(协议当前 v4,需加消息)申请 "native surface lease",主进程接管插件进程创建的 HWND(或代为创建子窗口并把句柄下发给插件引擎),按 DOM 占位同步坐标、管理 z-order(给插件预留的 inset 区域模型与样本一致)、随文档销毁——就得到与今天 Electron 样本完全同级的体验,且是签名内的受支持能力。这是对上游的功能提案,不是第三方能做的事。

### 附带维度:agent 可控 3D(与上述任一路径叠加)

dsh 是 agent harness,真正的协同点是把视口注册成工具(`ctx.tools`):`viewport.open/screenshot/set_camera/load_scene`。截图走 readback 一次性回读(现成 `requestScreenshot`),agent 就有了"眼睛",形成 computer-use 式的 3D 操作闭环。这是纯插件能力,零壳改动。

---

## 3. 推荐落地方案(路径 A)步骤

1. **打包**:`robocute-dsh-plugin` npm 包——`dsh.bundle.patch` 挂 Host 服务;`dsh.client` 声明 web bundle;`.node` + 运行时 DLL 随包分发(体积大,建议仿 dsh primary-runtime 的"首用按 lock.json 下载"模式)。无生命周期脚本,安装零批准摩擦。
2. **Host 服务**:fork 引擎子进程(ELECTRON_RUN_AS_NODE,与样本同构);注册 WS 帧流路由 + 控制 REST 路由;生命周期跟随 Cordis 插件卸载,引擎 crash 只死子进程。
3. **Client bundle**:`main` slot 整页视口面板 + canvas/WebCodecs 渲染;指针手势回传相机(样本旧路径);可加聊天节点做会话内嵌 3D 缩略图。
4. **Agent 工具**:注册 viewport.* 工具,截图反馈闭环。
5. **后续**:v2 帧编码降带宽;v3 视 Windows 需求评估路径 B 的 demo;向上游提路径 C 提案。

## 4. 风险与坑

- 插件 HMR 重挂载时引擎子进程必须整进程重启(addon 同进程 stop→start 有 `RegisterClassA` 不重注册的问题;子进程模型天然规避)。
- Host ABI 必须与 Desktop 内置 Electron 的 Node 匹配(N-API 解决了这一点;避免用非 N-API 的原生依赖)。
- Desktop 的 webview guest 禁访 Host origin——不要走"侧边栏浏览器挂插件页"的路线,主文档 client bundle 才是正路。
- 企业环境代码完整性策略可能拦截未签名 .node(Desktop README 自述"冒烟检查通过不代表兼容所有企业策略")。
- readback 带宽:样本实测数字为 110fps ≈ 235 MiB/s,面板场景务必限帧限分辨率。
- macOS/Linux 桌面端只有路径 A 成立(win32 视口层为空桩,自动回落 readback)。

## 5. 后续(路径 C 已落地并验证)

已在 deepseek-harness fork(`try3dfeat` 分支,`0e242b8` feat + `21ae184d` dev-carrier,已推送)落地壳实现 **native viewport lease**,并端到端验证通过(侧边栏「3D 视口」面板,GPU 直显):

- 设计/PR 叙事(壳改动全部在此):fork 的 `.agents/notes/proposed/feature/2026-10-06-native-viewport-lease.zh.md`
- 壳:`apps/desktop/src/native-viewport.ts`(koffi FFI 创建 STATIC 容器 HWND,DIP→物理像素换算,z-order 2Hz 重断言,owner 生命周期跟随)+ `preload-native-viewport.ts`(`dshNativeViewport` 桥)+ `main.ts` 接线(复用 Platform 的 `assertMainApplication` 校验);9 个单测全过,desktop tsc/tsdown 全过,koffi FFI 真机冒烟通过。**fork 分支不含任何 RoboCute 相关内容**,可直接作为上游 PR 源。
- 插件(独立第三方仓库,不在 fork 内):`D:\ws\repos\RoboCute-repo\dsh-RoboCute-plugin`(remote: github.com/sailing-innocent/dsh-RoboCute-plugin)。Host 半注册 `/robocute-viewport` 控制路由 + fork 引擎子进程;client 半手写 lazy-CJS 注册 `main`/`sidebar.panellist` slot 面板;RoboCute addon **零改动**(`present:'shared'` + `parentHwnd=容器` + inset 0);非 Windows/官方壳自动退化为 ~0.7fps 帧流面板。
- 安装(应用内正规路径):dsh-desktop → 设置 → 插件 → 添加 → 输入 `D:\ws\repos\RoboCute-repo\dsh-RoboCute-plugin` → 在线重组后侧边栏出现「3D 视口」。详见插件仓库 README。
- 关键运维事实:dev 启动的 DSH_HOME 是 **APP_ROOT 相对**的 `apps/desktop/.desktop-build/development/home`(不是仓库根);`file:` 安装是快照、`link:` 是活链接(开发迭代用 link:);npm 版 `dsh` 拒绝 desktop profile,管理用 fork 的 `scripts/dsh-desktop.mjs` 载体。

## 6. 一句话总结

RoboCute 写成一个 **dsh 插件完全可行**,而且插件模型(Host 无沙箱 Node + `dsh.client` 前端 bundle + slot 面板 + 工具注册)恰好承载"引擎子进程 + 帧流面板 + agent 可控"的完整形态;但 **HWND 直显级嵌入在封闭签名的 dsh 壳内没有第三方通道**,壳里唯一同类的 PlatformView 机制是写死的内部件——要么接受路径 A 的帧流折衷,要么推动 DeepSeek 把 PlatformView 通用化为插件可用的原生表面租约(路径 C),路径 B 只适合做概念验证。
