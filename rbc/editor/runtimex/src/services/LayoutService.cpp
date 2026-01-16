#include "RBCEditorRuntime/services/LayoutService.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include <QJsonDocument>
#include <QDebug>
#include <QLabel>

namespace rbc {

LayoutService::LayoutService(QObject *parent)
    : ILayoutService(parent) {
}

LayoutService::~LayoutService() {
}
QString LayoutService::currentLayoutId() const {
    return currentLayoutId_;
}
QStringList LayoutService::availableLayouts() const {
    return {};
}
QJsonObject LayoutService::getLayoutMetadata(const QString &layoutId) const {
    return {};
}

bool LayoutService::hasLayout(const QString &layoutId) const {
    return layouts_.contains(layoutId);
}

bool LayoutService::switchToLayout(const QString &layoutId, bool saveCurrentLayout) {
    if (layoutId == currentLayoutId_) {
        qDebug() << "LayoutService::switchToLayout: Already in layout:" << layoutId;
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

    // 3. Calculate which views can be preserved/reused between layouts
    QSet<QString> viewsToPreserve;
    const LayoutConfig &newConfig = layouts_[layoutId];
    for (auto it = newConfig.views.begin(); it != newConfig.views.end(); ++it) {
        viewsToPreserve.insert(it.key());
    }
    
    // 4. Clear current layout, preserving views that will be reused
    clearCurrentLayout(viewsToPreserve);

    // 5. apply new layout
    applyLayout(layoutId);

    // 6. update new id (already done in applyLayout, but ensure consistency)
    currentLayoutId_ = layoutId;

    // 7. Finish Transition and broadcast event
    isTransitioning_ = false;
    // emit layoutSwitched(layoutId);

    qDebug() << "LayoutService: Switch to layout: " << layoutId;
    return true;
}

bool LayoutService::saveCurrentLayout(const QString &layoutId) {
    return true;
}

void LayoutService::resetLayout(const QString &layoutId) {
    // Reset to the built-in layout configuration
    if (!layouts_.contains(layoutId)) {
        qWarning() << "LayoutService::resetLayout: Layout not found:" << layoutId;
        return;
    }
    
    // Clear current layout completely
    clearCurrentLayout();
    
    // Re-apply the layout
    applyLayout(layoutId);
    
    qDebug() << "LayoutService::resetLayout: Layout reset:" << layoutId;
}

void LayoutService::clearCurrentLayout(const QSet<QString> &preserveViews) {
    if (!windowManager_) {
        qWarning() << "LayoutService::clearCurrentLayout: WindowManager not initialized";
        return;
    }
    
    QMainWindow *mainWindow = windowManager_->main_window();
    if (!mainWindow) {
        qWarning() << "LayoutService::clearCurrentLayout: MainWindow not available";
        return;
    }
    
    qDebug() << "LayoutService::clearCurrentLayout: Clearing layout, preserving views:" << preserveViews;
    
    // Note: We don't need to handle the central widget here anymore
    // The central widget container is stable and managed by setCentralWidget()
    // The actual widget inside will be swapped when applying the new layout
    
    // Process all current dock widgets
    for (auto it = currentViewStates_.begin(); it != currentViewStates_.end(); ++it) {
        const QString &viewId = it.key();
        ViewState &viewState = it.value();
        
        if (viewState.dockWidget) {
            if (preserveViews.contains(viewId)) {
                // This view will be reused - hide it but don't delete
                viewState.dockWidget->hide();
                qDebug() << "LayoutService::clearCurrentLayout: Hiding preserved dock:" << viewId;
            } else {
                // This view is not needed in new layout - remove it
                // First, if the dock has a widget from a plugin, detach it
                QWidget *dockContent = viewState.dockWidget->widget();
                if (dockContent) {
                    // IMPORTANT: First detach widget from dock, then set parent to nullptr
                    // Otherwise deleteLater() will delete the widget as well
                    viewState.dockWidget->setWidget(nullptr);
                    dockContent->setParent(nullptr);  // Detach from dock widget's ownership
                }
                
                // Remove the dock widget from main window
                mainWindow->removeDockWidget(viewState.dockWidget);
                viewState.dockWidget->deleteLater();
                viewState.dockWidget = nullptr;
                qDebug() << "LayoutService::clearCurrentLayout: Removed dock:" << viewId;
            }
        }
    }
    
    // 3. Clear view states that are not preserved
    for (auto it = currentViewStates_.begin(); it != currentViewStates_.end();) {
        if (!preserveViews.contains(it.key())) {
            it = currentViewStates_.erase(it);
        } else {
            ++it;
        }
    }
    
    qDebug() << "LayoutService::clearCurrentLayout: Layout cleared, remaining views:" << currentViewStates_.keys();
}

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
    windowManager_ = windowManager;
    pluginManager_ = pluginManager;
    qDebug() << "LayoutService::initialize: Initialized with WindowManager and PluginManager";
}

void LayoutService::applyLayout(const QString &layoutId) {
    if (!layouts_.contains(layoutId)) {
        qWarning() << "LayoutService::applyLayout: Layout not found:" << layoutId;
        return;
    }
    
    if (!windowManager_ || !pluginManager_) {
        qWarning() << "LayoutService::applyLayout: WindowManager or PluginManager not initialized";
        return;
    }
    
    QMainWindow *mainWindow = windowManager_->main_window();
    if (!mainWindow) {
        qWarning() << "LayoutService::applyLayout: MainWindow not available";
        return;
    }
    
    LayoutConfig &config = layouts_[layoutId];
    qDebug() << "LayoutService::applyLayout: Applying layout:" << layoutId;
    
    // 1. First, handle the central widget if specified
    // setCentralWidget() now handles everything: extracting from dock, managing container, etc.
    if (!config.centralWidgetId.isEmpty()) {
        if (setCentralWidget(config.centralWidgetId)) {
            qDebug() << "LayoutService::applyLayout: Set central widget:" << config.centralWidgetId;
            // Update view state - setCentralWidget already updates currentViewStates_ internally
            // but we also need to sync with config
            ViewState centralState = config.views.value(config.centralWidgetId);
            centralState.isCentralWidget = true;
            centralState.centralWidget = currentViewStates_.contains(config.centralWidgetId) 
                ? currentViewStates_[config.centralWidgetId].centralWidget 
                : nullptr;
            currentViewStates_[config.centralWidgetId] = centralState;
        } else {
            qWarning() << "LayoutService::applyLayout: Failed to set central widget:" << config.centralWidgetId;
            // Don't set a placeholder on the container - it would replace our stable container
            // The container will just be empty, which is fine
        }
    }
    
    // 2. Update current layout ID
    currentLayoutId_ = layoutId;
    
    // 3. Create or reuse dock widgets for all views defined in layout (except center)
    for (auto it = config.views.begin(); it != config.views.end(); ++it) {
        const QString &viewId = it.key();
        ViewState viewState = it.value();
        
        // Skip center views (handled as central widget)
        if (viewState.dockArea == "Center") {
            // Already handled above
            continue;
        }
        
        // Check if we already have a dock widget for this view from previous layout
        QDockWidget *existingDock = nullptr;
        if (currentViewStates_.contains(viewId)) {
            existingDock = currentViewStates_[viewId].dockWidget;
        }
        
        if (existingDock) {
            // Reuse existing dock widget - just update its position and show it
            Qt::DockWidgetArea newArea = parseDockArea(viewState.dockArea);
            Qt::DockWidgetArea currentArea = mainWindow->dockWidgetArea(existingDock);
            
            // Move to correct area if needed
            if (currentArea != newArea && newArea != Qt::NoDockWidgetArea) {
                mainWindow->removeDockWidget(existingDock);
                mainWindow->addDockWidget(newArea, existingDock);
            }
            
            // Show the dock
            existingDock->show();
            viewState.dockWidget = existingDock;
            qDebug() << "LayoutService::applyLayout: Reused existing dock for view:" << viewId;
        } else {
            // Create new dock widget for this view
            QDockWidget *dock = createViewDock(viewState);
            if (dock) {
                viewState.dockWidget = dock;
                qDebug() << "LayoutService::applyLayout: Created new dock for view:" << viewId;
            }
        }
        
        // Update visibility based on layout config
        if (viewState.dockWidget) {
            viewState.dockWidget->setVisible(viewState.visible);
        }
        
        currentViewStates_[viewId] = viewState;
    }
    
    // 4. Apply dock arrangements (splits, tabs, etc.)
    applyDockArrangements(config.dockArrangements);
    
    qDebug() << "LayoutService::applyLayout: Layout applied successfully:" << layoutId;
}

bool LayoutService::setCentralWidget(const QString &viewId) {
    if (!windowManager_ || !pluginManager_) {
        qWarning() << "LayoutService::setCentralWidget: WindowManager or PluginManager not initialized";
        return false;
    }
    
    QMainWindow *mainWindow = windowManager_->main_window();
    if (!mainWindow) {
        qWarning() << "LayoutService::setCentralWidget: MainWindow not available";
        return false;
    }
    
    // 1. Ensure central widget container exists (create once, reuse forever)
    // This container acts as a stable wrapper - the actual widget is placed inside it
    // This prevents Qt from deleting widgets when we switch central widgets
    if (!centralWidgetContainer_) {
        centralWidgetContainer_ = new QWidget();
        centralWidgetContainer_->setObjectName("CentralWidgetContainer");
        
        centralContainerLayout_ = new QVBoxLayout(centralWidgetContainer_);
        centralContainerLayout_->setContentsMargins(0, 0, 0, 0);
        centralContainerLayout_->setSpacing(0);
        
        // Set container as central widget (only once)
        // Container is NOT external - we own it
        windowManager_->setCentralWidget(centralWidgetContainer_, false);
        
        qDebug() << "LayoutService::setCentralWidget: Created central widget container";
    }
    
    // 2. If already showing this view, nothing to do
    if (currentCentralViewId_ == viewId) {
        qDebug() << "LayoutService::setCentralWidget: Already showing:" << viewId;
        return true;
    }
    
    // 3. Remove the old widget from container (but don't delete it - plugin owns it)
    if (!currentCentralViewId_.isEmpty()) {
        // Update old view state
        if (currentViewStates_.contains(currentCentralViewId_)) {
            currentViewStates_[currentCentralViewId_].isCentralWidget = false;
            currentViewStates_[currentCentralViewId_].centralWidget = nullptr;
        }
        
        // Remove all widgets from layout (without deleting them)
        while (centralContainerLayout_->count() > 0) {
            QLayoutItem* item = centralContainerLayout_->takeAt(0);
            if (item && item->widget()) {
                // Detach from container but don't delete
                item->widget()->setParent(nullptr);
                item->widget()->hide();  // Hide it for now
            }
            delete item;  // Delete the layout item, not the widget
        }
        
        qDebug() << "LayoutService::setCentralWidget: Removed old central widget:" << currentCentralViewId_;
    }
    
    // 4. Find the plugin that provides this view
    IEditorPlugin *plugin = findPluginForView(viewId);
    if (!plugin) {
        qWarning() << "LayoutService::setCentralWidget: Could not find plugin for view:" << viewId;
        return false;
    }
    
    // 5. Get the widget from plugin
    QWidget *widget = plugin->getNativeWidget(viewId);
    if (!widget) {
        qWarning() << "LayoutService::setCentralWidget: Could not get widget for view:" << viewId;
        return false;
    }
    
    // 6. If widget is currently in a dock widget, remove it from there first
    QDockWidget *parentDock = qobject_cast<QDockWidget *>(widget->parent());
    if (parentDock) {
        parentDock->setWidget(nullptr);
        widget->setParent(nullptr);
        
        // Remove and delete the empty dock widget
        parentDock->hide();
        mainWindow->removeDockWidget(parentDock);
        parentDock->deleteLater();
        
        // Clear dock widget reference in view state
        if (currentViewStates_.contains(viewId)) {
            currentViewStates_[viewId].dockWidget = nullptr;
        }
        
        qDebug() << "LayoutService::setCentralWidget: Removed widget from dock:" << viewId;
    } else if (widget->parent() && widget->parent() != centralWidgetContainer_) {
        // Detach from any other parent
        widget->setParent(nullptr);
    }
    
    // 7. Add widget to central container
    widget->setParent(centralWidgetContainer_);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    centralContainerLayout_->addWidget(widget);
    widget->show();
    
    // 8. Update state
    currentCentralViewId_ = viewId;
    if (currentViewStates_.contains(viewId)) {
        currentViewStates_[viewId].isCentralWidget = true;
        currentViewStates_[viewId].centralWidget = widget;
    }
    
    qDebug() << "LayoutService::setCentralWidget: Set new central widget:" << viewId;
    return true;
}

void LayoutService::applyDockArrangements(const QJsonObject &arrangements) {
    if (!windowManager_) {
        return;
    }
    
    QMainWindow *mainWindow = windowManager_->main_window();
    if (!mainWindow) {
        return;
    }
    
    // Process each dock area's arrangement
    QStringList areas = {"Left", "Right", "Top", "Bottom"};
    
    for (const QString &areaName : areas) {
        if (!arrangements.contains(areaName)) {
            continue;
        }
        
        QJsonObject areaConfig = arrangements[areaName].toObject();
        QString splitOrientation = areaConfig["split_orientation"].toString();
        QJsonArray widgets = areaConfig["widgets"].toArray();
        
        if (widgets.size() <= 1) {
            continue;
        }
        
        // Collect the dock widgets for this area
        QList<QDockWidget *> dockWidgets;
        for (const QJsonValue &val : widgets) {
            QString viewId = val.toString();
            // Find the dock widget with this object name
            QDockWidget *dock = mainWindow->findChild<QDockWidget *>(viewId);
            if (dock) {
                dockWidgets.append(dock);
            }
        }
        
        if (dockWidgets.size() <= 1) {
            continue;
        }
        
        // Apply split or tabify based on configuration
        QString arrangeType = areaConfig["type"].toString();
        if (arrangeType == "tabbed") {
            // Tabify dock widgets
            for (int i = 1; i < dockWidgets.size(); ++i) {
                mainWindow->tabifyDockWidget(dockWidgets[0], dockWidgets[i]);
            }
            // Raise the first one
            dockWidgets[0]->raise();
        } else {
            // Split dock widgets (vertical or horizontal)
            Qt::Orientation orientation = (splitOrientation == "horizontal") 
                ? Qt::Horizontal : Qt::Vertical;
            for (int i = 1; i < dockWidgets.size(); ++i) {
                mainWindow->splitDockWidget(dockWidgets[i - 1], dockWidgets[i], orientation);
            }
        }
    }
}

void LayoutService::loadBuiltInLayouts() {
    // Built-in layouts are embedded as Qt resources in rbc_editor.qrc
    // They are located at :/ui/layout/*.json
    QStringList builtInLayoutFiles = {
        QStringLiteral(":/ui/layout/scene_editing.json"),
        QStringLiteral(":/ui/layout/aigc.json"),
    };
    
    for (const QString &filePath : builtInLayoutFiles) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "LayoutService::loadBuiltInLayouts: Failed to open:" << filePath;
            continue;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "LayoutService::loadBuiltInLayouts: JSON parse error in" << filePath 
                       << ":" << parseError.errorString();
            continue;
        }
        
        if (!doc.isObject()) {
            qWarning() << "LayoutService::loadBuiltInLayouts: Invalid JSON format in" << filePath;
            continue;
        }
        
        LayoutConfig config;
        if (parseLayoutConfig(doc.object(), config)) {
            config.isBuiltIn = true;
            layouts_[config.layoutId] = config;
            qDebug() << "LayoutService::loadBuiltInLayouts: Loaded layout:" << config.layoutId 
                     << "(" << config.layoutName << ")";
        }
    }
    
    qDebug() << "LayoutService::loadBuiltInLayouts: Loaded" << layouts_.size() << "built-in layouts";
}

void LayoutService::loadUserLayouts() {
    // TODO: Load from user config directory
    QString userLayoutDir = layoutConfigDirectory();
    if (userLayoutDir.isEmpty()) {
        return;
    }
    
    QDir dir(userLayoutDir);
    if (!dir.exists()) {
        return;
    }
    
    // Similar to loadBuiltInLayouts but for user layouts
}

QString LayoutService::layoutConfigDirectory() const {
    // Return user config directory for layouts
    return configDirectory_;
}

bool LayoutService::parseLayoutConfig(const QJsonObject &json, LayoutConfig &config) {
    config.layoutId = json["layout_id"].toString();
    config.layoutName = json["layout_name"].toString();
    config.description = json["description"].toString();
    config.version = json["version"].toString();
    
    if (config.layoutId.isEmpty()) {
        qWarning() << "LayoutService::parseLayoutConfig: Missing layout_id";
        return false;
    }
    
    // Parse views array
    QJsonArray viewsArray = json["views"].toArray();
    for (const QJsonValue &viewVal : viewsArray) {
        if (!viewVal.isObject()) continue;
        
        QJsonObject viewObj = viewVal.toObject();
        ViewState view;
        view.pluginId = viewObj["plugin_id"].toString();
        view.viewId = viewObj["view_id"].toString();
        view.dockArea = viewObj["dock_area"].toString();
        view.visible = viewObj["visible"].toBool(true);
        view.minimized = viewObj["minimized"].toBool(false);
        
        if (viewObj.contains("size")) {
            QJsonObject sizeObj = viewObj["size"].toObject();
            view.width = sizeObj["width"].toInt(-1);
            view.height = sizeObj["height"].toInt(-1);
        }
        
        view.tabGroup = viewObj["tab_group"].toString();
        view.tabIndex = viewObj["tab_index"].toInt(0);
        view.properties = viewObj["properties"].toObject();
        
        if (!view.viewId.isEmpty()) {
            config.views[view.viewId] = view;
        }
    }
    
    // Parse dock arrangements
    config.dockArrangements = json["dock_arrangements"].toObject();
    
    // Extract central widget ID from dock_arrangements
    if (config.dockArrangements.contains("Center")) {
        QJsonObject centerConfig = config.dockArrangements["Center"].toObject();
        config.centralWidgetId = centerConfig["central_widget"].toString();
        
        // Mark the view as central widget
        if (config.views.contains(config.centralWidgetId)) {
            config.views[config.centralWidgetId].isCentralWidget = true;
        }
    }
    
    return true;
}

Qt::DockWidgetArea LayoutService::parseDockArea(const QString &dockArea) {
    if (dockArea == "Left") {
        return Qt::LeftDockWidgetArea;
    } else if (dockArea == "Right") {
        return Qt::RightDockWidgetArea;
    } else if (dockArea == "Top") {
        return Qt::TopDockWidgetArea;
    } else if (dockArea == "Bottom") {
        return Qt::BottomDockWidgetArea;
    } else if (dockArea == "Center") {
        return Qt::NoDockWidgetArea;  // Center is not a dock area, it's the central widget
    }
    return Qt::LeftDockWidgetArea;  // Default
}

IEditorPlugin *LayoutService::findPluginForView(const QString &viewId) {
    if (!pluginManager_) {
        return nullptr;
    }
    
    // Search through all loaded plugins
    for (IEditorPlugin *plugin : pluginManager_->getLoadedPlugins()) {
        if (!plugin) continue;
        
        // Check native view contributions
        for (const auto &nativeView : plugin->native_view_contributions()) {
            if (nativeView.viewId == viewId) {
                return plugin;
            }
        }
        
        // Check QML view contributions
        for (const auto &view : plugin->view_contributions()) {
            if (view.viewId == viewId) {
                return plugin;
            }
        }
    }
    
    return nullptr;
}

QDockWidget *LayoutService::createViewDock(const ViewState &viewState) {
    if (!windowManager_ || !pluginManager_) {
        return nullptr;
    }
    
    QMainWindow *mainWindow = windowManager_->main_window();
    if (!mainWindow) {
        return nullptr;
    }
    
    const QString &viewId = viewState.viewId;
    
    // Check if this view is currently in the central container - if so, remove it from there
    if (currentCentralViewId_ == viewId && centralContainerLayout_) {
        // Remove the widget from central container
        while (centralContainerLayout_->count() > 0) {
            QLayoutItem* item = centralContainerLayout_->takeAt(0);
            if (item && item->widget()) {
                item->widget()->setParent(nullptr);
            }
            delete item;
        }
        
        // Update state
        currentCentralViewId_.clear();
        if (currentViewStates_.contains(viewId)) {
            currentViewStates_[viewId].isCentralWidget = false;
            currentViewStates_[viewId].centralWidget = nullptr;
        }
        
        qDebug() << "LayoutService::createViewDock: Removed widget from central container:" << viewId;
    }
    
    // Find the plugin that provides this view
    IEditorPlugin *plugin = findPluginForView(viewId);
    if (!plugin) {
        qWarning() << "LayoutService::createViewDock: Plugin not found for view:" << viewId
                   << "- creating placeholder";
        return createPlaceholderDock(viewId, viewId, viewState.dockArea);
    }
    
    // Try to find and create the view from plugin contributions
    // First, check native view contributions
    for (const auto &nativeView : plugin->native_view_contributions()) {
        if (nativeView.viewId == viewId) {
            QWidget *widget = plugin->getNativeWidget(viewId);
            if (!widget) {
                qWarning() << "LayoutService::createViewDock: Widget not available for view:" << viewId
                           << "- creating placeholder";
                return createPlaceholderDock(viewId, nativeView.title, viewState.dockArea);
            }
            
            // Check if widget is already in another parent - detach it first
            if (widget->parent()) {
                QDockWidget *parentDock = qobject_cast<QDockWidget *>(widget->parent());
                if (parentDock) {
                    parentDock->setWidget(nullptr);
                }
                widget->setParent(nullptr);  // Detach from any parent
            }
            
            // Create a modified contribution with the correct dock area from layout config
            NativeViewContribution modifiedContrib = nativeView;
            modifiedContrib.dockArea = viewState.dockArea;
            
            QObject *viewModel = plugin->getViewModel(viewId);
            return windowManager_->createDockableView(modifiedContrib, widget, viewModel);
        }
    }
    
    // Check QML view contributions
    for (const auto &view : plugin->view_contributions()) {
        if (view.viewId == viewId) {
            QObject *viewModel = plugin->getViewModel(viewId);
            if (!viewModel) {
                qWarning() << "LayoutService::createViewDock: ViewModel not available for view:" << viewId
                           << "- creating placeholder";
                return createPlaceholderDock(viewId, view.title, viewState.dockArea);
            }
            
            // Create a modified contribution with the correct dock area from layout config
            ViewContribution modifiedContrib = view;
            modifiedContrib.dockArea = viewState.dockArea;
            
            return windowManager_->createDockableView(modifiedContrib, viewModel);
        }
    }
    
    // View not found in plugin contributions
    qWarning() << "LayoutService::createViewDock: View" << viewId << "not found in plugin contributions"
               << "- creating placeholder";
    return createPlaceholderDock(viewId, viewId, viewState.dockArea);
}

QDockWidget *LayoutService::createPlaceholderDock(const QString &viewId, const QString &title, const QString &dockArea) {
    if (!windowManager_) {
        return nullptr;
    }
    
    auto *label = new QLabel(QString("View '%1' not available").arg(viewId));
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("QLabel { color: #888; font-style: italic; }");
    
    Qt::DockWidgetArea area = parseDockArea(dockArea);
    
    return windowManager_->createDockableView(
        viewId,
        title.isEmpty() ? viewId + " (Placeholder)" : title + " (Placeholder)",
        label,
        area,
        QDockWidget::NoDockWidgetFeatures,  // Placeholder cannot be closed/moved
        Qt::DockWidgetAreas(area),
        false  // Not external - we own this widget
    );
}

}// namespace rbc