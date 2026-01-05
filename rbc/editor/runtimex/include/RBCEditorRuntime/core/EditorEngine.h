#pragma once
#include <rbc_config.h>
#include "RBCEditorRuntime/core/IRenderer.h"

namespace rbc {

/**
 * EditorEngine
 * ===================================
 */
struct RBC_EDITOR_RUNTIME_API EditorEngine {
public:
    static EditorEngine &instance();

    void init(int argc, char **argv);
    void shutdown();
};

}// namespace rbc