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

    // === View Menu - Layout Switching ===
    // Add layout switching menu items under "View/Layouts"
    if (layoutService_->hasLayout("scene_editing")) {
        MenuContribution sceneEditing;
        sceneEditing.menuPath = "View/Layouts";
        sceneEditing.actionText = "Scene Editing";
        sceneEditing.actionId = "layout.switch.scene_editing";
        sceneEditing.shortcut = "";
        sceneEditing.callback = [this]() {
            if (layoutService_) {
                layoutService_->switchToLayout("scene_editing");
            }
        };
        menuContributions_.append(sceneEditing);
    }

    if (layoutService_->hasLayout("aigc")) {
        MenuContribution aigc;
        aigc.menuPath = "View/Layouts";
        aigc.actionText = "AIGC";
        aigc.actionId = "layout.switch.aigc";
        aigc.shortcut = "";
        aigc.callback = [this]() {
            if (layoutService_) {
                layoutService_->switchToLayout("aigc");
            }
        };
        menuContributions_.append(aigc);
    }

    // === View Menu - Window Visibility ===
    // Toggle Layout Manager Panel
    MenuContribution toggleLayoutManager;
    toggleLayoutManager.menuPath = "View/Panels";
    toggleLayoutManager.actionText = "Layout Manager";
    toggleLayoutManager.actionId = "view.toggle.layout_manager";
    toggleLayoutManager.shortcut = "";
    toggleLayoutManager.callback = [this]() {
        if (layoutService_) {
            bool visible = layoutService_->isViewVisible("layout_manager");
            layoutService_->setViewVisible("layout_manager", !visible);
        }
    };
    menuContributions_.append(toggleLayoutManager);

    // Toggle Connection Status Panel
    MenuContribution toggleConnection;
    toggleConnection.menuPath = "View/Panels";
    toggleConnection.actionText = "Connection Status";
    toggleConnection.actionId = "view.toggle.connection_status";
    toggleConnection.shortcut = "";
    toggleConnection.callback = [this]() {
        if (layoutService_) {
            bool visible = layoutService_->isViewVisible("connection_status");
            layoutService_->setViewVisible("connection_status", !visible);
        }
    };
    menuContributions_.append(toggleConnection);

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
