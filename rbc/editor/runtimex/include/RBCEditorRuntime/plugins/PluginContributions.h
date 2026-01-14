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

struct MenuContribution {
    QString menuPath;      // e.g., "File", "File/Open", "Edit/Preferences"
    QString actionText;    // Display text for the menu item
    QString actionId;      // Unique identifier for the action
    QString shortcut;     // Optional keyboard shortcut (e.g., "Ctrl+O")
    std::function<void()> callback; // Callback function to execute when menu item is triggered
};

struct ToolbarContribution {
    QString toolbarId;     // Unique identifier for the toolbar
    QString toolbarName;   // Display name for the toolbar
    QString actionId;      // Unique identifier for the action
    QString actionText;    // Display text for the toolbar button
    QString iconPath;      // Optional icon path
    std::function<void()> callback; // Callback function to execute when toolbar button is triggered
};

}// namespace rbc