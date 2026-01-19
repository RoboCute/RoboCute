#pragma once

#include <QSpinBox>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QPaintEvent>

namespace rbc {

/**
 * EntityIdSpinBox - 支持实体拖放的 SpinBox
 * 
 * 用于 EntityInputNode 的 entity_id 输入，支持从 SceneHierarchy 或 Viewport 拖放实体
 */
class EntityIdSpinBox : public QSpinBox {
    Q_OBJECT

public:
    explicit EntityIdSpinBox(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_isDragOver;
};

}// namespace rbc
