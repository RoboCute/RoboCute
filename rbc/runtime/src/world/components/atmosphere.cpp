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
    if (!graphics) return;
    DeviceImage *img{};
    if (hdri) {
        hdri->install();
        img = hdri->get_image();
    }
    auto render_plugin = graphics->render_plugin();
    if (render_plugin) {
        if (img)
            render_plugin->update_skybox(RC<DeviceImage>(img));
        else
            render_plugin->update_skybox(uint2(2048, 1024));
    }
}
void AtmosphereComponent::on_destroy() {}
AtmosphereComponent::AtmosphereComponent() {}
AtmosphereComponent::~AtmosphereComponent() {}
DECLARE_WORLD_OBJECT_REGISTER(AtmosphereComponent)

}// namespace rbc::world