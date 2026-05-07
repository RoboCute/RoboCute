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

    bool _is_initialized = false;
    luisa::string _program_path;
    luisa::string _backend_name;
    QRhi::Implementation _graphics_api = QRhi::D3D12;
};

}// namespace rbc