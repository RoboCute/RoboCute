#include <rbc_world/base_object.h>
#include <rbc_world/component.h>

namespace rbc::world {
struct TextureResource;
struct RBC_RUNTIME_API AtmosphereComponent final : ComponentDerive<AtmosphereComponent> {
    DECLARE_WORLD_OBJECT_FRIEND(AtmosphereComponent)
    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;
    void on_awake() override;
    void update_data() override;
    void on_destroy() override;
private:
    AtmosphereComponent();
    ~AtmosphereComponent();
    RC<TextureResource> _hdri;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::AtmosphereComponent);
