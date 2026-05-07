#include "LayoutPlugin.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"
#include "RBCEditorRuntime/plugins/IPluginFactory.h"

#include <QQmlEngine>
#include <QDebug>

namespace rbc {

LayoutPlugin::LayoutPlugin(QObject *parent)
    : IEditorPlugin(parent) {
    qDebug() << "LayoutPlugin created";
}

LayoutPlugin::~LayoutPlugin() {
    qDebug() << "LayoutPlugin destroyed";
}

bool LayoutPlugin::load(PluginContext *context) {
    if (!context) {
        qWarning() << "LayoutPlugin::load: context is null";
        return false;
    }

    _context = context;

    // Get LayoutService from PluginManager
    _layout_service = context->getService<LayoutService>();

    if (!_layout_service) {
        qWarning() << "LayoutPlugin::load: LayoutService should be registered before LayoutPlugin load";
        return false;
    }

    // Create ViewModel
    _view_model = new LayoutViewModel(_layout_service, this);

    // Build menu contributions
    buildMenuContributions();

    qDebug() << "LayoutPlugin loaded successfully";

    return true;
}

bool LayoutPlugin::unload() {
    if (_view_model) {
        _view_model->deleteLater();
        _view_model = nullptr;
    }

    _menu_contributions.clear();
    _layout_service = nullptr;
    _context = nullptr;

    qDebug() << "LayoutPlugin unloaded";
    return true;
}

bool LayoutPlugin::reload() {
    qDebug() << "LayoutPlugin reloading...";

    // Save current layout
    QString savedLayoutId = _layout_service ? _layout_service->currentLayoutId() : QString();

    // Unload and reload
    if (!unload()) {
        return false;
    }

    if (!load(_context)) {
        return false;
    }

    // Restore layout
    if (_layout_service && !savedLayoutId.isEmpty()) {
        _layout_service->switchToLayout(savedLayoutId, false);
    }

    qDebug() << "LayoutPlugin reloaded";
    return true;
}

QList<ViewContribution> LayoutPlugin::view_contributions() const {
    ViewContribution view;
    view.viewId = "layout_manager";
    view.title = "Layout Manager";
    view.qmlSource = "LayoutView.qml";
    view.qmlHotDir = STR(RBCE_PLUGIN_PATH);
    view.dockArea = "Right";
    view.preferredSize = "280,300";
    view.closable = true;
    view.movable = true;

    return {view};
}

QList<MenuContribution> LayoutPlugin::menu_contributions() const {
    return _menu_contributions;
}

void LayoutPlugin::buildMenuContributions() {
    _menu_contributions.clear();

    if (!_layout_service) {
        return;
    }

    auto layouts = _layout_service->availableLayouts();
    // === View Menu - Layout Switching ===
    // Add layout switching menu items under "View/Layouts"
    for (auto layout : layouts) {
        auto &config = _layout_service->getLayoutConfig(layout);

        MenuContribution menu;
        menu.menuPath = "View/Layouts/" + config.layoutName;
        menu.actionText = config.layoutName;
        menu.actionId = "layout.switch" + layout;
        menu.shortcut = "";
        menu.callback = [this, layout]() {
            if (_layout_service) {
                _layout_service->switchToLayout(layout);
            }
        };
        _menu_contributions.append(menu);
    }
    // === View Menu - Window Visibility ===

    // Toggle Connection Status Panel
    auto layout = _layout_service->currentLayoutId();
    auto config = _layout_service->getLayoutConfig(layout);

    for (auto view : config.views) {
        MenuContribution toggleConnection;
        toggleConnection.menuPath = "View/Panels/" + view.viewId;
        toggleConnection.actionText = view.viewId;
        toggleConnection.actionId = view.viewId;
        toggleConnection.shortcut = "";

        toggleConnection.callback = [this, view]() {
            if (_layout_service) {
                bool visible = _layout_service->isViewVisible(view.viewId);
                _layout_service->setViewVisible(view.viewId, !visible);
            }
        };
        _menu_contributions.append(toggleConnection);
    }

    // === Window Menu - Reset Layout ===
    MenuContribution resetLayout;
    resetLayout.menuPath = "View";
    resetLayout.actionText = "Reset Layout";
    resetLayout.actionId = "view.reset_layout";
    resetLayout.shortcut = "";
    resetLayout.callback = [this]() {
        if (_layout_service) {
            QString currentId = _layout_service->currentLayoutId();
            if (!currentId.isEmpty()) {
                _layout_service->resetLayout(currentId);
            }
        }
    };
    _menu_contributions.append(resetLayout);
}

void LayoutPlugin::register_view_models(QQmlEngine *engine) {
    if (!engine) {
        qWarning() << "LayoutPlugin::register_view_models: engine is null";
        return;
    }
    // Register LayoutViewModel as QML type
    qmlRegisterType<LayoutViewModel>("RoboCute.Layout", 1, 0, "LayoutViewModel");
    qDebug() << "LayoutPlugin: ViewModels registered";
}

QObject *LayoutPlugin::getViewModel(const QString &viewId) {
    if (viewId == "layout_manager" && _view_model) {
        return _view_model;
    }
    return nullptr;
}

// Export factory function
IPluginFactory *createPluginFactory() {
    return new PluginFactory<LayoutPlugin>();
}

}// namespace rbc
