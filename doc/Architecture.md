# RoboCute Architecture

这里我们介绍RoboCute的整体架构

## Editor Architecture Refactoring (Qt + Custom Renderer)

为了提高系统的可扩展性和解耦Qt UI与底层Renderer，我们采用了如下架构：

### 核心组件

1.  **EditorEngine (Runtime Core)**
    - **职责**: 管理应用程序的全局生命周期，持有 `rbc::App` (渲染后端) 和 `luisa::compute::Context`。
    - **模式**: Singleton (单例)。
    - **接口**: 实现了 `IRenderer` 接口，作为 UI 和 渲染器 之间的桥梁。
    - **生命周期**: 在 `main` 函数最开始初始化，在 `QApplication` 退出后销毁。

2.  **ViewportWidget (UI Component)**
    - **职责**: 封装了 `RhiWindow` (Qt RHI 窗口) 和 `QWidget` 容器。负责将 3D 渲染窗口嵌入到 Qt 布局中，并处理输入事件转发。
    - **复用性**: 作为一个独立的 `QWidget`，可以被放置在 `MainWindow` 的任意位置，也可以实例化多个。
    - **依赖**: 依赖 `IRenderer` (通常由 `EditorEngine` 提供) 来进行实际的渲染调用。

3.  **MainWindow (Main Frame)**
    - **职责**: 负责编辑器的主界面布局 (Docking, Menus, Toolbars)。
    - **集成**: 不再手动管理 Renderer 的初始化，而是直接使用 `ViewportWidget`。

### 生命周期 (Lifecycle)

1.  **Startup**:
    - `EditorEngine::instance().init()`: 初始化 LuisaCompute Context 和 Backend Device。
    - `QApplication` 初始化。
    - `MainWindow` 创建 -> `ViewportWidget` 创建 -> `RhiWindow` 创建。
    - `RhiWindow` 初始化 `QRhi` (DirectX/Vulkan/Metal)。

2.  **Runtime**:
    - Qt 事件循环处理 UI 事件。
    - `RhiWindow` 在每一帧调用 `IRenderer::update()` 和 `IRenderer::get_present_texture()` 进行渲染和显示。
    - 输入事件 (Key/Mouse) 由 `ViewportWidget` 捕获并转发给 `RhiWindow`，再传递给 `EditorEngine`。

3.  **Shutdown**:
    - `QApplication::exec()` 返回。
    - `MainWindow` 析构 -> `ViewportWidget` 析构 -> `RhiWindow` 释放 SwapChain 并析构。
    - **关键点**: UI 组件必须在 Engine 销毁前析构，以确保 RHI 资源 (SwapChain, Textures) 能在 Device 销毁前安全释放。
    - `EditorEngine::instance().shutdown()`: 销毁 Backend Device 和 Context。

### 优势

- **解耦**: `main.cpp` 不再包含大量的初始化胶水代码。
- **稳定性**: 明确了销毁顺序，避免了 Device Lost 或 资源泄露导致的 Crash。
- **可维护性**: `ViewportWidget` 可以在任何地方使用，方便未来支持多视口 (Multi-viewport)。
