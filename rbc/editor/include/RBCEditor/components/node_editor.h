#pragma once

#include <QWidget>

namespace rbc {

class NodeEditor : public QWidget {
    Q_OBJECT
public:
    explicit NodeEditor(QWidget *parent) : QWidget(parent) {
        SetUI();
    }

private:
    void SetUI();
};

}// namespace rbc