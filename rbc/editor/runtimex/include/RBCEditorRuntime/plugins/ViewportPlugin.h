#pragma once
#include <rbc_config.h>
#include <QObject>
#include "RBCEditorRuntime/services/SceneService.h"
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"

namespace rbc {

/**
 * ViewportViewModel Viewport状态视图
 * - Camera State
 * - Control Command State
 * - Show/Hide State
 * - RenderConfig
 */
class RBC_EDITOR_RUNTIME_API ViewportViewModel : public ViewModelBase {
    Q_OBJECT

public:
    explicit ViewportViewModel(ISceneService *scenService);
};

}// namespace rbc