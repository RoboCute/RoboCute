#include <rbc_world/base_object.h>
#include <rbc_world/component.h>

namespace rbc::world {
struct RBC_RUNTIME_API CameraComponent final : ComponentDerive<CameraComponent> {
    DECLARE_WORLD_COMPONENT_FRIEND(CameraComponent)
private:
    CameraComponent(Entity *entity);
    ~CameraComponent();
    void *_render_pipe_ctx{};
public:
    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;
};
}// namespace rbc::world

RBC_RTTI(rbc::world::CameraComponent);
