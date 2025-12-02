#include <rbc_app/graphics_object_types.h>
namespace rbc {
ObjectStub::~ObjectStub() {
    auto sm = SceneManager::instance_ptr();
    if (!sm) return;
    switch (type) {
        case ObjectRenderType::Mesh:
            sm->accel_manager().remove_mesh_instance(
                sm->buffer_allocator(),
                sm->buffer_uploader(),
                mesh_tlas_idx);
            break;
        case ObjectRenderType::EmissionMesh:
            if (Lights::instance()) {
                Lights::instance()->remove_mesh_light(mesh_light_idx);
            }
            break;
        case ObjectRenderType::Procedural:
            sm->accel_manager().remove_procedural_instance(
                sm->buffer_allocator(),
                sm->buffer_uploader(),
                sm->dispose_queue(),
                procedural_idx);
            break;
    }
}
LightStub::~LightStub() {
    auto lights = Lights::instance();
    if (!lights) return;
    auto type = light_type;
    switch (type) {
        case rbc::LightType::Sphere:
            lights->remove_point_light(id);
            break;
        case rbc::LightType::Spot:
            lights->remove_spot_light(id);
            break;
        case rbc::LightType::Area:
            lights->remove_area_light(id);
            break;
        case rbc::LightType::Disk:
            lights->remove_disk_light(id);
            break;
        case rbc::LightType::Blas:
            lights->remove_mesh_light(id);
            break;
        default:
            LUISA_ERROR("Unsupported light type.");
    }
}
MaterialStub::~MaterialStub() {
    auto sm = SceneManager::instance_ptr();
    if (!sm) return;
    sm->mat_manager().discard_mat_instance(mat_code);
}
}// namespace rbc