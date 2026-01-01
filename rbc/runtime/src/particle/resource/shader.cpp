/**
 * @file shader.cpp
 * @brief The Shader Implementation
 * @author sailing-innocent
 * @date 2024-05-21
 */

#include "rbc_particle/resource/shader.h"

namespace rbc::ps {

luisa::unique_ptr<ShaderDispatchCommand> ShaderCall::dispatch(uint3 dispatch_size) && noexcept {
    m_cmd_encoder.set_dispatch_size(dispatch_size);
    return std::move(m_cmd_encoder).build();
}

void LCShader::compile(Device &device, Function &&function) noexcept {
    mp_creation_info = eastl::make_unique<ShaderCreationInfo>(
        std::move(device.impl()->create_shader({.name = m_name}, function)));
    m_is_compiled = true;
    LUISA_INFO("shader compiled: {} handle {}", m_name, mp_creation_info->handle);
    m_arg_count = function.arguments().size();
    m_uniform_size = ShaderDispatchCmdEncoder::compute_uniform_size(function.unbound_arguments());
}

}// namespace rbc::ps