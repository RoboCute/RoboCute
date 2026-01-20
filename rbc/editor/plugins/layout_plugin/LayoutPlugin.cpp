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

    context_ = context;

    // Get LayoutService from PluginManager
    layoutService_ = context->getService<LayoutService>();

    if (!layoutService_) {
        qWarning() << "LayoutPlugin::load: LayoutService should be registered before LayoutPlugin load";
        return false;
    }

    // Create ViewModel
    viewModel_ = new LayoutViewModel(layoutService_, this);

    // Build menu contributions
    buildMenuContributions();

    qDebug() << "LayoutPlugin loaded successfully";

    return true;
}

bool LayoutPlugin::unload() {
    if (viewModel_) {
        viewModel_->deleteLater();
        viewModel_ = nullptr;
    }

    menuContributions_.clear();
    layoutService_ = nullptr;
    context_ = nullptr;

    qDebug() << "LayoutPlugin unloaded";
    return true;
}

bool LayoutPlugin::reload() {
    qDebug() << "LayoutPlugin reloading...";

    // Save current layout
    QString savedLayoutId = layoutService_ ? layoutService_->currentLayoutId() : QString();

    // Unload and reload
    if (!unload()) {
        return false;
    }

    if (!load(context_)) {
        return false;
    }

    // Restore layout
    if (layoutService_ && !savedLayoutId.isEmpty()) {
        layoutService_->switchToLayout(savedLayoutId, false);
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
    return menuContributions_;
}

void LayoutPlugin::buildMenuContributions() {
    menuContributions_.clear();

    if (!layoutService_) {
        return;
    }

    auto layouts = layoutService_->availableLayouts();
    // === View Menu - Layout Switching ===
    // Add layout switching menu items under "View/Layouts"
    for (auto layout : layouts) {
        auto &config = layoutService_->getLayoutConfig(layout);

        MenuContribution menu;
        menu.menuPath = "View/Layouts/" + config.layoutName;
        menu.actionText = config.layoutName;
        menu.actionId = "layout.switch" + layout;
        menu.shortcut = "";
        menu.callback = [this, layout]() {
            if (layoutService_) {
                layoutService_->switchToLayout(layout);
            }
        };
        menuContributions_.append(menu);
    }
    // === View Menu - Window Visibility ===

    // Toggle Connection Status Panel
    auto layout = layoutService_->currentLayoutId();
    auto config = layoutService_->getLayoutConfig(layout);

    for (auto view : config.views) {
        MenuContribution toggleConnection;
        toggleConnection.menuPath = "View/Panels/" + view.viewId;
        toggleConnection.actionText = view.viewId;
        toggleConnection.actionId = view.viewId;
        toggleConnection.shortcut = "";

        toggleConnection.callback = [this, view]() {
            if (layoutService_) {
                bool visible = layoutService_->isViewVisible(view.viewId);
                layoutService_->setViewVisible(view.viewId, !visible);
            }
        };
        menuContributions_.append(toggleConnection);
    }

    // === Window Menu - Reset Layout ===
    MenuContribution resetLayout;
    resetLayout.menuPath = "View";
    resetLayout.actionText = "Reset Layout";
    resetLayout.actionId = "view.reset_layout";
    resetLayout.shortcut = "";
    resetLayout.callback = [this]() {
        if (layoutService_) {
            QString currentId = layoutService_->currentLayoutId();
            if (!currentId.isEmpty()) {
                layoutService_->resetLayout(currentId);
            }
        }
    };
    menuContributions_.append(resetLayout);
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
    if (viewId == "layout_manager" && viewModel_) {
        return viewModel_;
    }
    return nullptr;
}

// 导出工厂函数
IPluginFactory *createPluginFactory() {
    return new PluginFactory<LayoutPlugin>();
}

}// namespace rbc
