#pragma once

#include <QObject>
#include <QTimer>
#include <luisa/vstl/common.h>
#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/runtime/EventBus.h"

namespace rbc {
class EditorContext;
class VisApp;
}// namespace rbc

/**
 * ViewportSelectionSync - 同步视口选择状态到UI层
 * 
 * 职责：
 * - 定期检查 VisApp 中的 dragged_object_ids (instance ids)
 * - 通过 EditorScene 将 instance ids 转换为 entity ids
 * - 通过事件总线发送 EntitySelected 事件
 */
class RBC_EDITOR_RUNTIME_API ViewportSelectionSync : public QObject {
    Q_OBJECT

public:
    explicit ViewportSelectionSync(rbc::EditorContext *context, QObject *parent = nullptr);
    ~ViewportSelectionSync() override;

    /**
     * 初始化同步器，启动定时器
     */
    void initialize();

    /**
     * 停止同步
     */
    void stop();

private slots:
    /**
     * 定时检查选择状态
     */
    void checkSelection();

private:
    rbc::EditorContext *context_;
    QTimer *checkTimer_;
    luisa::vector<uint> lastInstanceIds_; // 上次检查的 instance ids，用于检测变化
};

