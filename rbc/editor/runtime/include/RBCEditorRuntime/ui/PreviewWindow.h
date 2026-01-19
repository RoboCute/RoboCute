#pragma once
#include <QDialog>

namespace rbc {

/**
 * 预览窗口 - 用于详细查看结果
 * 
 * 支持的预览类型：
 * - 图片：可缩放、可拖拽查看
 * - 文本：带语法高亮的文本查看器
 * - 场景：3D 场景预览（未来支持）
 * - 动画：动画序列播放器（未来支持）
 */
class PreviewWindow : public QDialog {
    Q_OBJECT
public:
    explicit PreviewWindow(QWidget *parent = nullptr);
    ~PreviewWindow() override;
    // void setResultService(IResultService* resultService);
    // void setResultId(const QString& resultId);
    // QString resultId() const { return resultId; }
};

}// namespace rbc