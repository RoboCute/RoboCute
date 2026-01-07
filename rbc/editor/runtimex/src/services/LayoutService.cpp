#include "RBCEditorRuntime/services/LayoutService.h"

namespace rbc {
LayoutService::LayoutService(QObject *parent) {}
LayoutService::~LayoutService() {}
QString LayoutService::currentLayoutId() const {
    return currentLayoutId_;
}
QStringList LayoutService::availableLayouts() const {
    return {};
}
QJsonObject LayoutService::getLayoutMetadata(const QString &layoutId) const {
}

bool LayoutService::hasLayout(const QString &layoutId) const {
    return layouts_.contains(layoutId);
}

bool LayoutService::switchToLayout(const QString &layoutId, bool saveCurrentLayout) {
    if (layoutId == currentLayoutId_) {
        return true;// already current layout, nothing to do
    }
    if (!layouts_.contains(layoutId)) {
        qWarning() << "LayoutService::switchToLayout: Layout not found: " << layoutId;
        // emit layoutLoadFailed(layoutId, "Layout not found");
        return false;
    }

    // 1. broadcast start switching event
    // emit layoutSwitchStarted(currentLayoutId_, layoutId);
    isTransitioning_ = true;

    // 2. (optional) save current layout
    // if (saveCurrentLayout && !currentLayoutId_.isEmpty()) {
    //     saveCurrentLayout();
    // }

    // 3. clear current layout (optional: use fade anim)
    // clearAllViews();

    // 4. apply new layout
    applyLayout(layoutId);

    // 5. update new id
    currentLayoutId_ = layoutId;

    // 6. Finish Transition and broadcast event
    isTransitioning_ = false;
    // emit layoutSwitched(layoutId);

    qDebug() << "LayoutService: Switch to layout: " << layoutId;
    return true;
}

bool LayoutService::saveCurrentLayout(const QString &layoutId) {
    return true;
}

void LayoutService::resetLayout(const QString &layoutId) {}

bool LayoutService::createLayout(const QString &layoutId, const QJsonObject &layoutConfig) {
    return true;
}

bool LayoutService::loadLayoutFromFile(const QString &filepath) {
    return true;
}

bool LayoutService::saveLayoutToFile(const QString &layoutId, const QString &filePath) {
    return true;
}

bool LayoutService::deleteLayout(const QString &layoutId) {
    return true;
}

bool LayoutService::cloneLayout(const QString &sourceId, const QString &newId, const QString &newName) {
    return true;
}

void LayoutService::setViewVisible(const QString &viewId, bool visible) {
    if (!currentViewStates_.contains(viewId)) {
        return;
    }

    ViewState &viewState = currentViewStates_[viewId];

    if (viewState.dockWidget) {
        if (visible) {
            viewState.dockWidget->show();
        } else {
            viewState.dockWidget->hide();
        }
        viewState.visible = visible;
        // emit viewStateChanged(viewId, visible);
    }
}

bool LayoutService::isViewVisible(const QString &viewId) const {
    if (!currentViewStates_.contains(viewId)) [[unlikely]] {
        qWarning() << "Query View Visible with viewId " << viewId << "Not Valid";
        return false;
    }
    return currentViewStates_[viewId].visible;
}

void LayoutService::resizeView(const QString &viewId, int width, int height) {}
void LayoutService::moveViewToDockArea(const QString &viewId, const QString &dockArea) {}

void LayoutService::initialize(WindowManager *windowManager, EditorPluginManager *pluginManager) {
}

void LayoutService::applyLayout(const QString &layoutId) {
    if (!layouts_.contains(layoutId)) {
        qWarning() << "LayoutService::applyLayout: Layout not found:" << layoutId;
        return;
    }
    const LayoutConfig &config = layouts_[layoutId];
    // 1. iterate all view config
    for (auto it = config.views.begin(); it != config.views.end(); ++it) {
        const QString &viewId = it.key();
        const ViewState &viewState = it.value();

        // create or update view
        if (viewState.visible) {

        } else {
            // hide or move
        }

        // 6. update current view status
        currentViewStates_[viewId] = viewState;
    }

    // 7. apply docking arrangement
    // applyDockArrangements(config.dockArrangements);

    // 8. set central widget
}

// QDockWidget *LayoutService::createOrUpdateView(const ViewState &viewState) {
// }

// void LayoutService::applyDockArrangements(const QJsonObject &arrangements) {
// }

void LayoutService::loadBuiltInLayouts() {}
void LayoutService::loadUserLayouts() {}
QString LayoutService::layoutConfigDirectory() const {
    return "";
}

}// namespace rbc