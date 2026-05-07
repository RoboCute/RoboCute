#pragma once
#include <QDialog>

namespace rbc {

/**
 * Preview Window - For detailed result viewing
 *
 * Supported preview types:
 * - Image: zoomable, draggable
 * - Text: syntax-highlighted text viewer
 * - Scene: 3D scene preview (future support)
 * - Animation: animation sequence player (future support)
 */
class PreviewWindow : public QDialog {
    Q_OBJECT
public:
    explicit PreviewWindow(QWidget *parent = nullptr);
    ~PreviewWindow() override = default;
    // void setResultService(IResultService* resultService);
    // void setResultId(const QString& resultId);
    // QString resultId() const { return resultId; }
};

}// namespace rbc