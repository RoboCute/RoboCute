#pragma once

#include <QWidget>
#include "RBCEditorRuntime/components/RHIWindow.h"
#include "RBCEditorRuntime/runtime/Renderer.h"

namespace rbc {

// 自定义容器类，用于处理鼠标事件和拖动
class ViewportContainerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ViewportContainerWidget(RhiWindow *rhiWindow, QWidget *parent = nullptr);
    
    void setDragStartPos(const QPoint &pos) { m_dragStartPos = pos; }
    QPoint dragStartPos() const { return m_dragStartPos; }
    void resetDragStartPos() { m_dragStartPos = QPoint(); }
    
    void setEditorContext(class EditorContext *context) { m_editorContext = context; }
    class EditorContext *editorContext() const { return m_editorContext; }
    
    QWidget *innerContainer() const { return m_innerContainer; }

signals:
    void dragRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    RhiWindow *m_rhiWindow = nullptr;
    QWidget *m_innerContainer = nullptr;
    class EditorContext *m_editorContext = nullptr;
    QPoint m_dragStartPos;
};

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
    void setEditorContext(class EditorContext *context);

protected:
    // Event forwarding
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onDragRequested();

private:
    void setupUi();
    void startEntityDrag();

    IRenderer *m_renderer = nullptr;
    RhiWindow *m_rhiWindow = nullptr;
    ViewportContainerWidget *m_container = nullptr;
    class EditorContext *m_editorContext = nullptr;
};

}// namespace rbc
