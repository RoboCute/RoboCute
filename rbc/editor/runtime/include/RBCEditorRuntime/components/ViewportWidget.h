#pragma once

#include <QWidget>
#include "RBCEditorRuntime/components/RHIWindow.h"
#include "RBCEditorRuntime/runtime/Renderer.h"

namespace rbc {

/**
 * Viewport Widget
 * =======================================================
 * Viewport组件是渲染窗口的QWidget封装，负责转发窗口消息
*/
class ViewportWidget : public QWidget {
    Q_OBJECT
public:
    explicit ViewportWidget(IRenderer *renderer, QWidget *parent = nullptr);
    ~ViewportWidget() override;

    [[nodiscard]] RhiWindow *getRhiWindow() const { return m_rhiWindow; }
    
    // Set EditorContext for drag and drop support
    void setEditorContext(class EditorContext *context) { m_editorContext = context; }

protected:
    // Event forwarding
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUi();
    void startEntityDrag();

    IRenderer *m_renderer = nullptr;
    RhiWindow *m_rhiWindow = nullptr;
    QWidget *m_container = nullptr;
    class EditorContext *m_editorContext = nullptr;
    QPoint m_dragStartPos;
};

}// namespace rbc
