#pragma once

#include <QSpinBox>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QPaintEvent>

namespace rbc {

/**
 * EntityIdSpinBox - SpinBox supporting entity drag-and-drop
 *
 * Used for EntityInputNode's entity_id input, supports dragging and dropping entities from SceneHierarchy or Viewport
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
    bool _is_drag_over;
};

}// namespace rbc
