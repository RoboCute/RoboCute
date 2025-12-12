# Viewport 交互功能总结文档

## 概述

本文档总结了 RoboCute Editor 中 Viewport 组件的交互功能实现，包括选择、框选、拖动和拖放等核心功能。

## 功能特性

### 1. 对象选择（Click Selection）

**功能描述：**
- 左键点击视口中的对象可以选择该对象
- 点击空白区域会取消当前选择
- 选中的对象会显示轮廓高亮

**实现方式：**
- 使用 `ViewportInteractionManager` 管理选择状态
- 通过 `ClickManager` 进行射线检测，获取点击位置的对象ID
- 选择状态通过 `VisApp::dragged_object_ids` 同步到编辑器

**相关文件：**
- `rbc/editor/runtime/include/RBCEditorRuntime/engine/ViewportInteractionManager.h`
- `rbc/editor/runtime/src/engine/ViewportInteractionManager.cpp`
- `rbc/editor/runtime/src/engine/visapp.cpp`

### 2. 框选（Box Selection）

**功能描述：**
- 按住 `Ctrl` 键 + 左键拖动可以进行框选
- 框选区域内的所有对象都会被选中
- 支持多选模式（Ctrl + 框选会添加到当前选择）

**实现方式：**
- 检测 Ctrl 键状态和拖动距离
- 使用 `ClickManager::add_frame_selection()` 进行区域选择
- 将 NDC 坐标（-1 到 1）转换为屏幕 UV 坐标（0 到 1）

**交互模式：**
- `InteractionMode::DragSelect` - 框选模式
- 仅在 `Ctrl` 键按下时激活

**相关文件：**
- `rbc/editor/runtime/src/engine/ViewportInteractionManager.cpp` (handle_cursor_position, update)

### 3. 拖动已选对象（Drag Selected Objects）

**功能描述：**
- 当有对象被选中时，左键拖动可以移动已选对象（未来功能）
- 拖动优先级高于框选：如果有选择且未按 Ctrl，拖动会优先处理

**实现方式：**
- `InteractionMode::Dragging` - 拖动模式
- 在拖动模式下，相机控制被禁用，避免冲突

**相关文件：**
- `rbc/editor/runtime/src/engine/ViewportInteractionManager.cpp` (determine_interaction_mode)

### 4. 拖放到 NodeEditor（Drag and Drop to NodeEditor）

**功能描述：**
- 在视口中选中对象后，可以拖动到 NodeEditor 的 EntityInput 节点
- 拖动时会显示拖动的实体ID和名称
- 支持从 SceneHierarchy 和 Viewport 两个来源拖动实体

**实现方式：**
- 在 `ViewportWidget` 上安装事件过滤器，拦截发送到 `RhiWindow` 的鼠标事件
- 检测拖动距离（超过 5 像素）后启动 Qt 拖放操作
- 使用 `EntityDragDropHelper` 创建 MIME 数据
- 将实例ID（Instance ID）转换为实体ID（Entity ID）

**拖动条件：**
- 有选中对象时：直接拖动（无需 Ctrl）
- 无选中对象时：需要 Ctrl + 拖动（用于框选）

**相关文件：**
- `rbc/editor/runtime/src/components/ViewportWidget.cpp` (eventFilter, startEntityDrag)
- `rbc/editor/runtime/include/RBCEditorRuntime/runtime/EntityDragDropHelper.h`

### 5. 相机控制（Camera Control）

**功能描述：**
- 右键拖动：旋转相机
- 中键拖动：平移相机
- WASD/QE：移动相机
- 鼠标滚轮：缩放

**实现方式：**
- 使用 `CameraController` 处理相机输入
- 在非交互模式或拖动模式下允许相机控制
- 在点击选择或框选模式下禁用相机控制，避免冲突

**相关文件：**
- `rbc/editor/runtime/src/engine/visapp.cpp` (update, handle_mouse)
- `rbc/app/camera_controller.h`

## 架构设计

### ViewportInteractionManager

**职责：**
- 管理所有视口交互状态（选择、拖动、框选）
- 使用状态机模式管理交互模式
- 处理键盘和鼠标输入事件

**状态枚举：**
- `InteractionMode::None` - 无交互
- `InteractionMode::ClickSelect` - 点击选择
- `InteractionMode::DragSelect` - 框选
- `InteractionMode::Dragging` - 拖动已选物体

**鼠标状态枚举：**
- `MouseState::Idle` - 空闲
- `MouseState::Pressed` - 按下
- `MouseState::Dragging` - 拖动中
- `MouseState::Released` - 释放
- `MouseState::WaitingResult` - 等待点击查询结果

**关键方法：**
- `handle_key()` - 处理键盘事件（Ctrl键检测）
- `handle_mouse()` - 处理鼠标按下/释放事件
- `handle_cursor_position()` - 处理鼠标移动事件
- `update()` - 更新交互状态，查询选择结果

### ViewportWidget

**职责：**
- 封装 RhiWindow 为 QWidget
- 处理拖放操作
- 转发事件到 RhiWindow

**关键组件：**
- `ViewportContainerWidget` - 自定义容器，包装 `createWindowContainer` 创建的容器
- `eventFilter` - 拦截发送到 RhiWindow 的鼠标事件，处理拖动逻辑

**事件处理流程：**
1. 鼠标事件到达 `RhiWindow`
2. `ViewportWidget::eventFilter` 拦截事件
3. 检测拖动条件（选择状态、Ctrl键、拖动距离）
4. 启动 Qt 拖放操作
5. 事件继续传播到 `RhiWindow`，用于渲染和相机控制

## 技术细节

### 坐标转换

**屏幕坐标 → UV坐标：**
```cpp
luisa::float2 uv = clamp(xy / make_float2(resolution), luisa::float2(0.f), luisa::float2(1.f));
```

**UV坐标 → NDC坐标（用于框选）：**
```cpp
luisa::float2 min_ndc = selection_region.first * 2.f - 1.f;
luisa::float2 max_ndc = selection_region.second * 2.f - 1.f;
```

### 拖动阈值

- **拖动检测阈值：** 5 像素（Manhattan距离）
- **点击/拖动区分阈值：** 1e-2f（归一化UV坐标）

### 事件传递机制

由于 `createWindowContainer` 创建的容器直接将事件发送到 QWindow，而不是通过 QWidget 事件系统，因此：

1. 在 `ViewportWidget` 上安装事件过滤器到 `RhiWindow`
2. 在事件过滤器中处理拖动逻辑
3. 事件继续传播，不影响渲染和相机控制

### ID 转换

**Instance ID → Entity ID：**
- 渲染系统使用 Instance ID（实例ID）
- 编辑器使用 Entity ID（实体ID）
- 通过 `EditorScene::getEntityIdsFromInstanceIds()` 进行转换

## 交互优先级

1. **拖动已选对象** > **框选** > **点击选择**
2. **Ctrl + 左键** = 框选模式（无论是否有选择）
3. **有选择 + 左键拖动** = 拖动已选对象模式
4. **无选择 + 左键拖动** = 点击选择模式（不进行框选）

## 已知限制

1. **拖动已选对象**：当前仅实现了拖动检测，实际移动物体的功能尚未实现
2. **多选模式**：Ctrl + 框选会添加到当前选择，但 Ctrl + 点击选择的行为可能需要进一步确认
3. **重复选择**：如果框选后再次点击已选中的对象，会保持所有框选的物体不变（这是预期行为）

## 未来改进

1. 实现拖动已选对象的实际移动功能
2. 添加撤销/重做支持
3. 优化框选的视觉反馈
4. 添加选择框的绘制
5. 支持多选时的批量操作

## 相关文件清单

### 核心文件
- `rbc/editor/runtime/include/RBCEditorRuntime/engine/ViewportInteractionManager.h`
- `rbc/editor/runtime/src/engine/ViewportInteractionManager.cpp`
- `rbc/editor/runtime/include/RBCEditorRuntime/engine/visapp.h`
- `rbc/editor/runtime/src/engine/visapp.cpp`
- `rbc/editor/runtime/include/RBCEditorRuntime/components/ViewportWidget.h`
- `rbc/editor/runtime/src/components/ViewportWidget.cpp`

### 辅助文件
- `rbc/editor/runtime/include/RBCEditorRuntime/components/RHIWindow.h`
- `rbc/editor/runtime/src/components/RHIWindow.cpp`
- `rbc/editor/runtime/include/RBCEditorRuntime/runtime/EntityDragDropHelper.h`
- `rbc/app/camera_controller.h`

## 更新日志

### 2024-12-XX
- 重构了视口交互逻辑，引入 `ViewportInteractionManager`
- 实现了框选功能（Ctrl + 拖动）
- 实现了拖放到 NodeEditor 的功能
- 修复了点击空白区域取消选择的问题
- 修复了 Ctrl 键检测问题
- 优化了事件传递机制，解决了鼠标事件无法传递的问题

