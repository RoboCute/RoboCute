#include "RBCEditorRuntime/components/ResultPanel.h"
#include "RBCEditorRuntime/runtime/SceneSync.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QDebug>

namespace rbc {

ResultPanel::ResultPanel(QWidget *parent)
    : QWidget(parent) {
    setupUi();
}

void ResultPanel::setupUi() {
    auto *layout = new QVBoxLayout(this);

    // Title
    titleLabel_ = new QLabel("Results", this);
    QFont titleFont = titleLabel_->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    layout->addWidget(titleLabel_);

    // Status label
    statusLabel_ = new QLabel("No animations", this);
    statusLabel_->setStyleSheet("color: gray;");
    layout->addWidget(statusLabel_);

    // Animation list
    animationList_ = new QListWidget(this);
    animationList_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(animationList_);

    // Connect signals
    connect(animationList_, &QListWidget::itemClicked,
            this, &ResultPanel::onAnimationItemClicked);

    setLayout(layout);
}

void ResultPanel::updateFromSync(const SceneSync *sync) {
    if (!sync) return;

    const auto &animations = sync->animations();

    // Clear existing items
    animationList_->clear();

    if (animations.empty()) {
        statusLabel_->setText("No animations");
        statusLabel_->setStyleSheet("color: gray;");
        return;
    }

    // Add animation items
    for (const auto &anim : animations) {
        QString itemText = QString("%1 (%2 frames, %.1f fps)")
                               .arg(QString::fromStdString(anim.name.c_str()))
                               .arg(anim.total_frames)
                               .arg(anim.fps);

        auto *item = new QListWidgetItem(itemText, animationList_);
        item->setData(Qt::UserRole, QString::fromStdString(anim.name.c_str()));
    }

    statusLabel_->setText(QString("%1 animation(s)").arg(animations.size()));
    statusLabel_->setStyleSheet("color: green;");
}

void ResultPanel::clear() {
    animationList_->clear();
    statusLabel_->setText("No animations");
    statusLabel_->setStyleSheet("color: gray;");
}

void ResultPanel::onAnimationItemClicked(QListWidgetItem *item) {
    if (!item) return;

    QString animName = item->data(Qt::UserRole).toString();
    emit animationSelected(animName);
}

}// namespace rbc
