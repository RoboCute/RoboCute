#include "LayoutPlugin.h"
#include <QDebug>
#include <QJsonObject>

namespace rbc {

LayoutViewModel::LayoutViewModel(LayoutService *layoutService, QObject *parent)
    : ViewModelBase(parent), _layout_service(layoutService) {

    if (_layout_service) {
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
    return _layout_service ? _layout_service->currentLayoutId() : QString();
}

QStringList LayoutViewModel::availableLayouts() const {
    if (!_layout_service) {
        return {};
    }
    qDebug() << "Available Layouts: " << _layout_service->availableLayouts();
    return _layout_service->availableLayouts();
}

QVariantList LayoutViewModel::viewStates() const {
    return _view_states;
}

void LayoutViewModel::switchLayout(const QString &layoutId) {
    if (!_layout_service) {
        qWarning() << "LayoutViewModel::switchLayout: layoutService is null";
        return;
    }

    if (_layout_service->switchToLayout(layoutId)) {
        qDebug() << "LayoutViewModel: Switched to layout:" << layoutId;
        emit currentLayoutIdChanged();
        updateViewStates();
        emit viewStatesChanged();
    } else {
        qWarning() << "LayoutViewModel: Failed to switch to layout:" << layoutId;
    }
}

void LayoutViewModel::setViewVisible(const QString &viewId, bool visible) {
    if (!_layout_service) {
        qWarning() << "LayoutViewModel::setViewVisible: layoutService is null";
        return;
    }

    _layout_service->setViewVisible(viewId, visible);
    updateViewStates();
    emit viewStatesChanged();

    qDebug() << "LayoutViewModel: Set view" << viewId << "visible:" << visible;
}

bool LayoutViewModel::isViewVisible(const QString &viewId) const {
    if (!_layout_service) {
        return false;
    }
    return _layout_service->isViewVisible(viewId);
}

QString LayoutViewModel::getLayoutName(const QString &layoutId) const {
    if (!_layout_service) {
        return layoutId;
    }

    QJsonObject metadata = _layout_service->getLayoutMetadata(layoutId);
    if (metadata.contains("layout_name")) {
        return metadata["layout_name"].toString();
    }
    return layoutId;
}

QString LayoutViewModel::getLayoutDescription(const QString &layoutId) const {
    if (!_layout_service) {
        return QString();
    }
    QJsonObject metadata = _layout_service->getLayoutMetadata(layoutId);
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
    _view_states.clear();
    if (!_layout_service) {
        return;
    }
}

}// namespace rbc
