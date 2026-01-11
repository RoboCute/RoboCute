#include <rbc_world/base_object.h>
#include <rbc_world/component.h>
#include <luisa/runtime/image.h>

namespace rbc::world {
struct RBC_RUNTIME_API CameraComponent final : ComponentDerive<CameraComponent> {
    DECLARE_WORLD_COMPONENT_FRIEND(CameraComponent)
private:
    CameraComponent(Entity *entity);
    ~CameraComponent();
    void *_render_pipe_ctx{};
    void _on_transform_update();

public:
    double fov = luisa::radians(60.0f);
    double aspect_ratio = 1;
    double near_plane = 0.3f;
    double far_plane = 10000.0f;
    bool auto_aspect_ratio = true;
    // physical camera
    bool enable_physical_camera = false;
    double aperture = 1.4f;
    double focus_distance = 2.0f;
    luisa::compute::Image<float> dst_image;
    luisa::uint2 view_offset_pixels{};
    luisa::uint2 view_size_pixels{~0u};

    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;
    void on_awake() override;
    void on_destroy() override;
    void enable_camera();
    void disable_camera();
};
}// namespace rbc::world

RBC_RTTI(rbc::world::CameraComponent);
