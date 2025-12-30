# Editor V02 

- start date: 2025-12-12
- finish date: 

## 2025-12-12 重构MainWindow

### 目标

当前`MainWindow`类承担了过多的职责，包括UI组件管理、场景同步、工作流切换、事件处理等，导致代码臃肿难以维护。本次重构的目标是：
- 遵循单一职责原则，将不同功能分离到专门的类中
- 提高代码的可读性、可维护性和可测试性
- 保持现有功能不变，确保重构过程平滑过渡

### 重构计划

#### 0. 集中Context提取

将Editor上方提取一个EditorContext类，用来管理当前Editor的全部状态，让UI表现与工作流分离

#### 1. UI逻辑拆分 - EditorLayoutManager
- **职责**：负责管理所有UI组件的创建、布局和显示/隐藏
- **具体内容**：
  - 管理菜单栏(MenuBar)、工具栏(ToolBar)的创建和配置
  - 管理所有停靠窗口(DockWidgets)的创建、布局和可见性控制
  - 根据工作流类型动态调整UI布局
- **关联类**：MainWindow(使用)、WorkflowController(通知)

#### 2. 消息管线独立 - CentralEvent/Command Bus

- **职责**：作为消息中枢，处理组件间的通信
- **具体内容**：
  - 实现发布-订阅模式，允许组件间解耦通信
  - 管理命令队列，支持命令的发送、撤销和重做
  - 提供事件过滤和转发机制
- **关联类**：所有需要通信的组件

#### 3. 工作流管理 - WorkflowController
- **职责**：管理工作流的切换和相关资源配置
- **具体内容**：
  - 定义工作流类型枚举(如SceneEditing、Text2Image等)
  - 实现工作流切换逻辑
  - 协调UI布局变更和相关组件的启用/禁用
  - 管理不同工作流下的特定功能模块
- **关联类**：MainWindow(使用)、EditorLayoutManager(协调UI)

#### 4. 业务逻辑拆分：实现专门的业务管理类

- **SceneUpdater**：
  - **职责**：处理场景更新相关的逻辑
  - **功能**：响应场景变化事件，更新场景层次结构和相关视图
- **EntitySelectionHandler**：
  - **职责**：处理实体选择相关的逻辑
  - **功能**：响应实体选择事件，更新属性面板和相关视图
- **AnimationController**：
  - **职责**：处理动画播放控制相关的逻辑
  - **功能**：响应动画帧变化事件，更新播放器和场景显示

#### 5. 依赖注入 - Dependency Injection
- **职责**：管理对象之间的依赖关系
- **具体内容**：
  - 通过构造函数或setter方法注入依赖
  - 支持服务定位器模式获取常用服务
  - 简化对象创建和测试

### 类职责划分详情

#### MainWindow (精简后的主窗口类)
- **职责**：程序入口点和顶层容器
- **功能**：
  - 初始化核心子系统(EditorEngine等)
  - 创建并持有各个管理器实例
  - 设置主窗口基本属性和初始布局
  - 协调各管理器之间的初始化顺序

#### EditorLayoutManager (新增)
- **职责**：UI布局管理器
- **功能**：
  - 创建和管理所有UI组件(菜单、工具栏、停靠窗口)
  - 根据工作流类型调整UI布局
  - 处理UI组件的显示/隐藏逻辑

#### WorkflowController (增强)
- **职责**：工作流控制器
- **功能**：
  - 管理可用的工作流列表
  - 实现工作流切换逻辑
  - 通知EditorLayoutManager更新UI布局

#### SceneUpdater (新增)
- **职责**：场景更新处理器
- **功能**：
  - 监听场景同步管理器的更新事件
  - 更新场景层次结构视图
  - 更新结果面板等依赖场景数据的组件

#### EntitySelectionHandler (新增)
- **职责**：实体选择处理器
- **功能**：
  - 监听实体选择事件
  - 更新属性面板显示
  - 处理实体相关的上下文操作

#### AnimationController (新增)
- **职责**：动画控制器
- **功能**：
  - 监听动画帧变化事件
  - 控制动画播放管理器
  - 更新动画播放器UI和场景显示

### 依赖关系
```
MainWindow
├── EditorLayoutManager
├── WorkflowController
├── SceneUpdater
├── EntitySelectionHandler
├── AnimationController
└── CentralEventBus (消息总线)
```

