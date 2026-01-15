#include "RBCEditorRuntime/plugins/ViewportPlugin.h"

namespace rbc {

ViewportViewModel::ViewportViewModel(ISceneService *sceneService, QObject *parent) : ViewModelBase(parent), sceneService_(sceneService) {
    if (!sceneService_) {
        qWarning() << "ViewportViewModel: sceneService is null";
        return;
    }

    // connect to service signal
}

ViewportViewModel::~ViewportViewModel() {
    if (sceneService_) {
    }
    sceneService_ = nullptr;
}

}// namespace rbc