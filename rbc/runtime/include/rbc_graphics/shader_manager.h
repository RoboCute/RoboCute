#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/dsl/func.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/shader.h>
#include <luisa/runtime/raster/raster_shader.h>
#include <luisa/ast/function.h>
#include <luisa/vstl/common.h>
#include <luisa/vstl/spin_mutex.h>
#include <luisa/core/fiber.h>
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
template<typename T>
struct is_lc_shader {
    static constexpr bool value = false;
};
RBC_RUNTIME_API void shader_notfound_log(string_view name);
template<size_t dim, typename... Args>
struct is_lc_shader<Shader<dim, Args...>> {
    static constexpr bool value = true;
    static Shader<dim, Args...> load_shader(Device &device, string_view name) {
        auto shader = device.load_shader<dim, Args...>(name);
        if (!shader) [[unlikely]] {
            shader_notfound_log(name);
        }
        return shader;
    }
    static auto arg_types() {
        return luisa::compute::detail::shader_argument_types<Args...>();
    }
};
template<typename... Args>
struct is_lc_shader<RasterShader<Args...>> {
    static constexpr bool value = true;
    static RasterShader<Args...> load_shader(Device &device, string_view name) {
        auto shader = device.load_raster_shader<Args...>(name);
        if (!shader) [[unlikely]] {
            shader_notfound_log(name);
        }
        return shader;
    }
    static auto arg_types() {
        return luisa::compute::detail::shader_argument_types<Args...>();
    }
};
using ShaderType = vstd::variant<ShaderBase, RasterShader<>>;
;
struct RBC_RUNTIME_API ShaderManager : vstd::IOperatorNewBase {
    using ReloadFunc = vstd::func_ptr_t<ShaderType(Device &device, string_view name, luisa::span<Type const* const> arg_types)>;
    struct ShaderVariant {
        ShaderType shader;
        vstd::vector<Type const *> arg_types;
        ReloadFunc reload_func{nullptr};
        luisa::spin_mutex local_mtx;
        luisa::fiber::event _evt;
        bool support_preload{true};
        ShaderVariant()
            : _evt(luisa::fiber::event::Mode::Auto) {
        }
        template<typename ShaderT>
            requires(ShaderType::IndexOf<std::remove_cvref_t<ShaderT>> < ShaderType::argSize)
        ShaderVariant(
            ShaderT &&shader,
            vstd::vector<Type const *> &&arg_types,
            ReloadFunc reload_func = {},
            bool support_preload = true)
            : shader(std::forward<ShaderT>(shader)), arg_types(std::move(arg_types)), reload_func(reload_func), _evt(luisa::fiber::event::Mode::Auto), support_preload(support_preload) {
        }
    };

    // [SINGLETON]
    static ShaderManager *instance();
    static void create_instance(Device &device, luisa::filesystem::path const &shader_path);
    static void destroy_instance();

private:
    Device &_device;
    vstd::spin_mutex _mtx;
    luisa::filesystem::path _shader_path;
    std::atomic_uint64_t _all_shader_count{};
    std::atomic_uint64_t _finished_shaders{};
    vstd::HashMap<string, ShaderVariant> _shaders;
    void _empty_path_error();
    void _captured_not_empty_error(string_view name);
    ShaderType const *_load_shader(
        luisa::filesystem::path const &shader_path,
        luisa::variant<
            luisa::span<Type const *const>,
            luisa::span<Variable const>>
            args,
        vstd::FuncRef<ShaderType(string_view shader_path)> &&create_func,
        ReloadFunc reload_func,
        bool support_preload);
    template<size_t N, typename... Args>
    [[nodiscard]] auto _compile(
        const Kernel<N, Args...> &kernel,
        const ShaderOption &option = {},
        bool support_preload = true) noexcept {
        // return _create<Shader<N, Args...>>(kernel.function()->function(), option);
        if (option.name.empty()) [[unlikely]] {
            _empty_path_error();
        }
        auto func = kernel.function().get();
        if (!func->bound_arguments().empty()) {
            _captured_not_empty_error(option.name);
        }
        auto cb = [&](string_view shader_path) -> ShaderType {
            auto new_option = option;
            new_option.name = shader_path;
            return _device.compile(kernel, new_option);
        };
        return static_cast<Shader<N, Args...> const *>(
            _load_shader(
                option.name,
                kernel.function()->arguments(),
                cb,
                nullptr,
                support_preload)
                ->template try_get<ShaderBase>());
    }
    luisa::string_view _path_to_key(luisa::filesystem::path const &path, luisa::string &can_path_str, luisa::string &buffer) const;

public:
    ShaderManager(Device &device, luisa::filesystem::path const &shader_path);
    ~ShaderManager();
    void get_preload_progress(uint64_t &all_shader_count, uint64_t &finished_shader_count);
    [[nodiscard]] ShaderBase unload_shader(luisa::filesystem::path const &shader_path);

    template<typename T>
        requires(
            std::is_pointer_v<T> &&
            is_lc_shader<std::remove_cvref_t<std::remove_pointer_t<T>>>::value)
    void load(luisa::filesystem::path const &shader_path, T &shader_ptr, bool support_preload = true) {
        using PureT = std::remove_cvref_t<std::remove_pointer_t<T>>;
        using TT = is_lc_shader<PureT>;
        auto c1 = [&](string_view shader_path) -> ShaderType {
            return TT::load_shader(_device, shader_path);
        };
        auto c2 = [](Device &device, string_view name, luisa::span<Type const* const> arg_types) -> ShaderType {
            return TT::load_shader(device, name);
        };
        shader_ptr = static_cast<T>(
            _load_shader(
                shader_path,
                TT::arg_types(),
                c1, c2,
                support_preload)
                ->template try_get<ShaderBase>());
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
    ShaderBase const *load_typeless(luisa::filesystem::path const &shader_path, luisa::span<Type const *const> types, bool support_preload = true);
    template<typename T>
        requires(std::is_pointer_v<T> &&
                 is_lc_shader<std::remove_cvref_t<std::remove_pointer_t<T>>>::value)
    void load_raster_shader(luisa::filesystem::path const &shader_path, T &shader_ptr) {
        using PureT = std::remove_cvref_t<std::remove_pointer_t<T>>;
        using TT = is_lc_shader<PureT>;
        auto c1 = [&](string_view shader_path) -> ShaderType {
            auto shader = TT::load_shader(_device, shader_path);
            return reinterpret_cast<RasterShader<> &&>(shader);
        };
        auto c2 = [](Device &device, string_view name, luisa::span<Type const* const> arg_types) -> ShaderType {
            auto shader = TT::load_shader(device, name);
            return reinterpret_cast<RasterShader<> &&>(shader);
        };
        shader_ptr = reinterpret_cast<T>(
            _load_shader(
                shader_path,
                TT::arg_types(),
                c1, c2,
                // raster no support preload
                false)
                ->template try_get<RasterShader<>>());
    }
    template<typename... Args>
    void async_load_raster_shader(luisa::fiber::counter &counter, luisa::string_view shader_name, RasterShader<Args...> const *&shader) {
        counter.add();
        luisa::fiber::schedule([this, counter, &shader, shader_name = luisa::filesystem::path(shader_name)] {
            load_raster_shader(shader_name, shader);
            counter.done();
        });
    }
    template<size_t N, typename Func>
        requires(std::negation_v<luisa::compute::detail::is_dsl_kernel<std::remove_cvref_t<Func>>> && N >= 1 && N <= 3)
    [[nodiscard]] auto compile(
        Func &&f,
        const ShaderOption &option = {}) noexcept {
        if constexpr (N == 1u) {
            return _compile(Kernel1D{std::forward<Func>(f)}, option);
        } else if constexpr (N == 2u) {
            return _compile(Kernel2D{std::forward<Func>(f)}, option);
        } else {
            return _compile(Kernel3D{std::forward<Func>(f)}, option);
        }
    }
    fiber::counter reload_shaders(Device &device);
    vstd::unique_ptr<vstd::IRange<string_view>> loaded_shaders() const;
    vstd::vector<char> loaded_shaders_json(luisa::filesystem::path const &shader_path) const;
    void preload_shaders(
        Device &device,
        luisa::filesystem::path const &shader_path,
        vstd::string_view json);

    void preload_shaders(
        Device &device,
        luisa::filesystem::path const &shader_path,
        vstd::vector<std::pair<vstd::string, vstd::vector<Type const *>>> &&registed_shaders);
};
}// namespace rbc