#include "RBCEditorRuntime/core/EditorEngine.h"

namespace rbc {

EditorEngine &EditorEngine::instance() {
    static EditorEngine engine;
    return engine;
}

}// namespace rbc