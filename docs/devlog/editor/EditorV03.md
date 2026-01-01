# RGC Editor in V0.3.0

本次更新中对于Editor的部分主要需求是

- [x] 接入RBCRuntime中的场景定义
- [x] 更新灵活的Workflow
- [ ] 更科学的大型Qt项目组织
- [ ] 热更新，组件调试工具链支持
- [ ] 支持调试渲染参数的Detail面板
- [ ] 实现RBCProject的结构管理

## 更科学的大型Qt组织架构

### 1. 现状分析

#### 1.1 当前架构概览

目前 RoboCute Editor 的代码组织如下：

```
rbc/editor/
├── runtime/
│   ├── include/RBCEditorRuntime/
│   │   ├── components/          # UI组件
│   │   ├── engine/              # 渲染引擎
│   │   ├── runtime/             # 运行时系统
│   │   ├── nodes/               # 节点编辑器相关
│   │   ├── misc/                # 杂项工具
│   │   └── MainWindow.h         # 主窗口
│   └── src/
│       ├── components/
│       ├── engine/
│       ├── runtime/
│       ├── nodes/
│       └── animation/
└── editor/
    └── main.cpp
```

#### 1.2 当前架构的优点

1. **职责划分初步清晰**：
   - `components/` 包含UI组件
   - `runtime/` 包含业务逻辑和状态管理
   - `engine/` 包含渲染相关代码

2. **引入了现代设计模式**：
   - **状态机模式**：WorkflowManager + WorkflowState 管理工作流切换
   - **发布-订阅模式**：EventBus 实现组件间解耦通信
   - **适配器模式**：EventAdapter 连接Qt信号槽和EventBus
   - **工厂模式**：NodeFactory 创建动态节点

3. **事件驱动架构**：
   - 通过 EventBus 实现组件间松耦合
   - EventAdapter 负责信号到事件的转换

#### 1.3 当前架构存在的问题

1. **职责边界模糊**：
   - `MainWindow` 承担过多职责：UI管理、业务逻辑协调、生命周期管理
   - `EditorContext` 作为全局上下文容器，包含大量组件指针，导致耦合度高
   - Manager类职责不够单一，如 `EditorLayoutManager` 既管理布局又处理工作流切换

2. **组件耦合度高**：
   - 组件直接依赖 `EditorContext`，间接依赖其中的所有其他组件
   - 组件间通过 `EditorContext` 获取其他组件，产生隐式依赖
   - 难以进行独立的单元测试和Mock测试

3. **扩展性不足**：
   - 添加新功能需要修改多个文件
   - 新增组件需要在 `EditorContext` 中添加指针
   - 难以动态加载/卸载功能模块

4. **缺乏明确的分层架构**：
   - UI层、业务逻辑层、数据层混杂
   - 依赖关系不够清晰
   - 难以维护和测试

5. **测试困难**：
   - 组件高度耦合，难以进行单元测试
   - 缺乏接口抽象，难以创建Mock对象
   - 缺乏依赖注入机制

### 2. 新架构设计原则

#### 2.1 设计原则

1. **SOLID原则**：
   - **单一职责原则 (SRP)**：每个类只负责一个明确的职责
   - **开闭原则 (OCP)**：对扩展开放，对修改封闭
   - **里氏替换原则 (LSP)**：使用接口抽象，支持多态
   - **接口隔离原则 (ISP)**：接口应该小而专注
   - **依赖倒置原则 (DIP)**：依赖抽象而非具体实现

2. **依赖注入 (Dependency Injection)**：
   - 通过构造函数或Setter注入依赖
   - 使用Service Locator模式管理全局服务
   - 支持Mock对象替换，便于测试

3. **清晰的分层架构**：
   - Presentation Layer (UI层)
   - Application Layer (应用逻辑层)
   - Domain Layer (领域模型层)
   - Infrastructure Layer (基础设施层)

4. **模块化设计**：
   - 每个模块有明确的边界和接口
   - 模块间通过事件总线或服务接口通信
   - 支持模块的动态加载/卸载

### 3. 新架构设计

#### 3.1 整体架构分层

```
┌─────────────────────────────────────────────────────────┐
│                   Presentation Layer                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ MainWindow   │  │  Widgets     │  │   Panels     │  │
│  │              │  │              │  │              │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
                          ↓ ↑
┌─────────────────────────────────────────────────────────┐
│                  Application Layer                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  Services    │  │  Controllers │  │   Managers   │  │
│  │              │  │              │  │              │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
                          ↓ ↑
┌─────────────────────────────────────────────────────────┐
│                    Domain Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │    Models    │  │   Entities   │  │  Repositories│  │
│  │              │  │              │  │              │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
                          ↓ ↑
┌─────────────────────────────────────────────────────────┐
│                Infrastructure Layer                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  EventBus    │  │   Network    │  │  Rendering   │  │
│  │              │  │              │  │              │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
```

#### 3.2 新的目录结构

```
rbc/editor/
├── runtime/
│   ├── include/RBCEditorRuntime/
│   │   ├── ui/                          # Presentation Layer
│   │   │   ├── windows/                 # 窗口
│   │   │   │   ├── IMainWindow.h
│   │   │   │   └── MainWindow.h
│   │   │   ├── widgets/                 # 可复用的Widget
│   │   │   │   ├── IViewportWidget.h
│   │   │   │   ├── ViewportWidget.h
│   │   │   │   ├── ISceneHierarchyWidget.h
│   │   │   │   └── ...
│   │   │   ├── panels/                  # 功能面板
│   │   │   │   ├── IDetailsPanel.h
│   │   │   │   ├── DetailsPanel.h
│   │   │   │   ├── IResultPanel.h
│   │   │   │   └── ...
│   │   │   └── dialogs/                 # 对话框
│   │   │
│   │   ├── application/                 # Application Layer
│   │   │   ├── services/                # 应用服务
│   │   │   │   ├── ISceneService.h
│   │   │   │   ├── SceneService.h
│   │   │   │   ├── IAnimationService.h
│   │   │   │   ├── AnimationService.h
│   │   │   │   ├── IWorkflowService.h
│   │   │   │   ├── WorkflowService.h
│   │   │   │   ├── ILayoutService.h
│   │   │   │   ├── LayoutService.h
│   │   │   │   └── ServiceLocator.h     # 服务定位器
│   │   │   │
│   │   │   ├── controllers/             # 控制器（协调UI和Service）
│   │   │   │   ├── SceneController.h
│   │   │   │   ├── AnimationController.h
│   │   │   │   ├── SelectionController.h
│   │   │   │   └── ...
│   │   │   │
│   │   │   └── commands/                # 命令模式（支持Undo/Redo）
│   │   │       ├── ICommand.h
│   │   │       ├── CommandHistory.h
│   │   │       ├── TransformCommand.h
│   │   │       └── ...
│   │   │
│   │   ├── domain/                      # Domain Layer
│   │   │   ├── models/                  # 领域模型
│   │   │   │   ├── Scene.h
│   │   │   │   ├── Entity.h
│   │   │   │   ├── Animation.h
│   │   │   │   └── ...
│   │   │   │
│   │   │   ├── repositories/            # 数据仓库接口
│   │   │   │   ├── ISceneRepository.h
│   │   │   │   ├── IAnimationRepository.h
│   │   │   │   └── ...
│   │   │   │
│   │   │   └── workflows/               # 工作流状态（领域逻辑）
│   │   │       ├── IWorkflowState.h
│   │   │       ├── SceneEditingWorkflow.h
│   │   │       ├── Text2ImageWorkflow.h
│   │   │       └── WorkflowStateMachine.h
│   │   │
│   │   ├── infrastructure/              # Infrastructure Layer
│   │   │   ├── events/                  # 事件系统
│   │   │   │   ├── IEventBus.h
│   │   │   │   ├── EventBus.h
│   │   │   │   ├── Event.h
│   │   │   │   └── EventTypes.h
│   │   │   │
│   │   │   ├── network/                 # 网络层
│   │   │   │   ├── IHttpClient.h
│   │   │   │   ├── HttpClient.h
│   │   │   │   ├── ISceneSyncClient.h
│   │   │   │   └── SceneSyncClient.h
│   │   │   │
│   │   │   ├── rendering/               # 渲染基础设施
│   │   │   │   ├── IRenderer.h
│   │   │   │   ├── EditorEngine.h
│   │   │   │   └── ...
│   │   │   │
│   │   │   └── repositories/            # 数据仓库实现
│   │   │       ├── SceneRepository.h
│   │   │       ├── AnimationRepository.h
│   │   │       └── ...
│   │   │
│   │   ├── nodes/                       # 节点编辑器模块（独立）
│   │   │   ├── INodeEditor.h
│   │   │   ├── NodeEditor.h
│   │   │   ├── models/
│   │   │   ├── factories/
│   │   │   └── widgets/
│   │   │
│   │   ├── plugins/                     # 插件系统（可选，未来扩展）
│   │   │   ├── IEditorPlugin.h
│   │   │   └── PluginManager.h
│   │   │
│   │   └── utils/                       # 工具类
│   │       ├── Logger.h
│   │       ├── Config.h
│   │       └── ...
│   │
│   └── src/                             # 实现文件（镜像include结构）
│
├── tests/                               # 单元测试
│   ├── ui/
│   ├── application/
│   ├── domain/
│   └── mocks/                           # Mock对象
│
└── examples/                            # 示例和集成测试
```

#### 3.3 核心接口设计

##### 3.3.1 Service Layer接口

```cpp
// ISceneService.h - 场景管理服务
class ISceneService {
public:
    virtual ~ISceneService() = default;
    
    // 场景操作
    virtual bool loadScene(const QString& path) = 0;
    virtual bool saveScene(const QString& path) = 0;
    virtual Scene* getCurrentScene() = 0;
    
    // 实体操作
    virtual Entity* createEntity(const QString& name) = 0;
    virtual bool deleteEntity(int entityId) = 0;
    virtual Entity* getEntity(int entityId) = 0;
    virtual QList<Entity*> getAllEntities() = 0;
    
    // 选择管理
    virtual void selectEntity(int entityId) = 0;
    virtual int getSelectedEntityId() const = 0;
    
signals:
    void sceneLoaded(Scene* scene);
    void sceneSaved();
    void entityCreated(Entity* entity);
    void entityDeleted(int entityId);
    void entityModified(Entity* entity);
    void selectionChanged(int entityId);
};

// IAnimationService.h - 动画管理服务
class IAnimationService {
public:
    virtual ~IAnimationService() = default;
    
    virtual bool loadAnimation(const QString& name) = 0;
    virtual void playAnimation(const QString& name) = 0;
    virtual void pauseAnimation() = 0;
    virtual void stopAnimation() = 0;
    virtual void setFrame(int frame) = 0;
    virtual int getCurrentFrame() const = 0;
    virtual bool isPlaying() const = 0;
    
signals:
    void animationLoaded(const QString& name);
    void frameChanged(int frame);
    void playStateChanged(bool playing);
};

// IWorkflowService.h - 工作流管理服务
class IWorkflowService {
public:
    virtual ~IWorkflowService() = default;
    
    virtual bool switchWorkflow(WorkflowType type) = 0;
    virtual WorkflowType getCurrentWorkflow() const = 0;
    virtual IWorkflowState* getCurrentState() = 0;
    
signals:
    void workflowChanged(WorkflowType newType, WorkflowType oldType);
};

// ILayoutService.h - 布局管理服务
class ILayoutService {
public:
    virtual ~ILayoutService() = default;
    
    virtual void saveLayout(const QString& name) = 0;
    virtual bool loadLayout(const QString& name) = 0;
    virtual void resetLayout() = 0;
    
    virtual void showDock(const QString& dockName) = 0;
    virtual void hideDock(const QString& dockName) = 0;
    virtual void toggleDock(const QString& dockName) = 0;
    
signals:
    void layoutChanged();
};
```

##### 3.3.2 Service Locator模式

```cpp
// ServiceLocator.h - 全局服务访问点
class ServiceLocator {
public:
    // 单例访问
    static ServiceLocator& instance();
    
    // 服务注册
    void registerService(ISceneService* service);
    void registerService(IAnimationService* service);
    void registerService(IWorkflowService* service);
    void registerService(ILayoutService* service);
    
    // 服务获取
    ISceneService* sceneService();
    IAnimationService* animationService();
    IWorkflowService* workflowService();
    ILayoutService* layoutService();
    
    // 测试支持：清除所有服务（用于Mock）
    void clear();
    
private:
    ServiceLocator() = default;
    std::unique_ptr<ISceneService> sceneService_;
    std::unique_ptr<IAnimationService> animationService_;
    std::unique_ptr<IWorkflowService> workflowService_;
    std::unique_ptr<ILayoutService> layoutService_;
};
```

##### 3.3.3 Controller层

```cpp
// SceneController.h - 场景控制器
class SceneController : public QObject {
    Q_OBJECT
public:
    explicit SceneController(ISceneService* sceneService, 
                            IEventBus* eventBus,
                            QObject* parent = nullptr);
    
public slots:
    // UI触发的操作
    void onEntitySelected(int entityId);
    void onEntityTransformChanged(int entityId, const Transform& transform);
    void onCreateEntityRequested(const QString& name);
    void onDeleteEntityRequested(int entityId);
    
private slots:
    // Service事件响应
    void onSceneServiceEntityCreated(Entity* entity);
    void onSceneServiceEntityDeleted(int entityId);
    
private:
    ISceneService* sceneService_;
    IEventBus* eventBus_;
    QList<QMetaObject::Connection> connections_;
};

// AnimationController.h - 动画控制器
class AnimationController : public QObject {
    Q_OBJECT
public:
    explicit AnimationController(IAnimationService* animService,
                                 ISceneService* sceneService,
                                 IEventBus* eventBus,
                                 QObject* parent = nullptr);
    
public slots:
    void onAnimationSelected(const QString& name);
    void onPlayRequested();
    void onPauseRequested();
    void onFrameChanged(int frame);
    
private:
    IAnimationService* animService_;
    ISceneService* sceneService_;
    IEventBus* eventBus_;
};
```

##### 3.3.4 Widget接口

```cpp
// IViewportWidget.h - Viewport接口
class IViewportWidget {
public:
    virtual ~IViewportWidget() = default;
    
    virtual void setRenderer(IRenderer* renderer) = 0;
    virtual void setInteractionEnabled(bool enabled) = 0;
    virtual void highlightEntity(int entityId) = 0;
    virtual void clearHighlight() = 0;
    
    virtual QWidget* asWidget() = 0;
};

// ISceneHierarchyWidget.h - 场景层级接口
class ISceneHierarchyWidget {
public:
    virtual ~ISceneHierarchyWidget() = default;
    
    virtual void setScene(Scene* scene) = 0;
    virtual void selectEntity(int entityId) = 0;
    virtual void refresh() = 0;
    
    virtual QWidget* asWidget() = 0;
};

// IDetailsPanel.h - 属性面板接口
class IDetailsPanel {
public:
    virtual ~IDetailsPanel() = default;
    
    virtual void showEntity(int entityId) = 0;
    virtual void clear() = 0;
    virtual void setReadOnly(bool readOnly) = 0;
    
    virtual QWidget* asWidget() = 0;
};
```

#### 3.4 新架构的数据流

```
┌─────────────┐
│   UI Event  │  (用户点击、输入)
└──────┬──────┘
       ↓
┌─────────────┐
│ Controller  │  (解析UI事件，调用Service)
└──────┬──────┘
       ↓
┌─────────────┐
│  Service    │  (执行业务逻辑)
└──────┬──────┘
       ↓
┌─────────────┐
│ Repository  │  (数据持久化/网络同步)
└──────┬──────┘
       ↓
┌─────────────┐
│  EventBus   │  (发布变更事件)
└──────┬──────┘
       ↓
┌─────────────┐
│   Widget    │  (更新UI显示)
└─────────────┘
```

#### 3.5 新架构的优势

1. **职责清晰**：
   - UI层只负责显示和用户交互
   - Controller协调UI和Service
   - Service包含业务逻辑
   - Repository管理数据

2. **低耦合**：
   - 通过接口抽象依赖
   - Service Locator管理全局服务
   - EventBus实现松耦合通信

3. **易测试**：
   - 所有核心组件都有接口
   - 可以轻松创建Mock对象
   - 支持单元测试和集成测试

4. **易扩展**：
   - 新增功能只需添加新的Service和Controller
   - 不需要修改现有代码
   - 符合开闭原则

5. **易维护**：
   - 代码组织清晰
   - 依赖关系明确
   - 单一职责便于理解

### 4. 迁移策略

#### 4.1 渐进式重构

不建议一次性重写所有代码，而是采用渐进式重构策略：

**阶段1：建立基础设施** (1-2周)
- 创建新的目录结构
- 实现Service接口和ServiceLocator
- 实现EventBus改进版（如果需要）
- 编写单元测试框架

**阶段2：重构核心服务** (2-3周)
- 实现SceneService
- 实现AnimationService
- 实现WorkflowService
- 实现LayoutService
- 为每个Service编写单元测试

**阶段3：重构控制器层** (1-2周)
- 实现SceneController
- 实现AnimationController
- 实现SelectionController
- 连接Service和EventBus

**阶段4：重构UI层** (2-3周)
- 为现有Widget添加接口
- 将Widget与Service解耦
- 通过Controller连接UI和Service
- 移除对EditorContext的直接依赖

**阶段5：清理和优化** (1周)
- 删除旧代码
- 优化性能
- 补充文档
- 完善测试覆盖率

#### 4.2 兼容性保证

在迁移过程中：
- 保持现有功能正常工作
- 新旧代码可以并存
- 通过适配器模式桥接新旧接口
- 逐步替换旧代码

#### 4.3 质量保证

- 每个阶段都要有完整的测试
- 保证功能不退化
- Code Review机制
- 持续集成和自动化测试

### 5. 示例代码：重构前后对比

#### 5.1 重构前：MainWindow创建和初始化

```cpp
// 旧代码：MainWindow承担过多职责
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      context_(new rbc::EditorContext),
      layoutManager_(nullptr),
      containerManager_(nullptr) {
    
    // 直接创建和管理所有组件
    context_->httpClient = new rbc::HttpClient(this);
    context_->workflowManager = new rbc::WorkflowManager(this);
    context_->editorScene = new rbc::EditorScene();
    // ... 创建大量组件
    
    // 手动连接信号槽
    connect(context_->workflowManager, &rbc::WorkflowManager::workflowChanged,
            this, &MainWindow::onWorkflowChanged);
    // ... 大量信号槽连接
}
```

#### 5.2 重构后：使用Service和Controller

```cpp
// 新代码：职责清晰，依赖注入
class MainWindow : public QMainWindow, public IMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr) 
        : QMainWindow(parent) {
        
        // 从ServiceLocator获取服务
        sceneService_ = ServiceLocator::instance().sceneService();
        workflowService_ = ServiceLocator::instance().workflowService();
        layoutService_ = ServiceLocator::instance().layoutService();
        
        // 创建控制器
        sceneController_ = new SceneController(
            sceneService_, EventBus::instance(), this);
        animController_ = new AnimationController(
            animService_, sceneService_, EventBus::instance(), this);
        
        // UI组件通过工厂或Builder创建
        setupUI();
    }
    
private:
    void setupUI() {
        // 使用LayoutService管理布局
        layoutService_->setupMainWindow(this);
        
        // 创建UI组件（通过工厂）
        sceneHierarchy_ = WidgetFactory::createSceneHierarchy(this);
        detailsPanel_ = WidgetFactory::createDetailsPanel(this);
        viewport_ = WidgetFactory::createViewport(this);
        
        // 连接UI到Controller（单向，简单）
        connect(sceneHierarchy_->asWidget(), &SceneHierarchyWidget::entitySelected,
                sceneController_, &SceneController::onEntitySelected);
    }
    
private:
    // 服务引用（不拥有）
    ISceneService* sceneService_;
    IWorkflowService* workflowService_;
    ILayoutService* layoutService_;
    
    // 控制器（拥有）
    SceneController* sceneController_;
    AnimationController* animController_;
    
    // UI组件
    ISceneHierarchyWidget* sceneHierarchy_;
    IDetailsPanel* detailsPanel_;
    IViewportWidget* viewport_;
};
```

#### 5.3 重构前：组件间通信

```cpp
// 旧代码：通过EditorContext访问其他组件，耦合度高
void SceneHierarchyWidget::onEntityClicked(int entityId) {
    // 直接访问context中的其他组件
    if (context_->detailsPanel) {
        auto entity = context_->editorScene->getEntity(entityId);
        context_->detailsPanel->showEntity(entity);
    }
    
    if (context_->viewportWidget) {
        context_->viewportWidget->highlightEntity(entityId);
    }
    
    // 发送事件
    EventBus::instance().publish(EventType::EntitySelected, entityId);
}
```

#### 5.4 重构后：通过Service和EventBus通信

```cpp
// 新代码：通过Controller和EventBus，松耦合
class SceneHierarchyWidget : public QWidget, public ISceneHierarchyWidget {
signals:
    void entitySelected(int entityId);  // 只发送信号，不关心谁处理
};

// Controller处理业务逻辑
void SceneController::onEntitySelected(int entityId) {
    // 更新Service状态
    sceneService_->selectEntity(entityId);
    
    // 发布事件（其他组件监听）
    eventBus_->publish(Event(EventType::EntitySelected, entityId));
}

// DetailsPanel监听事件并更新
class DetailsPanel : public QWidget, public IDetailsPanel {
public:
    DetailsPanel() {
        // 订阅事件
        EventBus::instance().subscribe(EventType::EntitySelected,
            [this](const Event& e) {
                int entityId = e.data.toInt();
                showEntity(entityId);
            });
    }
};

// ViewportWidget同样监听事件
class ViewportWidget : public QWidget, public IViewportWidget {
public:
    ViewportWidget() {
        EventBus::instance().subscribe(EventType::EntitySelected,
            [this](const Event& e) {
                int entityId = e.data.toInt();
                highlightEntity(entityId);
            });
    }
};
```

### 6. 测试策略

#### 6.1 单元测试示例

```cpp
// tests/application/test_scene_service.cpp
class MockSceneRepository : public ISceneRepository {
public:
    MOCK_METHOD(Scene*, loadScene, (const QString&), (override));
    MOCK_METHOD(bool, saveScene, (Scene*, const QString&), (override));
    MOCK_METHOD(Entity*, getEntity, (int), (override));
};

class SceneServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockRepo_ = new MockSceneRepository();
        service_ = new SceneService(mockRepo_);
    }
    
    void TearDown() override {
        delete service_;
        delete mockRepo_;
    }
    
    MockSceneRepository* mockRepo_;
    SceneService* service_;
};

TEST_F(SceneServiceTest, SelectEntity_EmitsSignal) {
    // Arrange
    int entityId = 42;
    QSignalSpy spy(service_, &ISceneService::selectionChanged);
    
    // Act
    service_->selectEntity(entityId);
    
    // Assert
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toInt(), entityId);
}

TEST_F(SceneServiceTest, CreateEntity_CallsRepository) {
    // Arrange
    QString name = "TestEntity";
    EXPECT_CALL(*mockRepo_, createEntity(name))
        .WillOnce(Return(new Entity(1, name)));
    
    // Act
    auto entity = service_->createEntity(name);
    
    // Assert
    ASSERT_NE(entity, nullptr);
    EXPECT_EQ(entity->getName(), name);
}
```

#### 6.2 集成测试示例

```cpp
// tests/integration/test_scene_workflow.cpp
class SceneWorkflowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 使用真实Service，但Mock Repository和Network
        setupServices();
        setupUI();
    }
    
    void setupServices() {
        mockRepo_ = new MockSceneRepository();
        sceneService_ = new SceneService(mockRepo_);
        ServiceLocator::instance().registerService(sceneService_);
    }
    
    void setupUI() {
        mainWindow_ = new MainWindow();
        controller_ = new SceneController(sceneService_, &EventBus::instance());
    }
};

TEST_F(SceneWorkflowTest, SelectEntityUpdatesDetailPanel) {
    // Arrange
    int entityId = 1;
    auto entity = new Entity(entityId, "Test");
    EXPECT_CALL(*mockRepo_, getEntity(entityId))
        .WillOnce(Return(entity));
    
    // Act：模拟用户点击
    mainWindow_->sceneHierarchy()->selectEntity(entityId);
    QApplication::processEvents();  // 处理Qt事件循环
    
    // Assert：验证DetailPanel显示了正确的实体
    EXPECT_EQ(mainWindow_->detailsPanel()->currentEntityId(), entityId);
}
```

#### 6.3 UI Mock测试示例

```cpp
// tests/mocks/mock_viewport_widget.h
class MockViewportWidget : public IViewportWidget {
public:
    MOCK_METHOD(void, setRenderer, (IRenderer*), (override));
    MOCK_METHOD(void, highlightEntity, (int), (override));
    MOCK_METHOD(void, clearHighlight, (), (override));
    MOCK_METHOD(QWidget*, asWidget, (), (override));
};

// 在测试中使用Mock
TEST(SelectionTest, ViewportHighlightsSelectedEntity) {
    MockViewportWidget mockViewport;
    SceneController controller(sceneService, &EventBus::instance());
    
    // 期望highlight被调用
    EXPECT_CALL(mockViewport, highlightEntity(42));
    
    // 触发选择事件
    controller.onEntitySelected(42);
}
```

### 7. 未来扩展方向

#### 7.1 插件系统

基于新架构，可以轻松实现插件系统：

```cpp
class IEditorPlugin {
public:
    virtual ~IEditorPlugin() = default;
    
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual void initialize(ServiceLocator& services) = 0;
    virtual void shutdown() = 0;
    
    virtual QList<QAction*> menuActions() = 0;
    virtual QList<QWidget*> dockWidgets() = 0;
};

class PluginManager {
public:
    bool loadPlugin(const QString& path);
    void unloadPlugin(const QString& name);
    QList<IEditorPlugin*> loadedPlugins() const;
};
```

#### 7.2 命令系统（Undo/Redo）

```cpp
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual QString description() const = 0;
};

class CommandHistory {
public:
    void execute(std::unique_ptr<ICommand> cmd);
    void undo();
    void redo();
    void clear();
    
    bool canUndo() const;
    bool canRedo() const;
};
```

#### 7.3 脚本系统

通过Service接口，可以方便地暴露给脚本语言：

```cpp
class ScriptEngine {
public:
    void registerService(const QString& name, ISceneService* service);
    void executeScript(const QString& script);
    QVariant evaluateExpression(const QString& expr);
};
```

### 8. 特别案例：Animation Playback 模块重构

#### 8.1 当前Animation Playback的问题分析

当前动画播放功能存在以下严重的架构问题：

**问题1：职责分散且耦合严重**
- `AnimationPlayer`：UI控件（播放控制）
- `AnimationPlaybackManager`：实际播放逻辑（应用变换）
- `AnimationController`：控制器（连接两者）
- `ResultPanel`：显示动画列表
- `EditorScene`：接收动画变换

这5个组件通过`EditorContext`强耦合，难以独立测试和复用。

**问题2：与主场景编辑器耦合**
- 动画播放直接修改`EditorScene`
- 无法独立预览动画而不影响主编辑场景
- 无法同时查看多个动画结果
- 无法在独立窗口中查看动画

**问题3：缺乏灵活性**
- 不支持多个动画结果的对比查看
- 不支持独立窗口播放
- 不支持动画导出预览
- 不支持时间轴高级编辑

**问题4：测试困难**
- 组件高度耦合，难以Mock
- 播放逻辑与UI混合
- 无法独立测试播放逻辑

#### 8.2 新设计：独立的结果查看器系统

##### 8.2.1 核心设计思想

1. **独立的预览场景**：动画播放使用独立的Scene实例，不影响主编辑场景
2. **模块化的查看器**：支持嵌入式和独立窗口两种模式
3. **统一的结果管理**：通过ResultService管理所有类型的执行结果（动画、图片、视频等）
4. **清晰的分层**：Viewer（UI）→ Service（逻辑）→ Repository（数据）

##### 8.2.2 新架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                  Result Viewer System                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────────┐        ┌──────────────────┐         │
│  │ ResultViewer     │        │ AnimationViewer  │         │
│  │   (Abstract)     │◄───────│   (Concrete)     │         │
│  └────────┬─────────┘        └──────────────────┘         │
│           │                                                 │
│           │ uses                                            │
│           ▼                                                 │
│  ┌──────────────────┐                                      │
│  │ IResultService   │                                      │
│  │                  │                                      │
│  │ - getResults()   │                                      │
│  │ - loadResult()   │                                      │
│  │ - exportResult() │                                      │
│  └────────┬─────────┘                                      │
│           │                                                 │
│           │ implements                                      │
│           ▼                                                 │
│  ┌──────────────────┐        ┌──────────────────┐         │
│  │ ResultService    │───────►│ PreviewScene     │         │
│  │                  │        │   (Isolated)     │         │
│  └────────┬─────────┘        └──────────────────┘         │
│           │                                                 │
│           │ uses                                            │
│           ▼                                                 │
│  ┌──────────────────┐                                      │
│  │ IResultRepository│                                      │
│  │                  │                                      │
│  │ - fetch()        │                                      │
│  │ - cache()        │                                      │
│  └──────────────────┘                                      │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

##### 8.2.3 详细接口设计

```cpp
// ============================================================================
// Result Domain Models
// ============================================================================

namespace rbc {

/**
 * 结果类型枚举
 */
enum class ResultType {
    Animation,      // 动画结果
    Image,          // 静态图片
    Video,          // 视频
    Mesh,           // 网格模型
    PointCloud,     // 点云
    Custom          // 自定义类型
};

/**
 * 执行结果元数据
 */
struct ResultMetadata {
    QString id;                    // 唯一标识
    QString name;                  // 显示名称
    ResultType type;               // 结果类型
    QString sourceNode;            // 来源节点
    QDateTime timestamp;           // 生成时间
    QMap<QString, QVariant> properties;  // 额外属性
};

/**
 * 动画结果数据
 */
struct AnimationResultData {
    QString name;
    int totalFrames;
    float fps;
    const AnimationClip* clip;     // 原始动画数据
    QList<int> entityIds;          // 涉及的实体ID列表
};

/**
 * 执行结果基类
 */
class IResult {
public:
    virtual ~IResult() = default;
    
    virtual ResultMetadata metadata() const = 0;
    virtual ResultType type() const = 0;
    virtual QVariant data() const = 0;  // 实际数据（由具体类型决定）
};

/**
 * 动画结果
 */
class AnimationResult : public IResult {
public:
    explicit AnimationResult(const ResultMetadata& meta, 
                            const AnimationResultData& data);
    
    ResultMetadata metadata() const override;
    ResultType type() const override { return ResultType::Animation; }
    QVariant data() const override;
    
    const AnimationResultData& animationData() const { return data_; }
    
private:
    ResultMetadata metadata_;
    AnimationResultData data_;
};

// ============================================================================
// Result Service Layer
// ============================================================================

/**
 * 结果服务接口
 */
class IResultService : public QObject {
    Q_OBJECT
public:
    virtual ~IResultService() = default;
    
    // 结果查询
    virtual QList<IResult*> getAllResults() const = 0;
    virtual IResult* getResult(const QString& id) const = 0;
    virtual QList<IResult*> getResultsByType(ResultType type) const = 0;
    
    // 结果管理
    virtual void addResult(std::unique_ptr<IResult> result) = 0;
    virtual void removeResult(const QString& id) = 0;
    virtual void clearResults() = 0;
    
    // 结果预览（使用独立场景）
    virtual IPreviewScene* createPreviewScene(const QString& resultId) = 0;
    virtual void destroyPreviewScene(IPreviewScene* scene) = 0;
    
    // 结果导出
    virtual bool exportResult(const QString& resultId, const QString& path) = 0;
    
signals:
    void resultAdded(IResult* result);
    void resultRemoved(const QString& id);
    void resultsCleared();
};

/**
 * 预览场景接口（隔离的场景，用于结果预览）
 */
class IPreviewScene {
public:
    virtual ~IPreviewScene() = default;
    
    // 场景控制
    virtual void loadResult(const QString& resultId) = 0;
    virtual void clear() = 0;
    
    // 动画控制（如果是动画类型）
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void setFrame(int frame) = 0;
    virtual int currentFrame() const = 0;
    virtual bool isPlaying() const = 0;
    
    // 渲染输出（用于UI显示）
    virtual IRenderer* renderer() = 0;
    
    // 场景信息
    virtual QString resultId() const = 0;
    virtual ResultType resultType() const = 0;
};

// ============================================================================
// Viewer Layer (UI)
// ============================================================================

/**
 * 结果查看器基类
 */
class IResultViewer : public QWidget {
    Q_OBJECT
public:
    virtual ~IResultViewer() = default;
    
    // 显示结果
    virtual void setResult(IResult* result) = 0;
    virtual IResult* currentResult() const = 0;
    
    // 控制
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void reset() = 0;
    
    // 窗口模式
    virtual bool isStandalone() const = 0;
    virtual QWidget* asWidget() = 0;
    
signals:
    void resultChanged(IResult* result);
    void playStateChanged(bool playing);
    void closed();  // 独立窗口关闭时
};

/**
 * 动画查看器（具体实现）
 */
class AnimationViewer : public IResultViewer {
    Q_OBJECT
public:
    explicit AnimationViewer(IResultService* resultService,
                            QWidget* parent = nullptr);
    ~AnimationViewer() override;
    
    // IResultViewer 接口
    void setResult(IResult* result) override;
    IResult* currentResult() const override { return currentResult_; }
    
    void play() override;
    void pause() override;
    void reset() override;
    
    bool isStandalone() const override { return isStandalone_; }
    QWidget* asWidget() override { return this; }
    
    // 特有功能
    void setFrame(int frame);
    int currentFrame() const;
    
    // 创建独立窗口
    static AnimationViewer* createStandaloneWindow(IResultService* resultService);
    
private:
    void setupUI();
    void updatePlaybackControls();
    void onPreviewSceneFrameChanged(int frame);
    
    IResultService* resultService_;
    IResult* currentResult_;
    IPreviewScene* previewScene_;
    
    // UI组件
    ViewportWidget* viewportWidget_;      // 渲染视口
    QSlider* timelineSlider_;             // 时间轴
    QPushButton* playPauseButton_;        // 播放/暂停
    QPushButton* exportButton_;           // 导出按钮
    QLabel* frameLabel_;                  // 帧信息
    QLabel* infoLabel_;                   // 动画信息
    
    bool isStandalone_;                   // 是否为独立窗口
};

/**
 * 结果浏览器面板（显示结果列表）
 */
class ResultBrowserPanel : public QWidget {
    Q_OBJECT
public:
    explicit ResultBrowserPanel(IResultService* resultService,
                               QWidget* parent = nullptr);
    
    // 刷新结果列表
    void refresh();
    
    // 过滤
    void setTypeFilter(ResultType type);
    void clearFilter();
    
signals:
    void resultSelected(IResult* result);
    void openInNewWindowRequested(IResult* result);
    
private slots:
    void onResultItemClicked(QListWidgetItem* item);
    void onResultItemDoubleClicked(QListWidgetItem* item);
    void onOpenInWindowClicked();
    void onExportClicked();
    void onDeleteClicked();
    
private:
    void setupUI();
    void updateResultList();
    void populateContextMenu();
    
    IResultService* resultService_;
    
    QListWidget* resultList_;
    QComboBox* typeFilterCombo_;
    QPushButton* refreshButton_;
    QMenu* contextMenu_;
    
    IResult* selectedResult_;
};

// ============================================================================
// Implementation Classes
// ============================================================================

/**
 * 结果服务实现
 */
class ResultService : public IResultService {
    Q_OBJECT
public:
    explicit ResultService(QObject* parent = nullptr);
    ~ResultService() override;
    
    // IResultService 接口实现
    QList<IResult*> getAllResults() const override;
    IResult* getResult(const QString& id) const override;
    QList<IResult*> getResultsByType(ResultType type) const override;
    
    void addResult(std::unique_ptr<IResult> result) override;
    void removeResult(const QString& id) override;
    void clearResults() override;
    
    IPreviewScene* createPreviewScene(const QString& resultId) override;
    void destroyPreviewScene(IPreviewScene* scene) override;
    
    bool exportResult(const QString& resultId, const QString& path) override;
    
private:
    // 结果存储
    QMap<QString, std::unique_ptr<IResult>> results_;
    
    // 预览场景池
    QList<std::unique_ptr<IPreviewScene>> previewScenes_;
    
    // 结果仓库（用于网络同步等）
    IResultRepository* repository_;
};

/**
 * 预览场景实现
 */
class PreviewScene : public QObject, public IPreviewScene {
    Q_OBJECT
public:
    explicit PreviewScene(QObject* parent = nullptr);
    ~PreviewScene() override;
    
    // IPreviewScene 接口
    void loadResult(const QString& resultId) override;
    void clear() override;
    
    void play() override;
    void pause() override;
    void setFrame(int frame) override;
    int currentFrame() const override;
    bool isPlaying() const override;
    
    IRenderer* renderer() override { return renderer_; }
    
    QString resultId() const override { return resultId_; }
    ResultType resultType() const override { return resultType_; }
    
signals:
    void frameChanged(int frame);
    void playStateChanged(bool playing);
    
private:
    void applyAnimationFrame(int frame);
    
    QString resultId_;
    ResultType resultType_;
    
    // 独立的场景数据
    std::unique_ptr<EditorScene> scene_;
    
    // 渲染器
    IRenderer* renderer_;
    
    // 动画播放
    const AnimationClip* currentClip_;
    QTimer* playbackTimer_;
    int currentFrame_;
    bool isPlaying_;
};

} // namespace rbc
```

##### 8.2.4 使用场景示例

**场景1：在主窗口中嵌入式查看动画结果**

```cpp
// 在MainWindow中设置
void MainWindow::setupResultViewer() {
    // 创建结果服务
    auto* resultService = new ResultService(this);
    ServiceLocator::instance().registerResultService(resultService);
    
    // 创建结果浏览器面板（左侧或底部Dock）
    resultBrowserPanel_ = new ResultBrowserPanel(resultService, this);
    auto* browserDock = new QDockWidget("Results", this);
    browserDock->setWidget(resultBrowserPanel_);
    addDockWidget(Qt::BottomDockWidgetArea, browserDock);
    
    // 创建嵌入式动画查看器（右侧Dock）
    animationViewer_ = new AnimationViewer(resultService, this);
    auto* viewerDock = new QDockWidget("Animation Preview", this);
    viewerDock->setWidget(animationViewer_);
    addDockWidget(Qt::RightDockWidgetArea, viewerDock);
    
    // 连接信号
    connect(resultBrowserPanel_, &ResultBrowserPanel::resultSelected,
            animationViewer_, &AnimationViewer::setResult);
    
    connect(resultBrowserPanel_, &ResultBrowserPanel::openInNewWindowRequested,
            this, &MainWindow::openResultInNewWindow);
}

// 当节点执行完成，产生动画结果时
void MainWindow::onNodeExecutionFinished(const QJsonObject& output) {
    // 从输出中解析动画数据
    QString animName = output["animation_name"].toString();
    
    // 创建结果元数据
    ResultMetadata meta;
    meta.id = QUuid::createUuid().toString();
    meta.name = animName;
    meta.type = ResultType::Animation;
    meta.timestamp = QDateTime::currentDateTime();
    
    // 获取动画数据（从SceneSync）
    const auto* clip = sceneSyncManager_->sceneSync()->getAnimation(animName.toStdString());
    
    AnimationResultData animData;
    animData.name = animName;
    animData.totalFrames = clip->total_frames;
    animData.fps = clip->fps;
    animData.clip = clip;
    
    // 添加到结果服务
    auto result = std::make_unique<AnimationResult>(meta, animData);
    ServiceLocator::instance().resultService()->addResult(std::move(result));
}

// 打开独立窗口
void MainWindow::openResultInNewWindow(IResult* result) {
    auto* viewer = AnimationViewer::createStandaloneWindow(
        ServiceLocator::instance().resultService());
    
    viewer->setResult(result);
    viewer->setAttribute(Qt::WA_DeleteOnClose);
    viewer->show();
}
```

**场景2：独立窗口预览，不影响主场景**

```cpp
// 用户双击Result列表项
void ResultBrowserPanel::onResultItemDoubleClicked(QListWidgetItem* item) {
    auto* result = getResultFromItem(item);
    if (result) {
        emit openInNewWindowRequested(result);
    }
}

// 创建独立的预览窗口
AnimationViewer* AnimationViewer::createStandaloneWindow(
    IResultService* resultService) {
    
    auto* viewer = new AnimationViewer(resultService, nullptr);
    viewer->isStandalone_ = true;
    viewer->setWindowTitle("Animation Preview");
    viewer->resize(1280, 720);
    
    // 独立窗口有自己的工具栏
    viewer->setupStandaloneToolbar();
    
    return viewer;
}
```

**场景3：对比多个动画结果**

```cpp
class AnimationComparisonWindow : public QWidget {
public:
    AnimationComparisonWindow(IResultService* resultService) {
        // 创建多个并排的AnimationViewer
        auto* layout = new QHBoxLayout(this);
        
        viewer1_ = new AnimationViewer(resultService, this);
        viewer2_ = new AnimationViewer(resultService, this);
        
        layout->addWidget(viewer1_);
        layout->addWidget(viewer2_);
        
        // 同步播放控制
        connect(viewer1_, &AnimationViewer::playStateChanged,
                viewer2_, [this](bool playing) {
                    if (playing) viewer2_->play();
                    else viewer2_->pause();
                });
    }
    
    void setResults(IResult* result1, IResult* result2) {
        viewer1_->setResult(result1);
        viewer2_->setResult(result2);
    }
    
private:
    AnimationViewer* viewer1_;
    AnimationViewer* viewer2_;
};
```

##### 8.2.5 新架构的优势

**1. 完全解耦主编辑场景**
- 预览使用独立的`PreviewScene`
- 主场景编辑不受动画播放影响
- 可以同时编辑场景和预览动画

**2. 灵活的显示模式**
- 嵌入式查看器：集成在主窗口Dock中
- 独立窗口：弹出独立窗口预览
- 对比模式：并排对比多个结果

**3. 统一的结果管理**
- 所有类型的结果（动画、图片、视频）统一管理
- 便于添加新的结果类型
- 支持结果导出、缓存、历史记录

**4. 易于测试**
```cpp
// Mock测试示例
class MockResultService : public IResultService {
    MOCK_METHOD(QList<IResult*>, getAllResults, (), (const, override));
    MOCK_METHOD(IResult*, getResult, (const QString&), (const, override));
    // ...
};

TEST(AnimationViewerTest, LoadAnimation) {
    MockResultService mockService;
    AnimationViewer viewer(&mockService);
    
    auto mockResult = createMockAnimationResult();
    EXPECT_CALL(mockService, getResult("test-id"))
        .WillOnce(Return(mockResult));
    
    viewer.setResult(mockResult);
    
    EXPECT_EQ(viewer.currentResult(), mockResult);
}
```

**5. 可扩展性强**
- 新增结果类型只需实现`IResult`接口
- 新增查看器只需实现`IResultViewer`接口
- 支持插件式的结果处理器

##### 8.2.6 目录结构

```
rbc/editor/runtime/
├── include/RBCEditorRuntime/
│   ├── results/                         # 结果系统（新增）
│   │   ├── domain/                      # 领域模型
│   │   │   ├── IResult.h
│   │   │   ├── AnimationResult.h
│   │   │   ├── ImageResult.h
│   │   │   └── ResultMetadata.h
│   │   │
│   │   ├── services/                    # 服务层
│   │   │   ├── IResultService.h
│   │   │   ├── ResultService.h
│   │   │   ├── IPreviewScene.h
│   │   │   └── PreviewScene.h
│   │   │
│   │   ├── viewers/                     # 查看器（UI层）
│   │   │   ├── IResultViewer.h
│   │   │   ├── AnimationViewer.h
│   │   │   ├── ImageViewer.h
│   │   │   ├── ResultBrowserPanel.h
│   │   │   └── AnimationComparisonWindow.h
│   │   │
│   │   └── repositories/                # 数据访问
│   │       ├── IResultRepository.h
│   │       └── ResultRepository.h
│   │
│   └── components/                      # 保留的组件（简化）
│       ├── AnimationTimeline.h          # 纯UI组件（时间轴控件）
│       └── PlaybackControls.h           # 纯UI组件（播放控制）
```

##### 8.2.7 迁移步骤

**Phase 1：创建新的Result系统基础**
1. 实现`IResult`接口和基本结果类型
2. 实现`IResultService`和`ResultService`
3. 实现`IPreviewScene`和`PreviewScene`
4. 编写单元测试

**Phase 2：实现AnimationViewer**
1. 创建`AnimationViewer`类
2. 集成`PreviewScene`
3. 实现播放控制UI
4. 测试独立窗口模式

**Phase 3：创建ResultBrowserPanel**
1. 实现结果列表显示
2. 实现结果过滤和搜索
3. 连接到`ResultService`
4. 测试结果管理

**Phase 4：集成到MainWindow**
1. 替换旧的`ResultPanel`和`AnimationPlayer`
2. 连接NodeEditor执行结果到`ResultService`
3. 测试完整工作流
4. 清理旧代码

**Phase 5：扩展功能**
1. 添加其他结果类型（Image, Video）
2. 实现结果导出功能
3. 添加结果历史和缓存
4. 实现对比查看功能

##### 8.2.8 重构前后对比

**重构前：**
```cpp
// 问题：组件高度耦合，难以独立使用
class MainWindow {
    AnimationPlayer* animationPlayer_;          // UI控件
    AnimationPlaybackManager* playbackManager_; // 播放逻辑
    AnimationController* animController_;       // 控制器
    ResultPanel* resultPanel_;                  // 结果列表
    EditorScene* editorScene_;                  // 直接修改主场景！
};

// 问题：AnimationController访问多个组件
void AnimationController::handleAnimationSelected(const QString& name) {
    // 通过context访问多个组件
    context_->animationPlayer->setAnimation(...);
    context_->playbackManager->setAnimation(...);
    // 直接修改编辑场景
    context_->editorScene->...
}
```

**重构后：**
```cpp
// 清晰：通过Service管理，组件独立
class MainWindow {
    ResultBrowserPanel* resultBrowser_;  // 结果浏览器
    AnimationViewer* animationViewer_;   // 动画查看器（可选，嵌入式）
    // 不再直接持有AnimationPlayer等组件
};

// 清晰：AnimationViewer自包含，可独立使用
class AnimationViewer : public IResultViewer {
public:
    AnimationViewer(IResultService* resultService) {
        // 创建自己的预览场景（隔离的）
        previewScene_ = resultService->createPreviewScene();
        
        // 自己的渲染视口
        viewportWidget_ = new ViewportWidget(previewScene_->renderer());
    }
    
    void setResult(IResult* result) {
        // 加载到预览场景，不影响主场景
        previewScene_->loadResult(result->metadata().id);
    }
};

// 用户代码：创建独立预览窗口非常简单
void openAnimationInNewWindow(const QString& animId) {
    auto* service = ServiceLocator::instance().resultService();
    auto* viewer = AnimationViewer::createStandaloneWindow(service);
    auto* result = service->getResult(animId);
    viewer->setResult(result);
    viewer->show();
}
```

#### 8.3 Animation Playback重构总结

通过这套新设计，我们实现了：

1. **✅ 完全解耦**：预览场景独立于编辑场景
2. **✅ 灵活显示**：支持嵌入和独立窗口模式
3. **✅ 统一管理**：所有结果统一由ResultService管理
4. **✅ 易于测试**：每个组件都可以独立Mock测试
5. **✅ 可扩展**：轻松添加新的结果类型和查看器
6. **✅ 用户友好**：支持对比查看、导出等高级功能

这是一个**典型的领域驱动设计（DDD）案例**，展示了如何：
- 识别核心领域概念（Result、Viewer、PreviewScene）
- 设计清晰的分层架构
- 使用接口抽象和依赖注入
- 实现模块的独立性和可测试性

### 9. 总结

新架构的核心思想：

1. **分层清晰**：UI、Application、Domain、Infrastructure四层分离
2. **依赖倒置**：高层模块不依赖低层模块，都依赖抽象
3. **职责单一**：每个类只做一件事，做好一件事
4. **松耦合**：通过接口和EventBus通信，组件间独立
5. **易测试**：接口抽象+依赖注入+Mock对象
6. **易扩展**：符合开闭原则，新增功能不修改旧代码

通过这套架构，RoboCute Editor可以：
- 支持更多功能而不增加复杂度
- 团队成员可以并行开发不同模块
- 方便进行单元测试和Mock测试
- 代码维护和理解更加容易
- 为未来的插件系统、脚本系统等打下基础