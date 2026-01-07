#include "RBCEditorRuntime/plugins/ViewportPlugin.h"

namespace rbc {

ViewportViewModel::ViewportViewModel(ISceneService *sceneService, QObject *parent) : ViewModelBase(parent) {
    if (!sceneService) [[unlikely]] {
        qWarning() << "ViewportViewModel: sceneService not valid";
        return;
    }

    // connect to signals
}

}// namespace rbc