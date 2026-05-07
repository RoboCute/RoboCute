#include "RBCEditorRuntime/plugins/ViewportPlugin.h"
#include <QDebug>
#include <luisa/runtime/context.h>

namespace rbc {

// ============================================================================
// ViewportViewModel Implementation
// ============================================================================

ViewportViewModel::ViewportViewModel(
    const ViewportConfig &config,
    ISceneService *sceneService,
    QObject *parent)
    : ViewModelBase(parent)
    , _config(config)
    , _scene_service(sceneService) {
    
    if (!_scene_service) {
        qWarning() << "ViewportViewModel: sceneService is null for viewport:" << config.viewportId;
    }
    
    // Initialize state from config
    _gizmos_enabled = config.enableGizmos;
    _grid_enabled = config.enableGrid;
}

ViewportViewModel::~ViewportViewModel() {
    _scene_service = nullptr;
}

void ViewportViewModel::setGizmosEnabled(bool enabled) {
    if (_gizmos_enabled != enabled) {
        _gizmos_enabled = enabled;
        emit gizmosEnabledChanged();
        qDebug() << "ViewportViewModel:" << _config.viewportId << "gizmos enabled:" << enabled;
    }
}

void ViewportViewModel::setGridEnabled(bool enabled) {
    if (_grid_enabled != enabled) {
        _grid_enabled = enabled;
        emit gridEnabledChanged();
        qDebug() << "ViewportViewModel:" << _config.viewportId << "grid enabled:" << enabled;
    }
}

void ViewportViewModel::setCameraMode(const QString &mode) {
    if (_camera_mode != mode) {
        _camera_mode = mode;
        emit cameraModeChanged();
        qDebug() << "ViewportViewModel:" << _config.viewportId << "camera mode:" << mode;
    }
}

void ViewportViewModel::focusOnSelection() {
    qDebug() << "ViewportViewModel:" << _config.viewportId << "focusOnSelection";
    // TODO: Implement focus on selected object
    if (_scene_service) {
        // Get currently selected entity, compute its bounding box, adjust camera position
    }
}

void ViewportViewModel::resetCamera() {
    qDebug() << "ViewportViewModel:" << _config.viewportId << "resetCamera";
    // TODO: Reset camera to default position
    setCameraMode("Perspective");
}

void ViewportViewModel::setCameraView(const QString &preset) {
    qDebug() << "ViewportViewModel:" << _config.viewportId << "setCameraView:" << preset;
    
    // Supported preset views: Top, Bottom, Front, Back, Left, Right, Perspective
    if (preset == "Top" || preset == "Bottom" || 
        preset == "Front" || preset == "Back" ||
        preset == "Left" || preset == "Right") {
        setCameraMode(preset);
        // TODO: Actually adjust camera parameters
    } else if (preset == "Perspective") {
        setCameraMode("Perspective");
        // TODO: Switch to perspective view
    } else {
        qWarning() << "ViewportViewModel: Unknown camera preset:" << preset;
    }
}

} // namespace rbc
