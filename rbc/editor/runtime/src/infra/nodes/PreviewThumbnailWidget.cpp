#include "RBCEditorRuntime/infra/nodes/PreviewThumbnailWidget.h"

namespace rbc {

PreviewThumbnailWidget::PreviewThumbnailWidget(QWidget *parent) : QWidget(parent) {}
void PreviewThumbnailWidget::showThumbnail(const QImage &image) {}
void PreviewThumbnailWidget::showTextPreview(const QString &text, int maxLines) {}
void PreviewThumbnailWidget::showLoading() {}
void PreviewThumbnailWidget::showPlaceholder(const QString &message) {}
void PreviewThumbnailWidget::showError(const QString &error) {}
void PreviewThumbnailWidget::setResultTypeIcon(const QString &iconPath) {}

void PreviewThumbnailWidget::mouseDoubleClickEvent(QMouseEvent *event) {}
void PreviewThumbnailWidget::mousePressEvent(QMouseEvent *event) {}
void PreviewThumbnailWidget::mouseMoveEvent(QMouseEvent *event) {}
void PreviewThumbnailWidget::paintEvent(QPaintEvent *event) {}

void PreviewThumbnailWidget::setupUi() {}

}// namespace rbc