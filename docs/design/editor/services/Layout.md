# Layout Service

## 概述

LayoutService 是 RoboCute Editor 的布局管理服务，负责管理和切换不同工作场景的 UI 布局。它与 Plugin 系统深度集成，能够根据配置文件动态调整视图的可见性、位置和大小，支持在不同的工作流（Workflow）之间无缝切换。

## 核心功能

1. **布局配置管理**：支持多种预设布局和用户自定义布局
2. **动态布局切换**：在不同布局之间平滑过渡，保持应用状态
3. **视图生命周期管理**：按需创建/销毁视图，优化资源使用
4. **布局持久化**：自动保存用户的布局修改，支持导入导出
5. **与 Plugin 系统集成**：自动发现和管理 Plugin 提供的视图

## 架构设计

### 1. 核心概念

#### 1.1 Layout（布局）

一个 Layout 定义了编辑器在特定工作场景下的完整 UI 配置，包括：
- 哪些视图应该显示
- 每个视图的位置、大小、停靠区域
- 视图之间的排列关系（分栏、堆叠等）
- 中央窗口区域的内容

#### 1.2 ViewState（视图状态）

描述单个视图的状态信息：
- 可见性（visible/hidden）
- 停靠区域（Left/Right/Top/Bottom/Center/Floating）
- 尺寸信息
- 堆叠顺序（tabbed）

#### 1.3 Workflow（工作流）

不同的工作场景，每个 Workflow 关联一个或多个 Layout：
- **Scene Editing**：3D 场景编辑（Viewport 中央、Hierarchy 左侧、Detail 右侧、NodeEditor 底部）
- **Node Editing**：节点编辑为主（NodeEditor 中央、Viewport 最小化、Hierarchy 左侧）
- **AIGC Generation**：AI 生成内容（NodeEditor 中央、Result Preview 右侧、Viewport 和 Hierarchy 隐藏）
- **Animation Playback**：动画预览（AnimationViewer 中央、Timeline 底部、Hierarchy 左侧）

### 2. 数据结构

#### 2.1 布局配置 JSON 格式

```json
{
  "layout_id": "scene_editing",
  "layout_name": "3D Scene Editing",
  "description": "Default layout for 3D scene editing with viewport as central widget",
  "version": "1.0",
  "views": [
    {
      "plugin_id": "com.robocute.viewport",
      "view_id": "viewport",
      "dock_area": "Center",
      "visible": true,
      "size": {
        "width": -1,
        "height": -1
      },
      "properties": {
        "show_grid": true,
        "camera_mode": "orbit"
      }
    },
    {
      "plugin_id": "com.robocute.hierarchy",
      "view_id": "scene_hierarchy",
      "dock_area": "Left",
      "visible": true,
      "size": {
        "width": 300,
        "height": -1
      },
      "tab_group": null
    },
    {
      "plugin_id": "com.robocute.detail",
      "view_id": "detail_panel",
      "dock_area": "Right",
      "visible": true,
      "size": {
        "width": 350,
        "height": -1
      }
    },
    {
      "plugin_id": "com.robocute.nodeeditor",
      "view_id": "node_editor",
      "dock_area": "Bottom",
      "visible": true,
      "size": {
        "width": -1,
        "height": 300
      }
    },
    {
      "plugin_id": "com.robocute.connection",
      "view_id": "connection_status",
      "dock_area": "Left",
      "visible": true,
      "size": {
        "width": 300,
        "height": 200
      },
      "tab_group": "left_panel",
      "tab_index": 1
    }
  ],
  "dock_arrangements": {
    "Left": {
      "split_orientation": "vertical",
      "split_ratio": [0.7, 0.3],
      "widgets": ["scene_hierarchy", "connection_status"]
    },
    "Center": {
      "central_widget": "viewport"
    }
  }
}
```

#### 2.2 AIGC Generation 布局示例

```json
{
  "layout_id": "aigc_generation",
  "layout_name": "AIGC Generation",
  "description": "Layout optimized for AI-generated content creation",
  "version": "1.0",
  "views": [
    {
      "plugin_id": "com.robocute.nodeeditor",
      "view_id": "node_editor",
      "dock_area": "Center",
      "visible": true,
      "size": {
        "width": -1,
        "height": -1
      }
    },
    {
      "plugin_id": "com.robocute.result_viewer",
      "view_id": "result_browser",
      "dock_area": "Right",
      "visible": true,
      "size": {
        "width": 400,
        "height": -1
      }
    },
    {
      "plugin_id": "com.robocute.result_viewer",
      "view_id": "image_preview",
      "dock_area": "Right",
      "visible": true,
      "size": {
        "width": 400,
        "height": -1
      },
      "tab_group": "right_panel",
      "tab_index": 1
    },
    {
      "plugin_id": "com.robocute.viewport",
      "view_id": "viewport",
      "dock_area": "Left",
      "visible": false,
      "minimized": true
    },
    {
      "plugin_id": "com.robocute.hierarchy",
      "view_id": "scene_hierarchy",
      "dock_area": "Left",
      "visible": false,
      "minimized": true
    }
  ],
  "dock_arrangements": {
    "Center": {
      "central_widget": "node_editor"
    },
    "Right": {
      "split_orientation": "vertical",
      "split_ratio": [0.4, 0.6],
      "widgets": ["result_browser", "image_preview"]
    }
  }
}
```

### 3. 接口设计

#### 3.1 ILayoutService 接口

```cpp
// services/ILayoutService.h
#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>

namespace rbc {

class ILayoutService : public QObject {
    Q_OBJECT
public:
    virtual ~ILayoutService() = default;
    
    // === 布局查询 ===
    
    /**
     * 获取当前激活的布局 ID
     */
    virtual QString currentLayoutId() const = 0;
    
    /**
     * 获取所有可用的布局 ID 列表
     */
    virtual QStringList availableLayouts() const = 0;
    
    /**
     * 获取布局的元信息（名称、描述等）
     */
    virtual QJsonObject getLayoutMetadata(const QString& layoutId) const = 0;
    
    /**
     * 检查布局是否存在
     */
    virtual bool hasLayout(const QString& layoutId) const = 0;
    
    // === 布局切换 ===
    
    /**
     * 切换到指定布局
     * @param layoutId 布局 ID
     * @param saveCurrentLayout 是否保存当前布局状态
     * @return 是否成功切换
     */
    virtual bool switchToLayout(const QString& layoutId, bool saveCurrentLayout = true) = 0;
    
    /**
     * 保存当前布局状态
     * @param layoutId 保存的布局 ID（为空则保存到当前布局）
     */
    virtual bool saveCurrentLayout(const QString& layoutId = QString()) = 0;
    
    /**
     * 重置布局到默认配置
     */
    virtual void resetLayout(const QString& layoutId) = 0;
    
    // === 布局管理 ===
    
    /**
     * 从 JSON 对象创建新布局
     */
    virtual bool createLayout(const QString& layoutId, const QJsonObject& layoutConfig) = 0;
    
    /**
     * 从文件加载布局配置
     */
    virtual bool loadLayoutFromFile(const QString& filePath) = 0;
    
    /**
     * 保存布局到文件
     */
    virtual bool saveLayoutToFile(const QString& layoutId, const QString& filePath) = 0;
    
    /**
     * 删除布局
     */
    virtual bool deleteLayout(const QString& layoutId) = 0;
    
    /**
     * 复制布局（用于创建自定义布局）
     */
    virtual bool cloneLayout(const QString& sourceId, const QString& newId, const QString& newName) = 0;
    
    // === 视图状态管理 ===
    
    /**
     * 显示/隐藏指定视图
     */
    virtual void setViewVisible(const QString& viewId, bool visible) = 0;
    
    /**
     * 获取视图的可见性
     */
    virtual bool isViewVisible(const QString& viewId) const = 0;
    
    /**
     * 调整视图大小
     */
    virtual void resizeView(const QString& viewId, int width, int height) = 0;
    
    /**
     * 移动视图到指定停靠区域
     */
    virtual void moveViewToDockArea(const QString& viewId, const QString& dockArea) = 0;
    
    // === 初始化 ===
    
    /**
     * 初始化布局服务（与 WindowManager 和 PluginManager 关联）
     * 必须在使用前调用
     */
    virtual void initialize(class WindowManager* windowManager, 
                           class EditorPluginManager* pluginManager) = 0;
    
    /**
     * 应用布局到主窗口
     * 根据布局配置创建和排列所有视图
     */
    virtual void applyLayout(const QString& layoutId) = 0;
    
signals:
    /**
     * 布局切换开始时发出
     */
    void layoutSwitchStarted(const QString& fromLayoutId, const QString& toLayoutId);
    
    /**
     * 布局切换完成时发出
     */
    void layoutSwitched(const QString& layoutId);
    
    /**
     * 布局配置改变时发出
     */
    void layoutChanged(const QString& layoutId);
    
    /**
     * 视图状态改变时发出
     */
    void viewStateChanged(const QString& viewId, bool visible);
    
    /**
     * 布局保存完成
     */
    void layoutSaved(const QString& layoutId);
    
    /**
     * 布局加载失败
     */
    void layoutLoadFailed(const QString& layoutId, const QString& error);
};

} // namespace rbc
```

#### 3.2 LayoutService 实现类

```cpp
// services/LayoutService.h
#pragma once
#include "RBCEditorRuntime/services/ILayoutService.h"
#include <QMap>
#include <QTimer>

namespace rbc {

class WindowManager;
class EditorPluginManager;
class QDockWidget;

/**
 * 视图状态信息
 */
struct ViewState {
    QString pluginId;
    QString viewId;
    QString dockArea;
    bool visible = true;
    bool minimized = false;
    int width = -1;   // -1 表示自动
    int height = -1;
    QString tabGroup;
    int tabIndex = 0;
    QJsonObject properties;
    
    // 运行时状态
    QDockWidget* dockWidget = nullptr;
    QObject* viewModel = nullptr;
};

/**
 * 布局配置信息
 */
struct LayoutConfig {
    QString layoutId;
    QString layoutName;
    QString description;
    QString version;
    QMap<QString, ViewState> views;  // viewId -> ViewState
    QJsonObject dockArrangements;
    bool isBuiltIn = false;
    bool isModified = false;
};

/**
 * 布局服务实现
 */
class RBC_EDITOR_RUNTIME_API LayoutService : public ILayoutService {
    Q_OBJECT
    
public:
    explicit LayoutService(QObject* parent = nullptr);
    ~LayoutService() override;
    
    // === ILayoutService 接口实现 ===
    
    QString currentLayoutId() const override;
    QStringList availableLayouts() const override;
    QJsonObject getLayoutMetadata(const QString& layoutId) const override;
    bool hasLayout(const QString& layoutId) const override;
    
    bool switchToLayout(const QString& layoutId, bool saveCurrentLayout = true) override;
    bool saveCurrentLayout(const QString& layoutId = QString()) override;
    void resetLayout(const QString& layoutId) override;
    
    bool createLayout(const QString& layoutId, const QJsonObject& layoutConfig) override;
    bool loadLayoutFromFile(const QString& filePath) override;
    bool saveLayoutToFile(const QString& layoutId, const QString& filePath) override;
    bool deleteLayout(const QString& layoutId) override;
    bool cloneLayout(const QString& sourceId, const QString& newId, const QString& newName) override;
    
    void setViewVisible(const QString& viewId, bool visible) override;
    bool isViewVisible(const QString& viewId) const override;
    void resizeView(const QString& viewId, int width, int height) override;
    void moveViewToDockArea(const QString& viewId, const QString& dockArea) override;
    
    void initialize(WindowManager* windowManager, EditorPluginManager* pluginManager) override;
    void applyLayout(const QString& layoutId) override;
    
    // === 额外的公共方法 ===
    
    /**
     * 加载内置布局配置（从资源文件）
     */
    void loadBuiltInLayouts();
    
    /**
     * 加载用户自定义布局（从用户配置目录）
     */
    void loadUserLayouts();
    
    /**
     * 获取布局配置存储目录
     */
    QString layoutConfigDirectory() const;
    
private:
    // === 内部辅助方法 ===
    
    /**
     * 从 JSON 解析布局配置
     */
    bool parseLayoutConfig(const QJsonObject& json, LayoutConfig& config);
    
    /**
     * 将布局配置序列化为 JSON
     */
    QJsonObject serializeLayoutConfig(const LayoutConfig& config);
    
    /**
     * 创建或更新视图
     */
    QDockWidget* createOrUpdateView(const ViewState& viewState);
    
    /**
     * 移除视图
     */
    void removeView(const QString& viewId);
    
    /**
     * 应用停靠区域排列
     */
    void applyDockArrangements(const QJsonObject& arrangements);
    
    /**
     * 捕获当前布局状态
     */
    LayoutConfig captureCurrentLayoutState();
    
    /**
     * 清理所有视图
     */
    void clearAllViews();
    
    /**
     * 平滑过渡动画
     */
    void animateLayoutTransition(const QString& fromLayoutId, const QString& toLayoutId);
    
    /**
     * 验证布局配置的有效性
     */
    bool validateLayoutConfig(const LayoutConfig& config, QString& errorMessage);
    
    /**
     * 加载默认布局配置
     */
    void loadDefaultLayouts();
    
private:
    WindowManager* windowManager_ = nullptr;
    EditorPluginManager* pluginManager_ = nullptr;
    
    QString currentLayoutId_;
    QMap<QString, LayoutConfig> layouts_;  // layoutId -> LayoutConfig
    QMap<QString, ViewState> currentViewStates_;  // viewId -> ViewState
    
    bool isTransitioning_ = false;
    QTimer* transitionTimer_ = nullptr;
    
    QString configDirectory_;
};

} // namespace rbc
```

### 4. 实现细节

#### 4.1 布局切换流程

```cpp
bool LayoutService::switchToLayout(const QString& layoutId, bool saveCurrentLayout) {
    if (layoutId == currentLayoutId_) {
        return true;  // 已经是当前布局
    }
    
    if (!layouts_.contains(layoutId)) {
        qWarning() << "LayoutService::switchToLayout: Layout not found:" << layoutId;
        emit layoutLoadFailed(layoutId, "Layout not found");
        return false;
    }
    
    // 1. 发出切换开始信号
    emit layoutSwitchStarted(currentLayoutId_, layoutId);
    isTransitioning_ = true;
    
    // 2. 可选：保存当前布局状态
    if (saveCurrentLayout && !currentLayoutId_.isEmpty()) {
        saveCurrentLayout();
    }
    
    // 3. 清理当前视图（可选：使用淡出动画）
    clearAllViews();
    
    // 4. 应用新布局
    applyLayout(layoutId);
    
    // 5. 更新当前布局 ID
    currentLayoutId_ = layoutId;
    
    // 6. 发出切换完成信号
    isTransitioning_ = false;
    emit layoutSwitched(layoutId);
    
    qDebug() << "LayoutService: Switched to layout:" << layoutId;
    return true;
}
```

#### 4.2 应用布局

```cpp
void LayoutService::applyLayout(const QString& layoutId) {
    if (!layouts_.contains(layoutId)) {
        qWarning() << "LayoutService::applyLayout: Layout not found:" << layoutId;
        return;
    }
    
    const LayoutConfig& config = layouts_[layoutId];
    
    // 1. 遍历所有视图配置
    for (auto it = config.views.begin(); it != config.views.end(); ++it) {
        const QString& viewId = it.key();
        const ViewState& viewState = it.value();
        
        // 2. 创建或更新视图
        if (viewState.visible) {
            QDockWidget* dock = createOrUpdateView(viewState);
            if (dock) {
                // 3. 设置大小
                if (viewState.width > 0 || viewState.height > 0) {
                    QSize size(
                        viewState.width > 0 ? viewState.width : dock->width(),
                        viewState.height > 0 ? viewState.height : dock->height()
                    );
                    dock->resize(size);
                }
                
                // 4. 设置停靠区域
                Qt::DockWidgetArea area = parseDockArea(viewState.dockArea);
                if (area != Qt::NoDockWidgetArea) {
                    windowManager_->main_window()->addDockWidget(area, dock);
                }
                
                // 5. 显示视图
                dock->show();
            }
        } else {
            // 隐藏或移除视图
            if (currentViewStates_.contains(viewId)) {
                if (currentViewStates_[viewId].dockWidget) {
                    currentViewStates_[viewId].dockWidget->hide();
                }
            }
        }
        
        // 6. 更新当前视图状态
        currentViewStates_[viewId] = viewState;
    }
    
    // 7. 应用停靠区域排列（分栏、标签页等）
    applyDockArrangements(config.dockArrangements);
    
    // 8. 设置中央窗口部件
    QString centralViewId = config.dockArrangements["Center"].toObject()["central_widget"].toString();
    if (!centralViewId.isEmpty() && currentViewStates_.contains(centralViewId)) {
        ViewState& centralView = currentViewStates_[centralViewId];
        if (centralView.dockWidget) {
            // 将 DockWidget 转换为中央部件
            QWidget* widget = centralView.dockWidget->widget();
            centralView.dockWidget->setWidget(nullptr);
            windowManager_->main_window()->setCentralWidget(widget);
        }
    }
}
```

#### 4.3 创建或更新视图

```cpp
QDockWidget* LayoutService::createOrUpdateView(const ViewState& viewState) {
    // 1. 检查视图是否已存在
    if (currentViewStates_.contains(viewState.viewId)) {
        ViewState& existing = currentViewStates_[viewState.viewId];
        if (existing.dockWidget) {
            // 视图已存在，更新属性
            existing.visible = viewState.visible;
            existing.dockArea = viewState.dockArea;
            existing.width = viewState.width;
            existing.height = viewState.height;
            return existing.dockWidget;
        }
    }
    
    // 2. 获取 Plugin
    IEditorPlugin* plugin = pluginManager_->getPlugin(viewState.pluginId);
    if (!plugin) {
        qWarning() << "LayoutService::createOrUpdateView: Plugin not found:" << viewState.pluginId;
        return nullptr;
    }
    
    // 3. 查找对应的 ViewContribution
    ViewContribution contribution;
    bool found = false;
    for (const auto& vc : plugin->view_contributions()) {
        if (vc.viewId == viewState.viewId) {
            contribution = vc;
            found = true;
            break;
        }
    }
    
    if (!found) {
        qWarning() << "LayoutService::createOrUpdateView: ViewContribution not found:" 
                   << viewState.viewId;
        return nullptr;
    }
    
    // 4. 获取 ViewModel
    QObject* viewModel = plugin->getViewModel(viewState.viewId);
    if (!viewModel) {
        qWarning() << "LayoutService::createOrUpdateView: ViewModel not found for" 
                   << viewState.viewId;
        return nullptr;
    }
    
    // 5. 使用 WindowManager 创建 DockWidget
    contribution.dockArea = viewState.dockArea;
    contribution.preferredSize = QString("%1,%2").arg(viewState.width).arg(viewState.height);
    
    QDockWidget* dock = windowManager_->createDockableView(contribution, viewModel);
    if (dock) {
        // 6. 保存到当前视图状态
        ViewState newState = viewState;
        newState.dockWidget = dock;
        newState.viewModel = viewModel;
        currentViewStates_[viewState.viewId] = newState;
    }
    
    return dock;
}
```

#### 4.4 停靠区域排列

```cpp
void LayoutService::applyDockArrangements(const QJsonObject& arrangements) {
    QMainWindow* mainWindow = windowManager_->main_window();
    if (!mainWindow) return;
    
    // 1. 处理左侧区域
    if (arrangements.contains("Left")) {
        QJsonObject leftConfig = arrangements["Left"].toObject();
        QString orientation = leftConfig["split_orientation"].toString();
        QJsonArray widgets = leftConfig["widgets"].toArray();
        
        if (widgets.size() > 1) {
            // 创建分栏或标签页
            Qt::Orientation splitOrientation = (orientation == "horizontal") 
                ? Qt::Horizontal : Qt::Vertical;
            
            QList<QDockWidget*> dockWidgets;
            for (const QJsonValue& val : widgets) {
                QString viewId = val.toString();
                if (currentViewStates_.contains(viewId)) {
                    QDockWidget* dock = currentViewStates_[viewId].dockWidget;
                    if (dock) {
                        dockWidgets.append(dock);
                    }
                }
            }
            
            // 使用 Qt 的 splitDockWidget 或 tabifyDockWidget
            if (dockWidgets.size() > 1) {
                if (orientation == "tabbed") {
                    // 创建标签页
                    for (int i = 1; i < dockWidgets.size(); ++i) {
                        mainWindow->tabifyDockWidget(dockWidgets[0], dockWidgets[i]);
                    }
                } else {
                    // 创建分栏
                    for (int i = 1; i < dockWidgets.size(); ++i) {
                        mainWindow->splitDockWidget(dockWidgets[i-1], dockWidgets[i], splitOrientation);
                    }
                }
            }
        }
    }
    
    // 2. 类似地处理 Right、Top、Bottom 区域
    // ... 省略类似代码
}
```

#### 4.5 保存布局

```cpp
bool LayoutService::saveCurrentLayout(const QString& layoutId) {
    QString targetLayoutId = layoutId.isEmpty() ? currentLayoutId_ : layoutId;
    
    if (targetLayoutId.isEmpty()) {
        qWarning() << "LayoutService::saveCurrentLayout: No layout ID specified";
        return false;
    }
    
    // 1. 捕获当前布局状态
    LayoutConfig config = captureCurrentLayoutState();
    config.layoutId = targetLayoutId;
    
    // 2. 如果是内置布局，另存为用户自定义布局
    if (layouts_.contains(targetLayoutId) && layouts_[targetLayoutId].isBuiltIn) {
        QString customId = targetLayoutId + "_custom";
        config.layoutId = customId;
        config.layoutName = layouts_[targetLayoutId].layoutName + " (Custom)";
        targetLayoutId = customId;
    }
    
    // 3. 保存到内存
    layouts_[targetLayoutId] = config;
    
    // 4. 持久化到文件
    QString filePath = layoutConfigDirectory() + "/" + targetLayoutId + ".json";
    bool success = saveLayoutToFile(targetLayoutId, filePath);
    
    if (success) {
        emit layoutSaved(targetLayoutId);
        qDebug() << "LayoutService: Layout saved:" << targetLayoutId;
    }
    
    return success;
}

LayoutConfig LayoutService::captureCurrentLayoutState() {
    LayoutConfig config;
    config.version = "1.0";
    
    // 遍历所有当前显示的视图
    QMainWindow* mainWindow = windowManager_->main_window();
    if (!mainWindow) return config;
    
    // 捕获所有 DockWidget 的状态
    QList<QDockWidget*> dockWidgets = mainWindow->findChildren<QDockWidget*>();
    for (QDockWidget* dock : dockWidgets) {
        QString viewId = dock->objectName();
        if (currentViewStates_.contains(viewId)) {
            ViewState state = currentViewStates_[viewId];
            
            // 更新状态
            state.visible = dock->isVisible();
            state.width = dock->width();
            state.height = dock->height();
            
            // 确定停靠区域
            Qt::DockWidgetArea area = mainWindow->dockWidgetArea(dock);
            state.dockArea = dockAreaToString(area);
            
            config.views[viewId] = state;
        }
    }
    
    return config;
}
```

### 5. 预设布局配置

#### 5.1 内置布局配置文件

布局配置文件存储在资源文件中：

```
rbc/editor/runtimex/
├── layouts/                      # 布局配置目录
│   ├── scene_editing.json       # 3D 场景编辑布局
│   ├── node_editing.json        # 节点编辑布局
│   ├── aigc_generation.json     # AIGC 生成布局
│   ├── animation_playback.json  # 动画播放布局
│   └── minimal.json             # 最小化布局
```

添加到 `rbc_editor.qrc`：

```xml
<RCC>
    <qresource prefix="/layouts">
        <file>layouts/scene_editing.json</file>
        <file>layouts/node_editing.json</file>
        <file>layouts/aigc_generation.json</file>
        <file>layouts/animation_playback.json</file>
        <file>layouts/minimal.json</file>
    </qresource>
</RCC>
```

#### 5.2 场景编辑布局（scene_editing.json）

```json
{
  "layout_id": "scene_editing",
  "layout_name": "3D Scene Editing",
  "description": "Default layout for 3D scene editing",
  "version": "1.0",
  "views": [
    {
      "plugin_id": "com.robocute.viewport",
      "view_id": "viewport",
      "dock_area": "Center",
      "visible": true,
      "size": { "width": -1, "height": -1 }
    },
    {
      "plugin_id": "com.robocute.hierarchy",
      "view_id": "scene_hierarchy",
      "dock_area": "Left",
      "visible": true,
      "size": { "width": 300, "height": -1 }
    },
    {
      "plugin_id": "com.robocute.detail",
      "view_id": "detail_panel",
      "dock_area": "Right",
      "visible": true,
      "size": { "width": 350, "height": -1 }
    },
    {
      "plugin_id": "com.robocute.nodeeditor",
      "view_id": "node_editor",
      "dock_area": "Bottom",
      "visible": true,
      "size": { "width": -1, "height": 300 }
    },
    {
      "plugin_id": "com.robocute.connection",
      "view_id": "connection_status",
      "dock_area": "Left",
      "visible": true,
      "size": { "width": 300, "height": 200 },
      "tab_group": "left_tools",
      "tab_index": 1
    }
  ],
  "dock_arrangements": {
    "Left": {
      "type": "tabbed",
      "widgets": ["scene_hierarchy", "connection_status"]
    },
    "Center": {
      "central_widget": "viewport"
    }
  }
}
```

#### 5.3 AIGC 生成布局（aigc_generation.json）

```json
{
  "layout_id": "aigc_generation",
  "layout_name": "AIGC Generation",
  "description": "Layout optimized for AI content generation",
  "version": "1.0",
  "views": [
    {
      "plugin_id": "com.robocute.nodeeditor",
      "view_id": "node_editor",
      "dock_area": "Center",
      "visible": true,
      "size": { "width": -1, "height": -1 }
    },
    {
      "plugin_id": "com.robocute.result_viewer",
      "view_id": "result_browser",
      "dock_area": "Right",
      "visible": true,
      "size": { "width": 400, "height": -1 }
    },
    {
      "plugin_id": "com.robocute.viewport",
      "view_id": "viewport",
      "dock_area": "Left",
      "visible": false,
      "minimized": true,
      "size": { "width": 200, "height": 200 }
    },
    {
      "plugin_id": "com.robocute.hierarchy",
      "view_id": "scene_hierarchy",
      "dock_area": "Left",
      "visible": false,
      "minimized": true
    }
  ],
  "dock_arrangements": {
    "Center": {
      "central_widget": "node_editor"
    }
  }
}
```

### 6. 使用示例

#### 6.1 主程序集成

```cpp
// main.cpp
int main(int argc, char* argv[]) {
    // ... 初始化 Qt 应用和 EditorEngine
    
    // 1. 创建核心服务
    auto* layoutService = new LayoutService(&app);
    pluginManager.registerService(layoutService);
    
    // 2. 创建 WindowManager
    WindowManager windowManager(&pluginManager, &app);
    windowManager.setup_main_window();
    
    // 3. 初始化 LayoutService
    layoutService->initialize(&windowManager, &pluginManager);
    
    // 4. 加载布局配置
    layoutService->loadBuiltInLayouts();
    layoutService->loadUserLayouts();
    
    // 5. 加载插件
    pluginManager.loadPlugin("RBCE_ConnectionPlugin");
    pluginManager.loadPlugin("RBCE_ViewportPlugin");
    pluginManager.loadPlugin("RBCE_HierarchyPlugin");
    pluginManager.loadPlugin("RBCE_DetailPlugin");
    pluginManager.loadPlugin("RBCE_NodeEditorPlugin");
    pluginManager.loadPlugin("RBCE_ResultViewerPlugin");
    
    // 6. 应用默认布局
    layoutService->switchToLayout("scene_editing");
    
    // 7. 显示主窗口
    windowManager.main_window()->show();
    
    return app.exec();
}
```

#### 6.2 运行时切换布局

```cpp
// 通过菜单或快捷键切换布局
void MainWindow::onSwitchToAIGCLayout() {
    auto* layoutService = pluginManager.getService<LayoutService>();
    if (layoutService) {
        layoutService->switchToLayout("aigc_generation", true);
    }
}

// 恢复到场景编辑布局
void MainWindow::onSwitchToSceneEditingLayout() {
    auto* layoutService = pluginManager.getService<LayoutService>();
    if (layoutService) {
        layoutService->switchToLayout("scene_editing", true);
    }
}
```

#### 6.3 QML 中使用

```qml
// LayoutSwitcher.qml - 布局切换器 UI
import QtQuick
import QtQuick.Controls

ComboBox {
    id: layoutSelector
    
    property var layoutService: null
    
    model: layoutService ? layoutService.availableLayouts() : []
    currentIndex: {
        if (layoutService) {
            return model.indexOf(layoutService.currentLayoutId())
        }
        return 0
    }
    
    onActivated: (index) => {
        if (layoutService && index >= 0) {
            layoutService.switchToLayout(model[index], true)
        }
    }
    
    delegate: ItemDelegate {
        width: layoutSelector.width
        text: {
            if (layoutService) {
                var metadata = layoutService.getLayoutMetadata(modelData)
                return metadata.layout_name || modelData
            }
            return modelData
        }
        
        ToolTip.visible: hovered
        ToolTip.text: {
            if (layoutService) {
                var metadata = layoutService.getLayoutMetadata(modelData)
                return metadata.description || ""
            }
            return ""
        }
    }
}
```

#### 6.4 在 Plugin 中使用 LayoutService

```cpp
// ViewportPlugin.cpp
bool ViewportPlugin::load(PluginContext* context) {
    // 获取 LayoutService
    auto* layoutService = context->getService<LayoutService>();
    
    // 监听布局切换事件
    connect(layoutService, &LayoutService::layoutSwitched,
            this, &ViewportPlugin::onLayoutSwitched);
    
    return true;
}

void ViewportPlugin::onLayoutSwitched(const QString& layoutId) {
    // 根据布局调整视口行为
    if (layoutId == "aigc_generation") {
        // AIGC 模式：禁用一些 3D 交互功能
        viewportViewModel_->setInteractionMode("minimal");
    } else if (layoutId == "scene_editing") {
        // 场景编辑模式：启用完整的 3D 交互
        viewportViewModel_->setInteractionMode("full");
    }
}
```

### 7. 用户配置持久化

#### 7.1 配置目录结构

```
用户配置目录 (QStandardPaths::AppDataLocation):
<AppData>/RoboCute/Editor/
├── layouts/                     # 用户自定义布局
│   ├── my_layout_1.json
│   ├── my_layout_2.json
│   └── scene_editing_custom.json  # 用户修改的内置布局
├── layout_state.json            # 上次退出时的布局状态
└── preferences.json             # 其他偏好设置
```

#### 7.2 启动时恢复布局

```cpp
void LayoutService::loadUserLayouts() {
    QString layoutDir = layoutConfigDirectory();
    QDir dir(layoutDir);
    
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // 1. 加载所有用户布局
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo& fileInfo : files) {
        if (fileInfo.fileName() == "layout_state.json") {
            continue;  // 跳过状态文件
        }
        
        loadLayoutFromFile(fileInfo.absoluteFilePath());
    }
    
    // 2. 恢复上次的布局
    QString stateFile = layoutDir + "/layout_state.json";
    QFile file(stateFile);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject state = doc.object();
        
        QString lastLayout = state["last_layout"].toString();
        if (!lastLayout.isEmpty() && layouts_.contains(lastLayout)) {
            currentLayoutId_ = lastLayout;
            qDebug() << "LayoutService: Restored last layout:" << lastLayout;
        }
    }
}
```

#### 7.3 退出时保存状态

```cpp
void LayoutService::saveLayoutState() {
    // 保存当前布局状态
    saveCurrentLayout();
    
    // 保存布局切换历史
    QJsonObject state;
    state["last_layout"] = currentLayoutId_;
    state["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QString stateFile = layoutConfigDirectory() + "/layout_state.json";
    QFile file(stateFile);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(state);
        file.write(doc.toJson(QJsonDocument::Indented));
    }
}
```

### 8. 进阶功能

#### 8.1 布局动画过渡

```cpp
void LayoutService::animateLayoutTransition(const QString& fromLayoutId, 
                                           const QString& toLayoutId) {
    // 1. 淡出当前视图
    for (auto& viewState : currentViewStates_) {
        if (viewState.dockWidget && viewState.visible) {
            QPropertyAnimation* fadeOut = new QPropertyAnimation(viewState.dockWidget, "windowOpacity");
            fadeOut->setDuration(200);
            fadeOut->setStartValue(1.0);
            fadeOut->setEndValue(0.0);
            fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
    
    // 2. 延迟应用新布局
    QTimer::singleShot(200, this, [this, toLayoutId]() {
        applyLayout(toLayoutId);
        
        // 3. 淡入新视图
        for (auto& viewState : currentViewStates_) {
            if (viewState.dockWidget && viewState.visible) {
                viewState.dockWidget->setWindowOpacity(0.0);
                QPropertyAnimation* fadeIn = new QPropertyAnimation(viewState.dockWidget, "windowOpacity");
                fadeIn->setDuration(200);
                fadeIn->setStartValue(0.0);
                fadeIn->setEndValue(1.0);
                fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
            }
        }
    });
}
```

#### 8.2 布局验证

```cpp
bool LayoutService::validateLayoutConfig(const LayoutConfig& config, QString& errorMessage) {
    // 1. 验证必填字段
    if (config.layoutId.isEmpty()) {
        errorMessage = "Layout ID is required";
        return false;
    }
    
    if (config.layoutName.isEmpty()) {
        errorMessage = "Layout name is required";
        return false;
    }
    
    // 2. 验证视图配置
    for (auto it = config.views.begin(); it != config.views.end(); ++it) {
        const ViewState& view = it.value();
        
        // 验证 Plugin 是否存在
        if (!pluginManager_->getPlugin(view.pluginId)) {
            errorMessage = QString("Plugin not found: %1").arg(view.pluginId);
            return false;
        }
        
        // 验证停靠区域
        QStringList validAreas = {"Left", "Right", "Top", "Bottom", "Center", "Floating"};
        if (!validAreas.contains(view.dockArea)) {
            errorMessage = QString("Invalid dock area: %1").arg(view.dockArea);
            return false;
        }
    }
    
    // 3. 验证中央窗口部件
    QString centralWidget = config.dockArrangements["Center"].toObject()["central_widget"].toString();
    if (!centralWidget.isEmpty() && !config.views.contains(centralWidget)) {
        errorMessage = QString("Central widget not found in views: %1").arg(centralWidget);
        return false;
    }
    
    return true;
}
```

#### 8.3 布局导入导出

```cpp
// 导出布局为可分享的文件
bool LayoutService::exportLayoutForSharing(const QString& layoutId, const QString& filePath) {
    if (!layouts_.contains(layoutId)) {
        return false;
    }
    
    LayoutConfig config = layouts_[layoutId];
    
    // 移除运行时状态
    for (auto& view : config.views) {
        view.dockWidget = nullptr;
        view.viewModel = nullptr;
    }
    
    // 添加导出元数据
    QJsonObject exportData = serializeLayoutConfig(config);
    exportData["export_version"] = "1.0";
    exportData["exported_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    exportData["exported_by"] = "RoboCute Editor";
    
    // 保存到文件
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(exportData);
        file.write(doc.toJson(QJsonDocument::Indented));
        return true;
    }
    
    return false;
}
```

## 9. 测试策略

### 9.1 单元测试

```cpp
// tests/services/test_layout_service.cpp
class LayoutServiceTest : public QObject {
    Q_OBJECT
    
private slots:
    void test_loadBuiltInLayouts() {
        LayoutService service;
        service.loadBuiltInLayouts();
        
        QVERIFY(service.hasLayout("scene_editing"));
        QVERIFY(service.hasLayout("aigc_generation"));
        QVERIFY(service.availableLayouts().size() >= 2);
    }
    
    void test_switchLayout() {
        MockWindowManager windowManager;
        MockPluginManager pluginManager;
        
        LayoutService service;
        service.initialize(&windowManager, &pluginManager);
        service.loadBuiltInLayouts();
        
        QVERIFY(service.switchToLayout("scene_editing"));
        QCOMPARE(service.currentLayoutId(), QString("scene_editing"));
        
        QVERIFY(service.switchToLayout("aigc_generation"));
        QCOMPARE(service.currentLayoutId(), QString("aigc_generation"));
    }
    
    void test_saveAndLoadLayout() {
        LayoutService service;
        
        // Create a test layout
        QJsonObject layoutJson = createTestLayoutJson();
        QVERIFY(service.createLayout("test_layout", layoutJson));
        
        // Save to file
        QString tempFile = QDir::temp().filePath("test_layout.json");
        QVERIFY(service.saveLayoutToFile("test_layout", tempFile));
        
        // Load from file
        service.deleteLayout("test_layout");
        QVERIFY(!service.hasLayout("test_layout"));
        
        QVERIFY(service.loadLayoutFromFile(tempFile));
        QVERIFY(service.hasLayout("test_layout"));
        
        // Cleanup
        QFile::remove(tempFile);
    }
};
```

### 9.2 集成测试

```cpp
// tests/integration/test_layout_integration.cpp
void testLayoutPluginIntegration() {
    // 1. 初始化环境
    QApplication app(argc, argv);
    EditorEngine::instance().init(argc, argv);
    
    EditorPluginManager pluginManager;
    WindowManager windowManager(&pluginManager);
    LayoutService layoutService;
    
    pluginManager.registerService(&layoutService);
    windowManager.setup_main_window();
    layoutService.initialize(&windowManager, &pluginManager);
    
    // 2. 加载插件
    pluginManager.loadPlugin("RBCE_ViewportPlugin");
    pluginManager.loadPlugin("RBCE_NodeEditorPlugin");
    
    // 3. 加载并应用布局
    layoutService.loadBuiltInLayouts();
    layoutService.switchToLayout("scene_editing");
    
    // 4. 验证视图已创建
    QVERIFY(layoutService.isViewVisible("viewport"));
    QVERIFY(layoutService.isViewVisible("node_editor"));
    
    // 5. 切换布局
    layoutService.switchToLayout("aigc_generation");
    
    // 6. 验证视图状态变化
    QVERIFY(!layoutService.isViewVisible("viewport"));
    QVERIFY(layoutService.isViewVisible("node_editor"));
}
```

## 10. 性能优化

### 10.1 延迟视图创建

```cpp
// 只有在视图首次可见时才创建
QDockWidget* LayoutService::createOrUpdateView(const ViewState& viewState) {
    // 如果视图不可见且从未创建过，延迟创建
    if (!viewState.visible && !currentViewStates_.contains(viewState.viewId)) {
        // 保存视图状态，但不创建实际的 Widget
        currentViewStates_[viewState.viewId] = viewState;
        return nullptr;
    }
    
    // ... 创建视图的逻辑
}
```

### 10.2 视图缓存

```cpp
// 缓存隐藏的视图，而不是销毁
void LayoutService::setViewVisible(const QString& viewId, bool visible) {
    if (!currentViewStates_.contains(viewId)) {
        return;
    }
    
    ViewState& viewState = currentViewStates_[viewId];
    
    if (viewState.dockWidget) {
        if (visible) {
            viewState.dockWidget->show();
        } else {
            // 隐藏而不是销毁
            viewState.dockWidget->hide();
        }
        viewState.visible = visible;
        emit viewStateChanged(viewId, visible);
    }
}
```

### 10.3 批量布局更新

```cpp
// 在切换布局时批量更新，避免多次重绘
void LayoutService::applyLayout(const QString& layoutId) {
    QMainWindow* mainWindow = windowManager_->main_window();
    if (!mainWindow) return;
    
    // 1. 暂停布局更新
    mainWindow->setUpdatesEnabled(false);
    
    // 2. 应用所有布局变更
    // ... 创建和排列视图的逻辑
    
    // 3. 恢复布局更新并触发重绘
    mainWindow->setUpdatesEnabled(true);
    mainWindow->update();
}
```
