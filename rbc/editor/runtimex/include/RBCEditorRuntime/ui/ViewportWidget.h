#pragma once
#include <rbc_config.h>
#include <QWidget>
#include "RBCEditorRuntime/ui/RHIWindow.h"
#include "RBCEditorRuntime/infra/render/app_base.h"

namespace rbc {

class ViewportWidget;

// ============================================================================
// ViewportContainerWidget - Mouse Event Handler and Drag Support
// ============================================================================

/**
 * @brief ViewportContainerWidget - 处理鼠标事件和拖动的自定义容器
 * 
 * 这个 widget 包装了 RhiWindow 的容器，负责：
 * 1. 处理拖动起始位置检测
 * 2. 发出拖动请求信号
 * 3. 转发事件给 RhiWindow
 */
class RBC_EDITOR_RUNTIME_API ViewportContainerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ViewportContainerWidget(RhiWindow *rhiWindow, QWidget *parent = nullptr);

    void setDragStartPos(const QPoint &pos) { m_dragStartPos = pos; }
    QPoint dragStartPos() const { return m_dragStartPos; }
    void resetDragStartPos() { m_dragStartPos = QPoint(); }

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
    QPoint m_dragStartPos;
};

// ============================================================================
// ViewportWidget - Main Viewport Component
// ============================================================================

/**
 * @brief ViewportWidget - 视口组件
 * 
 * Viewport组件是渲染窗口的QWidget封装，负责：
 * 1. 创建和管理 RhiWindow
 * 2. 转发窗口消息给渲染器
 * 3. 支持实体拖放操作
 * 
 * 使用方式：
 * @code
 * IRenderer* renderer = createRenderer();
 * ViewportWidget* viewport = new ViewportWidget(renderer, parent);
 * viewport 会管理 RhiWindow 的生命周期
 * @endcode
 */
class RBC_EDITOR_RUNTIME_API ViewportWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param renderer 渲染器接口，ViewportWidget 不拥有其生命周期
     * @param graphicsApi RHI 图形 API 类型
     * @param parent 父 widget
     */
    explicit ViewportWidget(IRenderer *renderer,
                            QRhi::Implementation graphicsApi = QRhi::D3D12,
                            QWidget *parent = nullptr);
    ~ViewportWidget() override;

    [[nodiscard]] RhiWindow *getRhiWindow() const { return m_rhiWindow; }

    /**
     * @brief 获取图形 API 名称
     */
    [[nodiscard]] QString graphicsApiName() const;

signals:
    /**
     * @brief 拖动请求信号
     * 
     * 当用户在视口中拖动选中的实体时发出
     */
    void entityDragRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onDragRequested();

private:
    void setupUi();

    IRenderer *m_renderer = nullptr;
    RhiWindow *m_rhiWindow = nullptr;
    ViewportContainerWidget *m_container = nullptr;
    QRhi::Implementation m_graphicsApi = QRhi::D3D12;
};

}// namespace rbc
