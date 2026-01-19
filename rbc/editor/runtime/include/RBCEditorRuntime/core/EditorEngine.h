#pragma once
#include <rbc_config.h>
#include <luisa/vstl/common.h>
#include "RBCEditorRuntime/infra/render/app_base.h"

namespace rbc {

/**
 * EditorEngine
 * ===================================
 */
struct RBC_EDITOR_RUNTIME_API EditorEngine {
public:
    static EditorEngine &instance();

public:
    void init(int argc, char **argv);
    void shutdown();

private:
    EditorEngine() = default;
    ~EditorEngine() { shutdown(); }

    bool m_isInitialized = false;
    luisa::string m_programPath;
    luisa::string m_backendName;
    QRhi::Implementation m_graphicsApi = QRhi::D3D12;
};

}// namespace rbc