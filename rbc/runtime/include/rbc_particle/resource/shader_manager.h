#pragma once

#include "rbc_particle/resource/shader.h"

namespace rbc::ps {

class ShaderManager {
    luisa::vector<luisa::shared_ptr<LCShader>> m_shaders;

public:
    ShaderManager() = default;
    ~ShaderManager() = default;
    // delete copy
    ShaderManager(const ShaderManager &) = delete;
    ShaderManager &operator=(const ShaderManager &) = delete;
    // default move
    ShaderManager(ShaderManager &&) = default;
    ShaderManager &operator=(ShaderManager &&) = default;

    eastl::shared_ptr<LCShader> emplace_shader(LCShader &&shader) noexcept;
};

}// namespace rbc::ps