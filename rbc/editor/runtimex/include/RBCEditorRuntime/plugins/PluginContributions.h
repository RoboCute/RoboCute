#pragma once
#include <QString>
#include <QObject>
#include <functional>

namespace rbc {
enum struct PluginState {
};

struct ViewContribution {
    QString viewId;   // the unique identifier
    QString title;    // display title
    QString qmlSource;// relevant path for QML
    QString dockArea; // (Left/Right/Top/Bottom/Center/...)
    QString preferredSize;
    bool closable;
    bool movable;
};

/**
 * @brief Native Widget 视图贡献
 * 
 * 用于注册需要使用 QWidget 而非 QML 的视图，如 ViewportWidget。
 * 与 ViewContribution 区分开，更清晰地表达意图。
 * 使用Native QWidget而非QMLK定义的View Contribution
 */

struct NativeViewContribution {
    QString viewId;
    QString title;
    QString dockArea;// Left/Right/Top/Buttom/Center

    bool closable = true;
    bool movable = true;
    bool floatable = true;
    // Qt::DockWidgetAreas allowedAreas = Qt::AllDockWidgetAreas;

    // 是否由插件自行管理 widget 生命周期
    // true: WindowManager 不负责删除 widget（cleanup 时释放引用）
    // false: widget 所有权转移给 WindowManager
    bool isExternalManaged = false;
};

struct MenuContribution {
    QString menuPath;              // e.g., "File", "File/Open", "Edit/Preferences"
    QString actionText;            // Display text for the menu item
    QString actionId;              // Unique identifier for the action
    QString shortcut;              // Optional keyboard shortcut (e.g., "Ctrl+O")
    std::function<void()> callback;// Callback function to execute when menu item is triggered
};

struct ToolbarContribution {
    QString toolbarId;             // Unique identifier for the toolbar
    QString toolbarName;           // Display name for the toolbar
    QString actionId;              // Unique identifier for the action
    QString actionText;            // Display text for the toolbar button
    QString iconPath;              // Optional icon path
    std::function<void()> callback;// Callback function to execute when toolbar button is triggered
};

}// namespace rbc