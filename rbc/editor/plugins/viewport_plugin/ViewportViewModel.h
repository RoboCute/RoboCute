#pragma once
#include <QObject>
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/services/ISceneService.h"

namespace rbc {

class ViewportViewModel : public ViewModelBase {
    Q_OBJECT

    Q_PROPERTY(int selectedEntityId READ selectedEntityId NOTIFY selectionChanged)
    Q_PROPERTY(QString cameraMode READ cameraMode WRITE setCameraMode NOTIFY cameraModeChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)
    Q_PROPERTY(bool showGizmo READ showGizmo WRITE setShowGizmo NOTIFY showGizmoChanged)
    Q_PROPERTY(QVector3D cameraPosition READ cameraPosition NOTIFY cameraPositionChanged)

public:
    explicit ViewportViewModel(ISceneService *sceneService, IRenderer *renderer, QObject *parent = nullptr);
};

}// namespace rbc