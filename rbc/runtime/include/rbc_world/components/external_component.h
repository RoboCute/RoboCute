#pragma once
#include <rbc_world/component.h>
namespace rbc::world {

struct RBC_RUNTIME_API ExternalComponent final : ComponentDerive<ExternalComponent> {
    DECLARE_WORLD_OBJECT_FRIEND(ExternalComponent)
private:
    ExternalComponent();
    ~ExternalComponent();
public:
    luisa::function<void()> _on_awake_func;
    luisa::function<void()> _on_destroy_func;
    luisa::function<void()> _before_frame_func;
    luisa::function<void()> _after_frame_func;
    void on_awake() override;
    void on_destroy() override;
    void serialize_meta(ObjSerialize const &ser) const override;
    void deserialize_meta(ObjDeSerialize const &ser) override;
    rbc::coroutine before_frame();
    rbc::coroutine after_frame();
};
}// namespace rbc::world
RBC_RTTI(rbc::world::ExternalComponent)