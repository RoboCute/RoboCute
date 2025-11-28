#include "RBCEditor/components/node_editor.h"
#include <QtWidgets/qboxlayout.h>
#include <QLabel>

namespace rbc {

void NodeEditor::SetUI() {
    auto *main_layout = new QHBoxLayout(this);
    auto *nodePlaceholder = new QLabel("NodeEditor (WIP)");
    nodePlaceholder->setAlignment(Qt::AlignCenter);
    nodePlaceholder->setObjectName("NodeEditor (WIP)");
    main_layout->addWidget(nodePlaceholder);
}

}// namespace rbc