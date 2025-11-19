#pragma once
#include <rbc_core/state_map.hpp>
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
struct CameraData {
    float4x4 view;
    float4x4 proj;
    float4x4 vp;
    float4x4 inv_view;
    float4x4 inv_sky_view;
    float4x4 inv_proj;
    float4x4 inv_vp;
    float3x3 world_to_sky;

    float4x4 last_view;
    float4x4 last_sky_view;
    float4x4 last_proj;
    float4x4 last_vp;
    float4x4 last_sky_vp;
    float4x4 last_inv_vp;
};
struct PreparePass;

struct RBC_RUNTIME_API PipelineCtxMutable {
private:
    vstd::HashMap<vstd::Guid, vstd::unique_ptr<PassContext>> _pass_contexts;

public:

    template<typename T, typename... Args>
        requires(std::is_base_of_v<PassContext, T> && luisa::is_constructible_v<T, Args && ...>)
    T *get_pass_context(Args &&...args) {
        return static_cast<T *>(_pass_contexts.emplace(rbc::TypeInfo::get<T>(), vstd::lazy_eval([&]() -> vstd::unique_ptr<PassContext> {
                                                           return vstd::make_unique<T>(std::forward<Args>(args)...);
                                                       }))
                                    .value()
                                    .get());
    }
    template<typename T>
        requires(std::is_base_of_v<PassContext, T>)
    vstd::unique_ptr<T> &get_pass_context_mut() {
        return reinterpret_cast<vstd::unique_ptr<T> &>(_pass_contexts.emplace(rbc::TypeInfo::get<T>()).value());
    }
    StateMap states;
    Image<float> const *resolved_img;
    luisa::fiber::counter before_frame_task;
    luisa::fiber::counter after_frame_task;
    PipelineCtxMutable();
    PipelineCtxMutable(PipelineCtxMutable &&) = delete;
    ~PipelineCtxMutable();
    PipelineCtxMutable &operator=(PipelineCtxMutable const &) = delete;
    PipelineCtxMutable &operator=(PipelineCtxMutable &&rhs) = delete;
    PipelineCtxMutable(PipelineCtxMutable const &) = delete;
    void clear();
    void delay_dispose(DisposeQueue &queue);
};

struct RBC_RUNTIME_API PipelineContext {
    // settings
    rbc::StateMap *pipeline_settings;

    // config
    Camera cam;
    float4x4 projection;

    // resource
    Device *device{nullptr};
    SceneManager *scene{nullptr};
    TexStreamManager *tex_stream_mng{nullptr};
    CommandList *cmdlist{nullptr};

    mutable PipelineCtxMutable mut;
    PipelineContext();

    PipelineContext(
        Device &device,
        SceneManager &scene,
        CommandList &cmdlist);

    ~PipelineContext();

    PipelineContext(PipelineContext const &) = delete;
    PipelineContext(PipelineContext &&) = delete;
    PipelineContext &operator=(PipelineContext const &) = delete;
    PipelineContext &operator=(PipelineContext &&rhs) = delete;
};
}// namespace rbc