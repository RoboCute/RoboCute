# Viewport Widget

Viewport Widget是整个Editor的核心Widget之一，封装了几乎所有的渲染预览功能

核心功能是使用`QWidget::createWindowContainer`功能封装rhiWindow，将所有Margin和Spacing设置为0

Inner Elements
- m_innerContainer

## 交互功能

### 1. 对象选择（Click Selection）

**功能描述：**
- 左键点击视口中的对象可以选择该对象
- 点击空白区域会取消当前选择
- 选中的对象会显示轮廓高亮

**实现方式：**
- 使用 `ViewportInteractionManager` 管理选择状态
- 通过 `ClickManager` 进行射线检测，获取点击位置的对象ID
- 选择状态通过 `VisApp::dragged_object_ids` 同步到编辑器

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


### 3. 拖动已选对象（Drag Selected Objects）

**功能描述：**
- 当有对象被选中时，左键拖动可以移动已选对象（未来功能）
- 拖动优先级高于框选：如果有选择且未按 Ctrl，拖动会优先处理

**实现方式：**
- `InteractionMode::Dragging` - 拖动模式
- 在拖动模式下，相机控制被禁用，避免冲突



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



## ViewportContainerWidget

