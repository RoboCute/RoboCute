#include "RBCEditorRuntime/core/EditorEngine.h"

namespace rbc {

EditorEngine &EditorEngine::instance() {
    static EditorEngine engine;
    return engine;
}

void EditorEngine::init(int argc, char **argv) {
}

void EditorEngine::shutdown() {}

}// namespace rbc