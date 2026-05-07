#pragma once
#include <QString>
#include <QObject>
#include <functional>

namespace rbc {
enum struct PluginState {
};

// QML view contribution for basic component classes and simple interactive UIs, with convenient hot-reload support
struct ViewContribution {
    QString viewId;   // the unique identifier
    QString title;    // display title
    QString qmlSource;// relevant path for QML
    QString dockArea; // (Left/Right/Top/Bottom/Center/...)
    QString preferredSize;
    bool closable;
    bool movable;
    QString qmlHotDir;// hot reload qml directory
};

/**
 * @brief Native Widget view contribution
 *
 * Used for registering views that need QWidget instead of QML, such as ViewportWidget.
 * Distinguished from ViewContribution for clearer intent.
 * View Contribution defined using native QWidget instead of QML
 */
struct NativeViewContribution {
    QString viewId;
    QString title;
    QString dockArea;// Left/Right/Top/Bottom/Center

    bool closable = true;
    bool movable = true;
    bool floatable = true;
    // Qt::DockWidgetAreas allowedAreas = Qt::AllDockWidgetAreas;

    // Whether the plugin manages the widget lifecycle itself
    // true: WindowManager does not delete the widget (releases reference on cleanup)
    // false: widget ownership is transferred to WindowManager
    bool isExternalManaged = false;
};

// Menu interface
struct MenuContribution {
    QString menuPath;              // e.g., "File", "File/Open", "Edit/Preferences"
    QString actionText;            // Display text for the menu item
    QString actionId;              // Unique identifier for the action
    QString shortcut;              // Optional keyboard shortcut (e.g., "Ctrl+O")
    std::function<void()> callback;// Callback function to execute when menu item is triggered
};

// Toolbar interface
struct ToolbarContribution {
    QString toolbarId;             // Unique identifier for the toolbar
    QString toolbarName;           // Display name for the toolbar
    QString actionId;              // Unique identifier for the action
    QString actionText;            // Display text for the toolbar button
    QString iconPath;              // Optional icon path
    std::function<void()> callback;// Callback function to execute when toolbar button is triggered
};

}// namespace rbc