#include "LayoutPlugin.h"
#include <QDebug>
#include <QJsonObject>

namespace rbc {

LayoutViewModel::LayoutViewModel(LayoutService *layoutService, QObject *parent)
    : ViewModelBase(parent), layoutService_(layoutService) {

    if (layoutService_) {
        // Connect to layout service signals when available
        // For now, we manually update when needed
    }

    updateViewStates();
    qDebug() << "LayoutViewModel created";
}

LayoutViewModel::~LayoutViewModel() {
    qDebug() << "LayoutViewModel destroyed";
}

QString LayoutViewModel::currentLayoutId() const {
    return layoutService_ ? layoutService_->currentLayoutId() : QString();
}

QStringList LayoutViewModel::availableLayouts() const {
    if (!layoutService_) {
        return {};
    }
    qDebug() << "Available Layouts: " << layoutService_->availableLayouts();
    return layoutService_->availableLayouts();
}

QVariantList LayoutViewModel::viewStates() const {
    return viewStates_;
}

void LayoutViewModel::switchLayout(const QString &layoutId) {
    if (!layoutService_) {
        qWarning() << "LayoutViewModel::switchLayout: layoutService is null";
        return;
    }

    if (layoutService_->switchToLayout(layoutId)) {
        qDebug() << "LayoutViewModel: Switched to layout:" << layoutId;
        emit currentLayoutIdChanged();
        updateViewStates();
        emit viewStatesChanged();
    } else {
        qWarning() << "LayoutViewModel: Failed to switch to layout:" << layoutId;
    }
}

void LayoutViewModel::setViewVisible(const QString &viewId, bool visible) {
    if (!layoutService_) {
        qWarning() << "LayoutViewModel::setViewVisible: layoutService is null";
        return;
    }

    layoutService_->setViewVisible(viewId, visible);
    updateViewStates();
    emit viewStatesChanged();

    qDebug() << "LayoutViewModel: Set view" << viewId << "visible:" << visible;
}

bool LayoutViewModel::isViewVisible(const QString &viewId) const {
    if (!layoutService_) {
        return false;
    }
    return layoutService_->isViewVisible(viewId);
}

QString LayoutViewModel::getLayoutName(const QString &layoutId) const {
    if (!layoutService_) {
        return layoutId;
    }

    QJsonObject metadata = layoutService_->getLayoutMetadata(layoutId);
    if (metadata.contains("layout_name")) {
        return metadata["layout_name"].toString();
    }
    return layoutId;
}

QString LayoutViewModel::getLayoutDescription(const QString &layoutId) const {
    if (!layoutService_) {
        return QString();
    }
    QJsonObject metadata = layoutService_->getLayoutMetadata(layoutId);
    if (metadata.contains("description")) {
        return metadata["description"].toString();
    }
    return QString("No Description");
}

void LayoutViewModel::onLayoutChanged() {
    emit currentLayoutIdChanged();
    updateViewStates();
    emit viewStatesChanged();
}

void LayoutViewModel::updateViewStates() {
    viewStates_.clear();
    if (!layoutService_) {
        return;
    }
}

}// namespace rbc
