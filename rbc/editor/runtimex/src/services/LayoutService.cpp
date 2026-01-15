#include "RBCEditorRuntime/services/LayoutService.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include <QJsonDocument>
#include <QDebug>
#include <QCoreApplication>

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
    
    const LayoutConfig &config = layouts_[layoutId];
    qDebug() << "LayoutService::applyLayout: Applying layout:" << layoutId;
    
    // 1. First, handle the central widget if specified
    if (!config.centralWidgetId.isEmpty()) {
        if (setCentralWidget(config.centralWidgetId)) {
            qDebug() << "LayoutService::applyLayout: Set central widget:" << config.centralWidgetId;
        } else {
            qWarning() << "LayoutService::applyLayout: Failed to set central widget:" << config.centralWidgetId;
        }
    }
    
    // 2. Update current layout ID
    currentLayoutId_ = layoutId;
    
    // 3. Update view states (for future reference)
    for (auto it = config.views.begin(); it != config.views.end(); ++it) {
        const QString &viewId = it.key();
        const ViewState &viewState = it.value();
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
    
    // Find the plugin that provides this view
    IEditorPlugin *plugin = findPluginForView(viewId);
    if (!plugin) {
        qWarning() << "LayoutService::setCentralWidget: Could not find plugin for view:" << viewId;
        return false;
    }
    
    // Try to get the native widget first
    QWidget *widget = plugin->getNativeWidget(viewId);
    if (!widget) {
        qWarning() << "LayoutService::setCentralWidget: Could not get widget for view:" << viewId;
        return false;
    }
    
    // Check if the widget is currently in a dock widget
    QDockWidget *parentDock = qobject_cast<QDockWidget *>(widget->parent());
    if (parentDock) {
        // Remove the widget from the dock widget
        parentDock->setWidget(nullptr);
        // Hide or remove the empty dock widget
        parentDock->hide();
        qDebug() << "LayoutService::setCentralWidget: Removed widget from dock:" << viewId;
    }
    
    // Determine if this is an external widget (managed by plugin)
    bool isExternal = true;  // Native widgets from plugins are typically external
    
    // Check the native view contribution for isExternalManaged flag
    for (const auto &nativeView : plugin->native_view_contributions()) {
        if (nativeView.viewId == viewId) {
            isExternal = nativeView.isExternalManaged;
            break;
        }
    }
    
    // Set as central widget
    bool success = windowManager_->setCentralWidget(widget, isExternal);
    
    if (success) {
        // Update view state
        if (currentViewStates_.contains(viewId)) {
            currentViewStates_[viewId].isCentralWidget = true;
            currentViewStates_[viewId].centralWidget = widget;
        }
    }
    
    return success;
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
    // Get the application directory to find layout files
    QString appDir = QCoreApplication::applicationDirPath();
    
    // Try multiple possible paths for layout files
    QStringList searchPaths = {
        appDir + "/layouts",
        appDir + "/../rbc/editor/runtimex/ui/layout",
        appDir + "/../../rbc/editor/runtimex/ui/layout",
        appDir + "/../../../rbc/editor/runtimex/ui/layout",
        // For development/debug builds
        QStringLiteral(":/layouts"),  // Qt resource path
    };
    
    // Also check relative to source (for development)
#ifdef RBC_SOURCE_DIR
    QString sourcePath = QStringLiteral(RBC_SOURCE_DIR);
    if (!sourcePath.isEmpty()) {
        searchPaths.prepend(sourcePath + "/rbc/editor/runtimex/ui/layout");
    }
#endif
    
    QString layoutDir;
    for (const QString &path : searchPaths) {
        QDir dir(path);
        if (dir.exists()) {
            layoutDir = path;
            qDebug() << "LayoutService::loadBuiltInLayouts: Found layout directory:" << layoutDir;
            break;
        }
    }
    
    if (layoutDir.isEmpty()) {
        qWarning() << "LayoutService::loadBuiltInLayouts: Could not find layout directory";
        qWarning() << "Searched paths:" << searchPaths;
        return;
    }
    
    // Load all JSON files from the layout directory
    QDir dir(layoutDir);
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo &fileInfo : files) {
        QString filePath = fileInfo.absoluteFilePath();
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
    
    qDebug() << "LayoutService::loadBuiltInLayouts: Loaded" << layouts_.size() << "layouts";
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

}// namespace rbc