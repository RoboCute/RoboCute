#pragma once
#include <QString>

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

}// namespace rbc