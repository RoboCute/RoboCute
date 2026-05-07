#pragma once
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>

namespace rbc {

class PreviewThumbnailWidget : public QWidget {
    Q_OBJECT

public:
    explicit PreviewThumbnailWidget(QWidget *parent = nullptr);
    // show thumbnail
    void showThumbnail(const QImage &image);
    // show text preview
    void showTextPreview(const QString &text, int maxLines = 4);
    void showLoading();
    void showPlaceholder(const QString &message = "Connect to preview");
    void showError(const QString &error);
    void setResultTypeIcon(const QString &iconPath);

signals:
    void doubleClicked();
    void dragStarted();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUi();

private:
    QLabel *_thumbnail_label;
    QLabel *_type_icon_label;
    QLabel *_status_label;
    QPoint _drag_start_pos;
    bool _is_dragging;
};

}// namespace rbc