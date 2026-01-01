/**
 * @file shader_manger.cpp
 * @brief The Shader Manager
 * @author sailing-innocent
 * @date 2024-05-21
 */
#include "rbc_particle/resource/shader_manager.h"

namespace rbc::ps {

eastl::shared_ptr<LCShader> ShaderManager::emplace_shader(LCShader &&shader) noexcept {
    m_shaders.emplace_back(std::move(eastl::make_shared<LCShader>(std::move(shader))));
    return m_shaders.back();
}

}// namespace rbc::ps