#include <rbc_world/components/atmosphere_component.h>
#include <rbc_world/type_register.h>
#include <rbc_world/resources/texture.h>
#include <rbc_graphics/graphics_utils.h>
#include <rbc_graphics/device_assets/device_image.h>

namespace rbc::world {
void AtmosphereComponent::serialize_meta(ObjSerialize const &obj) const {
    if (hdri) {
        obj.ar.value(hdri->guid(), "hdri");
    }
}
void AtmosphereComponent::deserialize_meta(ObjDeSerialize const &obj) {
    vstd::Guid hdri_guid;
    if (obj.ar.value(hdri_guid, "hdri")) {
        hdri = load_resource(hdri_guid);
    }
}
void AtmosphereComponent::on_awake() {
}
void AtmosphereComponent::update_data() {
    auto graphics = GraphicsUtils::instance();
    if (!graphics || !hdri) return;
    hdri->install();
    auto img = hdri->get_image();
    auto render_plugin = graphics->render_plugin();
    if (img && render_plugin)
        render_plugin->update_skybox(RC<DeviceImage>(img));
}
void AtmosphereComponent::on_destroy() {}
AtmosphereComponent::AtmosphereComponent() {}
AtmosphereComponent::~AtmosphereComponent() {}
DECLARE_WORLD_OBJECT_REGISTER(AtmosphereComponent)

}// namespace rbc::world