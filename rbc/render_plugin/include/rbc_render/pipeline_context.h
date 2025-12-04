#pragma once
#include <rbc_core/state_map.h>
#include <rbc_graphics/camera.h>
#include <rbc_graphics/dispose_queue.h>
#include <rbc_graphics/mat_code.h>
#include <luisa/runtime/command_list.h>
#include <luisa/runtime/rtx/accel.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/image.h>
#include <luisa/vstl/common.h>
#include <luisa/vstl/functional.h>
#include <luisa/vstl/lockfree_array_queue.h>

namespace rbc {
using namespace luisa::compute;
struct SceneManager;
struct TexStreamManager;
struct PassContext {
    virtual ~PassContext() = default;
};

struct RayCastRequire {
    float3 ray_origin;
    float3 ray_dir;
    float t_min;
    float t_max;
    uint8_t mask = std::numeric_limits<uint8_t>::max();
};
struct ClickRequire {
    float2 screen_uv;
    uint8_t mask = std::numeric_limits<uint8_t>::max();
};
struct RayCastResult {
    MatCode mat_code;
    uint inst_id;
    uint prim_id;
    uint submesh_id;
    float ray_length;
    float2 triangle_bary;
};
struct PreparePass;
struct ClickManager {
    friend struct PreparePass;
    inline void add_require(
        luisa::string &&key,
        RayCastRequire const &require) {
        _requires.emplace_back(std::move(key), require);
    }
    inline void add_require(
        luisa::string &&key,
        ClickRequire const &require) {
        _requires.emplace_back(std::move(key), require);
    }
    inline luisa::optional<RayCastResult> query_result(luisa::string_view key) {
        // TODO @zhurong: should this be erased after found?
        std::lock_guard lck{_mtx};
        if (auto kvp = _results.find(key)) {
            auto disp = vstd::scope_exit([&]() {
                _results.remove(kvp);
            });
            if (kvp.value().ray_length < 0.0f) {
                return {};
            }
            return kvp.value();
        }
        return {};
    }
    inline void clear_requires() {
        _requires.clear();
    }
    void clear() {
        _requires.clear();
        _results.clear();
    }

private:
    luisa::vector<std::pair<luisa::string, luisa::variant<RayCastRequire, ClickRequire>>> _requires;
    vstd::HashMap<luisa::string, RayCastResult> _results;
    luisa::spin_mutex _mtx;
};

struct PipelineCtxMutable {
private:
    vstd::HashMap<TypeInfo, vstd::unique_ptr<PassContext>> _pass_contexts;

public:
    ClickManager click_manager;

    template<typename T, typename... Args>
        requires(std::is_base_of_v<PassContext, T> && luisa::is_constructible_v<T, Args && ...>)
    T *get_pass_context(Args &&...args) {
        auto lambda = [&]() -> vstd::unique_ptr<PassContext> {
            return vstd::make_unique<T>(std::forward<Args>(args)...);
        };
        return static_cast<T *>(
            _pass_contexts.emplace(
                              TypeInfo::get<T>(),
                              vstd::lazy_eval(std::move(lambda)))
                .value()
                .get());
    }
    template<typename T>
        requires(std::is_base_of_v<PassContext, T>)
    vstd::unique_ptr<T> &get_pass_context_mut() {
        return reinterpret_cast<vstd::unique_ptr<T> &>(_pass_contexts.emplace(TypeInfo::get<T>()).value());
    }
    PipelineCtxMutable();
    PipelineCtxMutable(PipelineCtxMutable &&) = delete;
    ~PipelineCtxMutable();
    PipelineCtxMutable &operator=(PipelineCtxMutable const &) = delete;
    PipelineCtxMutable &operator=(PipelineCtxMutable &&rhs) = delete;
    PipelineCtxMutable(PipelineCtxMutable const &) = delete;
    void clear();
    void delay_dispose(DisposeQueue &queue);
};
struct HDRI;
struct SkyAtmosphere;
struct PipelineContext : vstd::IOperatorNewBase {
    // settings
    rbc::StateMap *pipeline_settings;

    // config
    Camera cam;

    // resource
    Device *device{nullptr};
    Stream *stream{nullptr};
    SceneManager *scene{nullptr};
    CommandList *cmdlist{nullptr};

    mutable PipelineCtxMutable mut;
    PipelineContext();

    PipelineContext(
        Device &device,
        Stream &stream,
        SceneManager &scene,
        CommandList &cmdlist,
        rbc::StateMap *pipeline_settings);

    ~PipelineContext();

    PipelineContext(PipelineContext const &) = delete;
    PipelineContext(PipelineContext &&) = delete;
    PipelineContext &operator=(PipelineContext const &) = delete;
    PipelineContext &operator=(PipelineContext &&rhs) = delete;
};
}// namespace rbc