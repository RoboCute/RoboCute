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
    , config_(config)
    , sceneService_(sceneService) {
    
    if (!sceneService_) {
        qWarning() << "ViewportViewModel: sceneService is null for viewport:" << config.viewportId;
    }
    
    // 从配置中初始化状态
    gizmosEnabled_ = config.enableGizmos;
    gridEnabled_ = config.enableGrid;
}

ViewportViewModel::~ViewportViewModel() {
    sceneService_ = nullptr;
}

void ViewportViewModel::setGizmosEnabled(bool enabled) {
    if (gizmosEnabled_ != enabled) {
        gizmosEnabled_ = enabled;
        emit gizmosEnabledChanged();
        qDebug() << "ViewportViewModel:" << config_.viewportId << "gizmos enabled:" << enabled;
    }
}

void ViewportViewModel::setGridEnabled(bool enabled) {
    if (gridEnabled_ != enabled) {
        gridEnabled_ = enabled;
        emit gridEnabledChanged();
        qDebug() << "ViewportViewModel:" << config_.viewportId << "grid enabled:" << enabled;
    }
}

void ViewportViewModel::setCameraMode(const QString &mode) {
    if (cameraMode_ != mode) {
        cameraMode_ = mode;
        emit cameraModeChanged();
        qDebug() << "ViewportViewModel:" << config_.viewportId << "camera mode:" << mode;
    }
}

void ViewportViewModel::focusOnSelection() {
    qDebug() << "ViewportViewModel:" << config_.viewportId << "focusOnSelection";
    // TODO: 实现聚焦到选中对象
    if (sceneService_) {
        // 获取当前选中的实体，计算其边界盒，调整相机位置
    }
}

void ViewportViewModel::resetCamera() {
    qDebug() << "ViewportViewModel:" << config_.viewportId << "resetCamera";
    // TODO: 重置相机到默认位置
    setCameraMode("Perspective");
}

void ViewportViewModel::setCameraView(const QString &preset) {
    qDebug() << "ViewportViewModel:" << config_.viewportId << "setCameraView:" << preset;
    
    // 支持的预设视图：Top, Bottom, Front, Back, Left, Right, Perspective
    if (preset == "Top" || preset == "Bottom" || 
        preset == "Front" || preset == "Back" ||
        preset == "Left" || preset == "Right") {
        setCameraMode(preset);
        // TODO: 实际调整相机参数
    } else if (preset == "Perspective") {
        setCameraMode("Perspective");
        // TODO: 切换到透视视图
    } else {
        qWarning() << "ViewportViewModel: Unknown camera preset:" << preset;
    }
}

} // namespace rbc
