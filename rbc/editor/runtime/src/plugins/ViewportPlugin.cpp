#include "RBCEditorRuntime/plugins/ViewportPlugin.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"
#include "RBCEditorRuntime/ui/ViewportWidget.h"
#include <QDebug>
#include <luisa/runtime/context.h>

namespace rbc {

// ============================================================================
// ViewportPlugin Implementation
// ============================================================================

ViewportPlugin::ViewportPlugin(QObject *parent)
    : IEditorPlugin(parent) {
}

ViewportPlugin::~ViewportPlugin() {
    destroyAllViewports();
}

bool ViewportPlugin::load(PluginContext *context) {
    if (!context) {
        qWarning() << "ViewportPlugin::load: context is null";
        return false;
    }

    context_ = context;
    sceneService_ = context->getService<ISceneService>();

    if (!sceneService_) {
        qWarning() << "ViewportPlugin::load: SceneService not available";
    }

    // 不在 load 时自动创建默认视口
    // 调用者需要先设置 rendererFactory，然后手动调用 createDefaultViewports 或 createViewport
    qDebug() << "ViewportPlugin loaded successfully";
    return true;
}

bool ViewportPlugin::unload() {
    qDebug() << "ViewportPlugin::unload";

    destroyAllViewports();
    registeredContributions_.clear();

    sceneService_ = nullptr;
    context_ = nullptr;

    return true;
}

bool ViewportPlugin::reload() {
    qDebug() << "ViewportPlugin::reload";
    return true;
}

void ViewportPlugin::createDefaultViewports() {
    if (!rendererFactory_) {
        qWarning() << "ViewportPlugin::createDefaultViewports: rendererFactory not set";
        return;
    }

    // 创建主视口
    ViewportConfig mainConfig;
    mainConfig.viewportId = "viewport.main";
    mainConfig.type = ViewportType::Main;
    mainConfig.rendererType = "scene";
    mainConfig.graphicsApi = defaultGraphicsApi_;
    mainViewportId_ = createViewport(mainConfig);

    if (!mainViewportId_.isEmpty()) {
        // 注册到 contributions
        NativeViewContribution mainContrib;
        mainContrib.viewId = mainConfig.viewportId;
        mainContrib.title = "Scene";
        mainContrib.dockArea = "Center";
        mainContrib.isExternalManaged = true;// 由 Plugin 管理生命周期

        registeredContributions_.append(mainContrib);

        qDebug() << "ViewportPlugin: Created main viewport:" << mainViewportId_;
    }
}

QString ViewportPlugin::createViewport(const ViewportConfig &config) {
    if (viewports_.contains(config.viewportId)) {
        qWarning() << "ViewportPlugin::createViewport: Viewport already exists:" << config.viewportId;
        return QString();
    }

    // 创建渲染器
    IRenderer *renderer = createRenderer(config);
    if (!renderer) {
        qWarning() << "ViewportPlugin::createViewport: Failed to create renderer for:" << config.viewportId;
        return QString();
    }

    return createViewportWithRenderer(config, renderer);
}

QString ViewportPlugin::createViewportWithRenderer(const ViewportConfig &config, IRenderer *renderer) {
    if (viewports_.contains(config.viewportId)) {
        qWarning() << "ViewportPlugin::createViewportWithRenderer: Viewport already exists:" << config.viewportId;
        return QString();
    }

    if (!renderer) {
        qWarning() << "ViewportPlugin::createViewportWithRenderer: renderer is null";
        return QString();
    }

    auto *instance = new ViewportInstance();
    instance->config = config;
    instance->renderer = renderer;

    // 创建 Widget
    instance->widget = new ViewportWidget(renderer, config.graphicsApi, nullptr);

    // 创建 ViewModel
    instance->viewModel = new ViewportViewModel(config, sceneService_, nullptr);

    // 连接 Widget 的拖动信号（可以在这里处理实体拖放）
    connect(instance->widget.data(), &ViewportWidget::entityDragRequested, this, [this, viewportId = config.viewportId]() {
        qDebug() << "ViewportPlugin: Entity drag requested from viewport:" << viewportId;
        // TODO: 实现实体拖放逻辑
    });

    viewports_.insert(config.viewportId, instance);

    emit viewportCreated(config.viewportId);
    qDebug() << "ViewportPlugin: Created viewport:" << config.viewportId;

    return config.viewportId;
}

bool ViewportPlugin::destroyViewport(const QString &viewportId) {
    auto it = viewports_.find(viewportId);
    if (it == viewports_.end()) {
        qWarning() << "ViewportPlugin::destroyViewport: Viewport not found:" << viewportId;
        return false;
    }

    ViewportInstance *instance = it.value();

    // 删除 Widget（会触发 RhiWindow 的清理）
    // 使用 QPointer 检查 widget 是否仍然存在
    // 如果 Qt 已经删除了 widget（例如通过 parent-child 机制），QPointer 会变成 nullptr
    if (instance->widget) {
        qDebug() << "ViewportPlugin::destroyViewport: Deleting widget for:" << viewportId;
        delete instance->widget.data();
        // QPointer 会自动变成 nullptr，无需手动设置
    } else {
        qDebug() << "ViewportPlugin::destroyViewport: Widget already deleted for:" << viewportId;
    }

    // 删除实例（析构函数会清理 viewModel）
    delete instance;

    viewports_.erase(it);

    // 如果是主视口，清除引用
    if (viewportId == mainViewportId_) {
        mainViewportId_.clear();
    }

    // 从 contributions 中移除
    registeredContributions_.erase(
        std::remove_if(registeredContributions_.begin(), registeredContributions_.end(),
                       [&viewportId](const NativeViewContribution &c) {
                           return c.viewId == viewportId;
                       }),
        registeredContributions_.end());

    emit viewportDestroyed(viewportId);
    qDebug() << "ViewportPlugin: Destroyed viewport:" << viewportId;

    return true;
}

void ViewportPlugin::destroyAllViewports() {
    QStringList ids = viewports_.keys();
    for (const QString &id : ids) {
        destroyViewport(id);
    }
}

IRenderer *ViewportPlugin::createRenderer(const ViewportConfig &config) {
    if (rendererFactory_) {
        return rendererFactory_(config);
    }

    qWarning() << "ViewportPlugin::createRenderer: No renderer factory set";
    return nullptr;
}

ViewportInstance *ViewportPlugin::getViewport(const QString &viewportId) {
    return viewports_.value(viewportId, nullptr);
}

QStringList ViewportPlugin::allViewportIds() const {
    return viewports_.keys();
}

ViewportInstance *ViewportPlugin::mainViewport() const {
    if (mainViewportId_.isEmpty()) {
        return nullptr;
    }
    return viewports_.value(mainViewportId_, nullptr);
}

// ============================================================================
// UI Contributions
// ============================================================================

QList<NativeViewContribution> ViewportPlugin::native_view_contributions() const {
    return registeredContributions_;
}

QWidget *ViewportPlugin::getNativeWidget(const QString &viewId) {
    if (auto *instance = viewports_.value(viewId, nullptr)) {
        return instance->widget.data();  // QPointer::data() 返回原始指针
    }
    return nullptr;
}

QObject *ViewportPlugin::getViewModel(const QString &viewId) {
    if (auto *instance = viewports_.value(viewId, nullptr)) {
        return instance->viewModel;
    }
    return nullptr;
}

QList<MenuContribution> ViewportPlugin::menu_contributions() const {
    // TODO: 添加视口相关菜单项
    return {};
}

QList<ToolbarContribution> ViewportPlugin::toolbar_contributions() const {
    // TODO: 添加视口相关工具栏项
    return {};
}

}// namespace rbc
