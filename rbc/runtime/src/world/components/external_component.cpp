#include <rbc_world/components/external_component.h>
#include <rbc_world/type_register.h>

namespace rbc::world {
void ExternalComponent::on_awake() {
    if (_on_awake_func) _on_awake_func();
}
void ExternalComponent::on_destroy() {
    if (_on_destroy_func) _on_destroy_func();
}
void ExternalComponent::serialize_meta(ObjSerialize const &ser) const {}
void ExternalComponent::deserialize_meta(ObjDeSerialize const &ser) {}
rbc::coroutine ExternalComponent::before_frame() {
    while (true) {
        if (!_before_frame_func)
            co_return;
        _before_frame_func();
        co_await std::suspend_always{};
    }
    co_return;
}
rbc::coroutine ExternalComponent::after_frame() {
    while (true) {
        if (!_after_frame_func)
            co_return;
        _after_frame_func();
        co_await std::suspend_always{};
    }
    co_return;
}
ExternalComponent::ExternalComponent(Entity *entity) : ComponentDerive<ExternalComponent>(entity) {}
ExternalComponent::~ExternalComponent() {}
DECLARE_WORLD_COMPONENT_REGISTER(ExternalComponent);
}// namespace rbc::world