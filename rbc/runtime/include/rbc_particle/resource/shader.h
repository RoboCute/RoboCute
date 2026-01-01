#pragma once

#include <luisa/luisa-compute.h>

namespace rbc::ps {

struct ShaderArgument {
    enum struct ArgumentTag {
        BUFFER,
        UNKNOWN
    };
    ArgumentTag tag;
    luisa::string name;
};

class LCShader : public luisa::enable_shared_from_this<LCShader> {
    using ShaderCreationInfo = luisa::compute::ShaderCreationInfo;
    using Device = luisa::compute::Device;
    using Function = luisa::compute::Function;

private:
    bool m_is_init;
    bool m_is_compiled;
    luisa::unique_ptr<ShaderCreationInfo> mp_creation_info;
    luisa::string m_name = "dummy_shader";
    size_t m_uniform_size = 0;
    size_t m_arg_count = 0;

public:
    LCShader(luisa::string name)
        : m_is_init(false), m_is_compiled(false), m_name(name) {}
    ~LCShader() = default;
    // delete copy
    LCShader(const LCShader &) = delete;
    LCShader &operator=(const LCShader &) = delete;
    // default move
    LCShader(LCShader &&) = default;
    LCShader &operator=(LCShader &&) = default;
    ShaderCreationInfo &creation_info() noexcept {
        return *mp_creation_info;
    }
    uint64_t compiled_handle() const noexcept {
        LUISA_ASSERT(m_is_compiled && mp_creation_info->valid(), "shader not compiled");

        return mp_creation_info->handle;
    }
    [[nodiscard]] size_t uniform_size() const { return m_uniform_size; }
    [[nodiscard]] size_t arg_count() noexcept { return m_arg_count; };
    [[nodiscard]] luisa::string name() const noexcept { return m_name; }
    void compile(Device &device, Function &&function) noexcept;
};

class ShaderCall {
    using ComputeDispatchCmdEncoder = luisa::compute::ComputeDispatchCmdEncoder;
    using ShaderDispatchCommand = luisa::compute::ShaderDispatchCommand;
    using uint3 = luisa::uint3;
private:
    eastl::shared_ptr<LCShader> p_shader;

    bool m_is_sync = false; // check if should sync after dispatch
    bool m_is_ready = false;// create and encoded

public:
    eastl::vector<ShaderArgument> arguments;
    ComputeDispatchCmdEncoder m_cmd_encoder;

    explicit ShaderCall(eastl::shared_ptr<LCShader> p_shad) noexcept
        : p_shader{p_shad}, m_cmd_encoder{
                                p_shad->compiled_handle(),
                                p_shad->arg_count(),
                                p_shad->uniform_size()} {
        LUISA_INFO("shader call created for {}", p_shad->compiled_handle());
        LUISA_INFO("arg count: {}", p_shad->arg_count());
        LUISA_INFO("uniform size: {}", p_shad->uniform_size());
    }
    ~ShaderCall() = default;
    // delete copy
    ShaderCall(const ShaderCall &) = delete;
    ShaderCall &operator=(const ShaderCall &) = delete;
    // default move
    ShaderCall(ShaderCall &&) = default;
    ShaderCall &operator=(ShaderCall &&) = default;

    [[nodiscard]] luisa::unique_ptr<ShaderDispatchCommand> dispatch(uint3 dispatch_size) && noexcept;// construct shader call

    // set/get sync
    void set_sync(bool sync) noexcept { m_is_sync = sync; }
    [[nodiscard]] bool is_sync() const noexcept { return m_is_sync; }
    [[nodiscard]] bool is_ready() const noexcept { return m_is_ready; }
    void set_ready(bool ready) noexcept { m_is_ready = ready; }
    [[nodiscard]] luisa::string shader_name() const noexcept { return p_shader->name(); }

    ShaderArgument &emplace_argument(ShaderArgument &&arg) noexcept {
        arguments.emplace_back(std::move(arg));
        return arguments.back();
    }

    void create_encoder() noexcept {
        m_cmd_encoder = ComputeDispatchCmdEncoder{
            p_shader->compiled_handle(),
            p_shader->arg_count(),
            p_shader->uniform_size()};
    }
    void encode_buffer(uint64_t handle, size_t offset_bytes, size_t size_bytes) {
        m_cmd_encoder.encode_buffer(handle, offset_bytes, size_bytes);
    }
};

}// namespace rbc::ps