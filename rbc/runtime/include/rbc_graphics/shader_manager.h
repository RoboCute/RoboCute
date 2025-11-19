#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/dsl/func.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/shader.h>
#include <luisa/ast/function.h>
#include <luisa/vstl/common.h>
#include <luisa/vstl/functional.h>
#include <luisa/vstl/spin_mutex.h>
#include <luisa/core/fiber.h>
namespace rbc
{
using namespace luisa;
using namespace luisa::compute;
template <typename T>
struct is_lc_shader {
    static constexpr bool value = false;
};
RBC_RUNTIME_API void shader_notfound_log(string_view name);
template <size_t dim, typename... Args>
struct is_lc_shader<Shader<dim, Args...>> {
    static constexpr bool value = true;
    static Shader<dim, Args...> load_shader(Device& device, string_view name)
    {
        auto shader = device.load_shader<dim, Args...>(name);
        if (!shader) [[unlikely]]
        {
            shader_notfound_log(name);
        }
        return shader;
    }
    static auto arg_types()
    {
        return luisa::compute::detail::shader_argument_types<Args...>();
    }
};
struct RBC_RUNTIME_API ShaderManager {
    using ReloadFunc = vstd::function<ShaderBase(Device& device, string_view name)>;
    struct ShaderVariant {
        ShaderBase shader;
        vstd::vector<Type const*> arg_types;
        ReloadFunc reload_func{ nullptr };
        luisa::spin_mutex local_mtx;
        luisa::fiber::event _evt;
        bool support_preload{ true };
        ShaderVariant()
            : _evt(luisa::fiber::event::Mode::Auto)
        {
        }
        ShaderVariant(
            ShaderBase&& shader,
            vstd::vector<Type const*>&& arg_types,
            ReloadFunc&& reload_func = {},
            bool support_preload = true
        )
            : shader(std::move(shader))
            , arg_types(std::move(arg_types))
            , reload_func(std::move(reload_func))
            , _evt(luisa::fiber::event::Mode::Auto)
            , support_preload(support_preload)
        {
        }
    };

    // [SINGLETON]
    static ShaderManager* instance();
    static void create_instance(Device& device, luisa::filesystem::path const& shader_path, bool use_lmdb);
    static void destroy_instance();

private:
    Device& _device;
    vstd::spin_mutex _mtx;
    luisa::filesystem::path _shader_path;
    std::atomic_uint64_t _all_shader_count{};
    std::atomic_uint64_t _finished_shaders{};
    mutable bool _use_lmdb{ false };
    vstd::HashMap<string, ShaderVariant> _shaders;
    void _empty_path_error();
    void _captured_not_empty_error(string_view name);
    ShaderBase const* _load_shader(
        luisa::filesystem::path const& shader_path,
        luisa::variant<
            luisa::span<Type const* const>,
            luisa::span<Variable const>>
            args,
        vstd::FuncRef<ShaderBase(string_view shader_path)>&& create_func,
        ReloadFunc reload_func,
        bool support_preload
    );
    template <size_t N, typename... Args>
    [[nodiscard]] auto _compile(
        const Kernel<N, Args...>& kernel,
        const ShaderOption& option = {},
        bool support_preload = true
    ) noexcept
    {
        // return _create<Shader<N, Args...>>(kernel.function()->function(), option);
        if (option.name.empty()) [[unlikely]]
        {
            _empty_path_error();
        }
        auto func = kernel.function().get();
        if (!func->bound_arguments().empty())
        {
            _captured_not_empty_error(option.name);
        }
        auto cb = [&](string_view shader_path) -> ShaderBase {
            auto new_option = option;
            new_option.name = shader_path;
            return _device.compile(kernel, new_option);
        };
        return static_cast<Shader<N, Args...> const*>(
            _load_shader(
                option.name,
                kernel.function()->arguments(),
                cb,
                nullptr,
                support_preload
            )
        );
    }
    luisa::string_view _path_to_key(luisa::filesystem::path const& path, luisa::string& can_path_str, luisa::string& buffer) const;

public:
    ShaderManager(Device& device, luisa::filesystem::path const& shader_path);
    ~ShaderManager();
    void get_preload_progress(uint64_t& all_shader_count, uint64_t& finished_shader_count);
    void set_lmdb_mode() const { _use_lmdb = true; }
    [[nodiscard]] ShaderBase unload_shader(luisa::filesystem::path const& shader_path);

    template <typename T>
    requires(
        std::is_pointer_v<T> &&
        is_lc_shader<std::remove_cvref_t<std::remove_pointer_t<T>>>::value
    )
    void load(luisa::filesystem::path const& shader_path, T& shader_ptr, bool support_preload = true)
    {
        using PureT = std::remove_cvref_t<std::remove_pointer_t<T>>;
        using TT = is_lc_shader<PureT>;
        auto c1 = [&](string_view shader_path) -> ShaderBase {
            return TT::load_shader(_device, shader_path);
        };
        auto c2 = [](Device& device, string_view name) -> ShaderBase {
            return TT::load_shader(device, name);
        };
        shader_ptr = static_cast<T>(_load_shader(
            shader_path,
            TT::arg_types(),
            c1, c2,
            support_preload
        ));
    }
    template<typename T>
        requires(
            std::is_pointer_v<T> &&
            is_lc_shader<std::remove_cvref_t<std::remove_pointer_t<T>>>::value)
    void async_load(luisa::fiber::counter &counter, luisa::filesystem::path const &shader_path, T &shader_ptr, bool support_preload = true) {
        counter.add(1);
        luisa::fiber::schedule([this, counter, &shader_ptr, shader_path, support_preload]() {
            this->template load<T>(shader_path, shader_ptr, support_preload);
            counter.done();
        });
    }
    ShaderBase const* load_typeless(luisa::filesystem::path const& shader_path, luisa::span<Type const* const> types, bool support_preload = true)
    {
        auto c1 = [&](string_view shader_path) -> ShaderBase {
            auto uniform_size = ShaderDispatchCmdEncoder::compute_uniform_size(types);
            return ShaderBase{
                _device.impl(),
                _device.impl()->load_shader(shader_path, types),
                uniform_size
            };
        };
        luisa::vector<Type const*> types_vec;
        vstd::push_back_all(types_vec, types);
        auto c2 = [types_vec = std::move(types_vec)](Device& device, string_view name) -> ShaderBase {
            auto uniform_size = ShaderDispatchCmdEncoder::compute_uniform_size(types_vec);
            return ShaderBase{
                device.impl(),
                device.impl()->load_shader(name, types_vec),
                uniform_size
            };
        };

        return _load_shader(
            shader_path,
            types,
            c1, c2,
            support_preload
        );
    }
    template <typename... Args>
    RasterShader<Args...> load_raster_shader(luisa::string_view shader_name)
    {
        if (_use_lmdb)
        {
            return _device.template load_raster_shader<Args...>(shader_name);
        }
        else
        {
            return _device.template load_raster_shader<Args...>(luisa::to_string(_shader_path / shader_name));
        }
    }
    template <size_t N, typename Func>
    requires(std::negation_v<luisa::compute::detail::is_dsl_kernel<std::remove_cvref_t<Func>>> && N >= 1 && N <= 3)
    [[nodiscard]] auto compile(
        Func&& f,
        const ShaderOption& option = {}
    ) noexcept
    {
        if constexpr (N == 1u)
        {
            return _compile(Kernel1D{ std::forward<Func>(f) }, option);
        }
        else if constexpr (N == 2u)
        {
            return _compile(Kernel2D{ std::forward<Func>(f) }, option);
        }
        else
        {
            return _compile(Kernel3D{ std::forward<Func>(f) }, option);
        }
    }
    fiber::counter reload_shaders(Device& device);
    vstd::unique_ptr<vstd::IRange<string_view>> loaded_shaders() const;
    vstd::vector<char> loaded_shaders_json(luisa::filesystem::path const& shader_path) const;
    void preload_shaders(
        Device& device,
        luisa::filesystem::path const& shader_path,
        vstd::string_view json
    );

    void preload_shaders(
        Device& device,
        luisa::filesystem::path const& shader_path,
        vstd::vector<std::pair<vstd::string, vstd::vector<Type const*>>>&& registed_shaders
    );
};
} // namespace rbc