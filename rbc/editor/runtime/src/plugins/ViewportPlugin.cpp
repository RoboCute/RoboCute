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

    _context = context;
    _scene_service = context->getService<ISceneService>();

    if (!_scene_service) {
        qWarning() << "ViewportPlugin::load: SceneService not available";
    }

    // Do not auto-create default viewports on load
    // Caller needs to set rendererFactory first, then manually call createDefaultViewports or createViewport
    qDebug() << "ViewportPlugin loaded successfully";
    return true;
}

bool ViewportPlugin::unload() {
    qDebug() << "ViewportPlugin::unload";

    destroyAllViewports();
    _registered_contributions.clear();

    _scene_service = nullptr;
    _context = nullptr;

    return true;
}

bool ViewportPlugin::reload() {
    qDebug() << "ViewportPlugin::reload";
    return true;
}

void ViewportPlugin::createDefaultViewports() {
    if (!_renderer_factory) {
        qWarning() << "ViewportPlugin::createDefaultViewports: rendererFactory not set";
        return;
    }

    // Create main viewport
    ViewportConfig mainConfig;
    mainConfig.viewportId = "viewport.main";
    mainConfig.type = ViewportType::Main;
    mainConfig.rendererType = "scene";
    mainConfig.graphicsApi = _default_graphics_api;
    _main_viewport_id = createViewport(mainConfig);

    if (!_main_viewport_id.isEmpty()) {
        // Register to contributions
        NativeViewContribution mainContrib;
        mainContrib.viewId = mainConfig.viewportId;
        mainContrib.title = "Scene";
        mainContrib.dockArea = "Center";
        mainContrib.isExternalManaged = true;// Lifecycle managed by Plugin

        _registered_contributions.append(mainContrib);

        qDebug() << "ViewportPlugin: Created main viewport:" << _main_viewport_id;
    }
}

QString ViewportPlugin::createViewport(const ViewportConfig &config) {
    if (_viewports.contains(config.viewportId)) {
        qWarning() << "ViewportPlugin::createViewport: Viewport already exists:" << config.viewportId;
        return QString();
    }

    // Create renderer
    IRenderer *renderer = createRenderer(config);
    if (!renderer) {
        qWarning() << "ViewportPlugin::createViewport: Failed to create renderer for:" << config.viewportId;
        return QString();
    }

    return createViewportWithRenderer(config, renderer);
}

QString ViewportPlugin::createViewportWithRenderer(const ViewportConfig &config, IRenderer *renderer) {
    if (_viewports.contains(config.viewportId)) {
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

    // Create widget
    instance->widget = new ViewportWidget(renderer, config.graphicsApi, nullptr);

    // Create viewModel
    instance->viewModel = new ViewportViewModel(config, _scene_service, nullptr);

    // Connect widget drag signals (entity drag-and-drop can be handled here)
    connect(instance->widget.data(), &ViewportWidget::entityDragRequested, this, [viewportId = config.viewportId]() {
        qDebug() << "ViewportPlugin: Entity drag requested from viewport:" << viewportId;
        // TODO: Implement entity drag-and-drop logic
    });

    _viewports.insert(config.viewportId, instance);

    emit viewportCreated(config.viewportId);
    qDebug() << "ViewportPlugin: Created viewport:" << config.viewportId;

    return config.viewportId;
}

bool ViewportPlugin::destroyViewport(const QString &viewportId) {
    auto it = _viewports.find(viewportId);
    if (it == _viewports.end()) {
        qWarning() << "ViewportPlugin::destroyViewport: Viewport not found:" << viewportId;
        return false;
    }

    ViewportInstance *instance = it.value();

    // Delete widget (triggers RhiWindow cleanup)
    // Use QPointer to check if widget still exists
    // If Qt already deleted widget (e.g., via parent-child mechanism), QPointer becomes nullptr
    if (instance->widget) {
        qDebug() << "ViewportPlugin::destroyViewport: Deleting widget for:" << viewportId;
        delete instance->widget.data();
        // QPointer auto-becomes nullptr, no manual setting needed
    } else {
        qDebug() << "ViewportPlugin::destroyViewport: Widget already deleted for:" << viewportId;
    }

    // Delete instance (destructor cleans up viewModel)
    delete instance;

    _viewports.erase(it);

    // If main viewport, clear reference
    if (viewportId == _main_viewport_id) {
        _main_viewport_id.clear();
    }

    // Remove from contributions
    _registered_contributions.erase(
        std::remove_if(_registered_contributions.begin(), _registered_contributions.end(),
                       [&viewportId](const NativeViewContribution &c) {
                           return c.viewId == viewportId;
                       }),
        _registered_contributions.end());

    emit viewportDestroyed(viewportId);
    qDebug() << "ViewportPlugin: Destroyed viewport:" << viewportId;

    return true;
}

void ViewportPlugin::destroyAllViewports() {
    QStringList ids = _viewports.keys();
    for (const QString &id : ids) {
        destroyViewport(id);
    }
}

IRenderer *ViewportPlugin::createRenderer(const ViewportConfig &config) {
    if (_renderer_factory) {
        return _renderer_factory(config);
    }

    qWarning() << "ViewportPlugin::createRenderer: No renderer factory set";
    return nullptr;
}

ViewportInstance *ViewportPlugin::getViewport(const QString &viewportId) {
    return _viewports.value(viewportId, nullptr);
}

QStringList ViewportPlugin::allViewportIds() const {
    return _viewports.keys();
}

ViewportInstance *ViewportPlugin::mainViewport() const {
    if (_main_viewport_id.isEmpty()) {
        return nullptr;
    }
    return _viewports.value(_main_viewport_id, nullptr);
}

// ============================================================================
// UI Contributions
// ============================================================================

QList<NativeViewContribution> ViewportPlugin::native_view_contributions() const {
    return _registered_contributions;
}

QWidget *ViewportPlugin::getNativeWidget(const QString &viewId) {
    if (auto *instance = _viewports.value(viewId, nullptr)) {
        return instance->widget.data();  // QPointer::data() returns raw pointer
    }
    return nullptr;
}

QObject *ViewportPlugin::getViewModel(const QString &viewId) {
    if (auto *instance = _viewports.value(viewId, nullptr)) {
        return instance->viewModel;
    }
    return nullptr;
}

QList<MenuContribution> ViewportPlugin::menu_contributions() const {
    // TODO: Add viewport-related menu items
    return {};
}

QList<ToolbarContribution> ViewportPlugin::toolbar_contributions() const {
    // TODO: Add viewport-related toolbar items
    return {};
}

}// namespace rbc
