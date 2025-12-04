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

struct PreparePass;

struct PipelineCtxMutable {
private:
    vstd::HashMap<TypeInfo, vstd::unique_ptr<PassContext>> _pass_contexts;

public:

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
    void clear();
    ~PipelineContext();

    PipelineContext(PipelineContext const &) = delete;
    PipelineContext(PipelineContext &&) = delete;
    PipelineContext &operator=(PipelineContext const &) = delete;
    PipelineContext &operator=(PipelineContext &&rhs) = delete;
};
}// namespace rbc
