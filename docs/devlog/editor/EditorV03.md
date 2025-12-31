# RGC Editor in V0.3.0

本次更新中对于Editor的部分主要需求是

- [x] 接入RBCRuntime中的场景定义
- [ ] 更新灵活的Workflow
- [ ] 支持调试渲染参数的Detail面板
- [ ] 实现RBCProject的结构管理

## Workflow新架构

### 问题分析

#### 当前架构的缺陷

当前的 Workflow 切换实现存在以下严重问题，特别是从 Text2Image 切换回 Scene 时会导致程序崩溃：

1. **Widget 生命周期管理混乱**
   - 在不同 Workflow 之间转移 Widget（从 DockWidget 到 CentralWidget）
   - Parent 管理不当，频繁调用 `setParent()` 和 `setWidget(nullptr)`
   - ViewportWidget 包含原生窗口句柄（RhiWindow），在 parent 转移时可能导致资源泄漏

2. **信号连接未清理**
   - 切换 Workflow 时旧的信号连接仍然存在
   - 可能访问已销毁或处于不稳定状态的对象
   - 导致崩溃或未定义行为

3. **布局重组逻辑复杂且脆弱**
   ```cpp
   // 当前的做法：移除widget，从dock取出，设置parent，再添加
   context_->viewportDock->setWidget(nullptr);
   context_->viewportWidget->setParent(mainWindow_);
   mainWindow_->setCentralWidget(context_->viewportWidget);
   ```

4. **职责划分不清**
   - EditorLayoutManager 同时处理 UI 创建、布局调整、Workflow 切换
   - WorkflowManager 仅仅是信号中继，没有真正管理状态
   - MainWindow 仍然处理部分业务逻辑

5. **缺乏明确的状态管理**
   - 没有状态机机制确保切换的原子性
   - 中间状态可能导致不一致

### 新架构设计

#### 核心设计思想

**1. 状态机模式（State Pattern）**

每个 Workflow 是一个独立的状态（State），封装该 Workflow 下的：
- UI 布局配置
- Widget 的可见性
- 信号连接管理
- 生命周期管理

**2. 容器化隔离（Container Isolation）**

不再在 Workflow 之间转移 Widget，而是：
- 每个 Workflow 使用固定的容器作为 CentralWidget
- 容器内部管理自己的 Widget 布局
- 切换 Workflow 时只需切换容器的可见性

**3. RAII 资源管理**

使用 RAII 和智能指针管理资源：
- 信号连接使用 `QMetaObject::Connection` 管理
- 状态切换时自动清理旧连接
- 避免手动管理生命周期

#### 架构组件

```
┌─────────────────────────────────────────────────────┐
│                   MainWindow                         │
│  ┌──────────────────────────────────────────────┐  │
│  │          WorkflowManager                      │  │
│  │  (State Machine)                              │  │
│  │  ┌──────────────┐  ┌──────────────┐         │  │
│  │  │ SceneWorkflow│  │ Text2Image   │  ...    │  │
│  │  │   State      │  │   Workflow   │         │  │
│  │  └──────────────┘  └──────────────┘         │  │
│  └──────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────┐  │
│  │       WorkflowContainerManager               │  │
│  │  (Manages Central Widget Containers)         │  │
│  │  ┌──────────────┐  ┌──────────────┐         │  │
│  │  │ Scene        │  │ Text2Image   │         │  │
│  │  │ Container    │  │ Container    │         │  │
│  │  └──────────────┘  └──────────────┘         │  │
│  └──────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────┐  │
│  │      EditorLayoutManager                     │  │
│  │  (Only manages DockWidgets visibility)       │  │
│  └──────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
```

#### 关键类设计

**1. WorkflowState（抽象基类）**

```cpp
class WorkflowState {
public:
    virtual ~WorkflowState() = default;
    
    // 进入该状态时调用
    virtual void enter(EditorContext* context) = 0;
    
    // 退出该状态时调用（清理资源）
    virtual void exit(EditorContext* context) = 0;
    
    // 获取该状态的中央容器
    virtual QWidget* getCentralContainer() = 0;
    
    // 获取该状态下应该可见的 Dock
    virtual QList<QDockWidget*> getVisibleDocks() = 0;
    
    // 获取该状态下应该隐藏的 Dock
    virtual QList<QDockWidget*> getHiddenDocks() = 0;
    
    // 获取状态类型
    virtual WorkflowType getType() const = 0;
    
protected:
    // 管理信号连接
    QList<QMetaObject::Connection> connections_;
    
    // 清理所有信号连接
    void disconnectAll();
};
```

**2. SceneEditingWorkflowState（场景编辑状态）**

```cpp
class SceneEditingWorkflowState : public WorkflowState {
public:
    SceneEditingWorkflowState(QObject* parent = nullptr);
    
    void enter(EditorContext* context) override;
    void exit(EditorContext* context) override;
    QWidget* getCentralContainer() override;
    QList<QDockWidget*> getVisibleDocks() override;
    QList<QDockWidget*> getHiddenDocks() override;
    WorkflowType getType() const override;
    
private:
    // 场景编辑的中央容器（包含 Viewport 和底部的 NodeEditor）
    QWidget* centralContainer_;
    QSplitter* splitter_;  // 分割 Viewport 和 NodeEditor
};
```

**3. Text2ImageWorkflowState（Text2Image 状态）**

```cpp
class Text2ImageWorkflowState : public WorkflowState {
public:
    Text2ImageWorkflowState(QObject* parent = nullptr);
    
    void enter(EditorContext* context) override;
    void exit(EditorContext* context) override;
    QWidget* getCentralContainer() override;
    QList<QDockWidget*> getVisibleDocks() override;
    QList<QDockWidget*> getHiddenDocks() override;
    WorkflowType getType() const override;
    
private:
    // Text2Image 的中央容器（NodeEditor 占据全部）
    QWidget* centralContainer_;
};
```

**4. WorkflowManager（状态机管理器）**

```cpp
class WorkflowManager : public QObject {
    Q_OBJECT
public:
    explicit WorkflowManager(QObject* parent = nullptr);
    ~WorkflowManager() override;
    
    // 初始化所有状态
    void initialize(EditorContext* context);
    
    // 切换到指定 Workflow
    void switchWorkflow(WorkflowType type);
    
    // 获取当前状态
    WorkflowState* currentState() const;
    WorkflowType currentWorkflow() const;
    
signals:
    void workflowWillChange(WorkflowType newType, WorkflowType oldType);
    void workflowChanged(WorkflowType newType, WorkflowType oldType);
    
private:
    EditorContext* context_;
    WorkflowState* currentState_;
    QMap<WorkflowType, WorkflowState*> states_;
    
    // 确保切换的原子性
    bool isTransitioning_;
};
```

**5. WorkflowContainerManager（容器管理器）**

```cpp
class WorkflowContainerManager : public QObject {
    Q_OBJECT
public:
    explicit WorkflowContainerManager(QMainWindow* mainWindow, 
                                      EditorContext* context,
                                      QObject* parent = nullptr);
    
    // 创建所有 Workflow 的容器
    void createContainers();
    
    // 切换到指定 Workflow 的容器
    void switchContainer(WorkflowType type);
    
    // 获取指定 Workflow 的容器
    QWidget* getContainer(WorkflowType type) const;
    
private:
    QMainWindow* mainWindow_;
    EditorContext* context_;
    QStackedWidget* stackedWidget_;  // 用于管理多个容器
    QMap<WorkflowType, QWidget*> containers_;
    
    // 创建特定 Workflow 的容器
    QWidget* createSceneEditingContainer();
    QWidget* createText2ImageContainer();
};
```

#### 切换流程

```
用户触发切换
    ↓
WorkflowManager::switchWorkflow(newType)
    ↓
1. 检查是否正在切换（防止重入）
    ↓
2. emit workflowWillChange(newType, oldType)
    ↓
3. currentState_->exit(context_)
   - 断开所有信号连接
   - 清理临时资源
   - 隐藏对应的 UI 元素
    ↓
4. WorkflowContainerManager::switchContainer(newType)
   - 切换 StackedWidget 的当前页面
    ↓
5. EditorLayoutManager::adjustDockVisibility(newType)
   - 根据新状态显示/隐藏 Dock
    ↓
6. currentState_ = states_[newType]
    ↓
7. currentState_->enter(context_)
   - 建立新的信号连接
   - 初始化状态
   - 更新 UI
    ↓
8. emit workflowChanged(newType, oldType)
    ↓
切换完成
```

#### 优势

1. **安全性**
   - 每个 Workflow 的 Widget 固定在自己的容器中
   - 不再需要 parent 转移，避免生命周期问题
   - 状态机确保切换的原子性

2. **可维护性**
   - 每个 Workflow 的逻辑独立封装
   - 添加新 Workflow 只需实现新的 State 类
   - 职责清晰：WorkflowManager（状态机）、ContainerManager（容器）、LayoutManager（Dock）

3. **扩展性**
   - 易于添加新的 Workflow（如 Animation、Physics 等）
   - 易于扩展状态的功能（如保存/恢复布局）
   - 支持复杂的状态转换逻辑

4. **性能**
   - 使用 QStackedWidget 切换容器，只是改变可见性
   - 避免重复创建/销毁 Widget
   - 信号连接的 RAII 管理避免内存泄漏

### 实现计划

1. ✅ 实现 WorkflowState 基类和具体状态类
2. ✅ 重构 WorkflowManager 使用状态机模式
3. ✅ 实现 WorkflowContainerManager
4. ✅ 重构 EditorLayoutManager 简化职责
5. ✅ 更新 MainWindow 集成新架构
6. ⏳ 测试各种切换场景（待用户测试）

### 实现总结

#### 已实现的文件

**新增文件：**
1. `rbc/editor/runtime/include/RBCEditorRuntime/runtime/WorkflowState.h` - 抽象基类
2. `rbc/editor/runtime/src/runtime/WorkflowState.cpp`
3. `rbc/editor/runtime/include/RBCEditorRuntime/runtime/SceneEditingWorkflowState.h`
4. `rbc/editor/runtime/src/runtime/SceneEditingWorkflowState.cpp`
5. `rbc/editor/runtime/include/RBCEditorRuntime/runtime/Text2ImageWorkflowState.h`
6. `rbc/editor/runtime/src/runtime/Text2ImageWorkflowState.cpp`
7. `rbc/editor/runtime/include/RBCEditorRuntime/runtime/WorkflowContainerManager.h`
8. `rbc/editor/runtime/src/runtime/WorkflowContainerManager.cpp`

**修改的文件：**
1. `rbc/editor/runtime/include/RBCEditorRuntime/runtime/WorkflowManager.h` - 重构为状态机
2. `rbc/editor/runtime/src/runtime/WorkflowManager.cpp`
3. `rbc/editor/runtime/include/RBCEditorRuntime/runtime/EditorLayoutManager.h` - 简化职责
4. `rbc/editor/runtime/src/runtime/EditorLayoutManager.cpp`
5. `rbc/editor/runtime/include/RBCEditorRuntime/MainWindow.h` - 集成新架构
6. `rbc/editor/runtime/src/MainWindow.cpp`

#### 核心改进

1. **Widget 生命周期安全**
   - 每个 Workflow 有独立的容器
   - Widget 固定在各自容器中，不再跨 parent 转移
   - 使用 QStackedWidget 切换容器，只改变可见性

2. **状态机管理**
   - WorkflowManager 使用状态机模式
   - 每个状态封装 enter/exit 逻辑
   - 自动管理信号连接的生命周期
   - 防止重入的保护机制

3. **职责分离**
   - WorkflowManager：状态机管理
   - WorkflowContainerManager：中央容器管理
   - EditorLayoutManager：DockWidgets 可见性管理
   - WorkflowState：具体布局逻辑

4. **扩展性**
   - 添加新 Workflow 只需实现新的 WorkflowState 子类
   - 不需要修改核心的 Manager 代码

#### 初始化流程修复 (2024-12-31)

**问题：** Viewport 在启动时固定在左上角而不是居中填充

**原因分析：**
1. ViewportWidget 和 NodeEditor 在 `EditorLayoutManager::setupDocks()` 时创建，parent 是 mainWindow_
2. 它们没有被添加到任何布局中，处于"浮动"状态
3. 在 `registerWorkflowContainers()` 时被添加到容器，但此时容器可能还没有正确的尺寸
4. `setupSplitterSizes()` 在容器显示之前就被调用，导致尺寸计算不正确

**修复方案：**
1. ✅ 在创建 ViewportWidget 和 NodeEditor 时设置 `setSizePolicy(Expanding, Expanding)`
2. ✅ 初始时设置为不可见 `setVisible(false)`，避免在错误位置显示
3. ✅ 在 WorkflowState::enter() 时才设置为可见
4. ✅ 使用 `QTimer::singleShot(0, ...)` 延迟调用 `setupSplitterSizes()`，确保容器已正确布局
5. ✅ 设置 Splitter 的 `setChildrenCollapsible(false)` 防止子 widget 被折叠

**相关代码修改：**
- `EditorLayoutManager.cpp` - 创建时设置尺寸策略和初始可见性
- `SceneEditingWorkflowState.cpp` - 延迟设置 splitter 尺寸，添加 setSizePolicy
- `Text2ImageWorkflowState.cpp` - 设置尺寸策略

#### 测试建议

1. **基本切换测试**
   - SceneEditing ↔ Text2Image 多次切换
   - 确认不会崩溃

2. **Widget 状态测试**
   - 切换后 Viewport 和 NodeEditor 的显示是否正常
   - 交互功能是否正常（选择、拖动等）

3. **布局测试**
   - 启动时 Viewport 应正确填充中央区域，不会固定在左上角
   - Viewport 和 NodeEditor 的 Splitter 比例应为 70:30
   - 调整窗口大小时 Widget 应正确缩放

4. **内存泄漏测试**
   - 长时间频繁切换后检查内存使用

5. **边界情况测试**
   - 快速连续切换
   - 切换过程中的其他操作

### 下一步优化建议

1. **保存/恢复布局**
   - 每个 Workflow 保存自己的布局配置（Splitter 大小、Dock 位置等）
   - 切换回来时恢复之前的布局

2. **动画过渡**
   - 添加切换动画使体验更流畅

3. **更多 Workflow**
   - Animation Workflow
   - Physics Workflow
   - Debugging Workflow

4. **状态持久化**
   - 将当前 Workflow 和布局配置保存到配置文件
   - 下次启动时恢复

