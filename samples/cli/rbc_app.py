from robocute.rbc_ext.luisa import (
    float2,
    float3,
    float4,
    int2,
    int3,
    int4,
    uint2,
    uint3,
    uint4,
    double2,
    double3,
    double4,
    bool2,
    bool3,
    bool4,
    half2,
    half3,
    half4,
    short2,
    short3,
    short4,
    ushort2,
    ushort3,
    ushort4,
    float2x2,
    float3x3,
    float4x4,
    make_float2,
    make_float3,
    make_float4,
    make_int2,
    make_int3,
    make_int4,
    make_bool2,
    make_bool3,
    make_bool4,
    make_float2x2,
    make_float3x3,
    make_float4x4,
)
import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re
from robocute.rbc_ext._C import lcapi_c as lcapi
import samples.cli as cli
from robocute.utils.rotation import euler_to_quaternion
from typing import Optional, Generator
import math

app: rbc.app.App = None


def display_cam_add_pos(x: float, y: float, z: float) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    transform = app.get_display_transform()
    if transform:
        current_pos = transform.position()
        new_pos = lc.double3(
            current_pos.x + x,
            current_pos.y + y,
            current_pos.z + z
        )
        transform.set_pos(new_pos, False)


def display_cam_rotate(euler_x: float, euler_y: float, euler_z: float) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    transform = app.get_display_transform()
    if transform:
        quat = euler_to_quaternion(euler_x, euler_y, euler_z)
        new_rotation = lc.float4(quat[0], quat[1], quat[2], quat[3])
        transform.set_rotation(new_rotation, False)

####################### Entity

def editing_select_object(uv_x: float, uv_y: float) -> Generator[Optional[re.world.Entity], None, None]:
    global app
    if app is None:
        app = rbc.app.App()
    if app.ctx is None:
        yield None
        return

    click_name = "__tui_agent_click__"
    app.ctx.editing_add_click_requires(click_name, lc.float2(uv_x, uv_y))

    render_comp = None
    while render_comp is None:
        render_comp = app.ctx.editing_query_click_requires(click_name)
        if render_comp is None:
            yield None

    entity = render_comp.entity()
    yield entity


def entity_transform_set_position(
    entity: re.world.Entity, x: float, y: float, z: float
) -> None:
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        transform.set_pos(lc.double3(x, y, z), False)
    else:
        raise Exception("TransformComponent not found.")


def entity_transform_get_position(entity: re.world.Entity) -> lc.double3:
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        return transform.position()
    else:
        raise Exception("TransformComponent not found.")


def entity_transform_add_position(
    entity: re.world.Entity, x: float, y: float, z: float
) -> None:
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        current_pos = transform.position()
        new_pos = lc.double3(
            current_pos.x + x, current_pos.y + y, current_pos.z + z)
        transform.set_pos(new_pos, False)
    else:
        raise Exception("TransformComponent not found.")


def entity_transform_set_rotation(
    entity: re.world.Entity, x: float, y: float, z: float, w: float
) -> None:
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        transform.set_rotation(lc.float4(x, y, z, w), False)
    else:
        raise Exception("TransformComponent not found.")


def entity_transform_get_rotation(entity: re.world.Entity) -> lc.float4:
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        return transform.rotation()
    else:
        raise Exception("TransformComponent not found.")


def entity_transform_set_rotation_euler(
    entity: re.world.Entity, euler_x: float, euler_y: float, euler_z: float
) -> None:
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        quat = euler_to_quaternion(euler_x, euler_y, euler_z)
        transform.set_rotation(
            lc.float4(quat[0], quat[1], quat[2], quat[3]), False)
    else:
        raise Exception("TransformComponent not found.")


def entity_transform_set_scale(
    entity: re.world.Entity, x: float, y: float, z: float
) -> None:
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        transform.set_scale(lc.double3(x, y, z), False)
    else:
        raise Exception("TransformComponent not found.")


def entity_transform_get_scale(entity: re.world.Entity) -> lc.double3:
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        return transform.scale()
    else:
        raise Exception("TransformComponent not found.")


def entity_transform_add_scale(
    entity: re.world.Entity, x: float, y: float, z: float
) -> None:
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        current_scale = transform.scale()
        new_scale = lc.double3(
            current_scale.x + x, current_scale.y + y, current_scale.z + z
        )
        transform.set_scale(new_scale, False)
    else:
        raise Exception("TransformComponent not found.")


def entity_light_add_point_light(
    entity: re.world.Entity, r: float, g: float, b: float, visible: bool
) -> None:
    light = re.world.LightComponent(entity.get_component("LightComponent"))
    if light:
        light.add_point_light(lc.float3(r, g, b), visible)
    else:
        raise Exception("LightComponent not found.")


def entity_light_add_area_light(
    entity: re.world.Entity, r: float, g: float, b: float, visible: bool
) -> None:
    light = re.world.LightComponent(entity.get_component("LightComponent"))
    if light:
        light.add_area_light(lc.float3(r, g, b), visible)
    else:
        raise Exception("LightComponent not found.")


def entity_light_add_disk_light(
    entity: re.world.Entity, r: float, g: float, b: float, visible: bool
) -> None:
    light = re.world.LightComponent(entity.get_component("LightComponent"))
    if light:
        light.add_disk_light(lc.float3(r, g, b), visible)
    else:
        raise Exception("LightComponent not found.")


def entity_light_add_spot_light(
    entity: re.world.Entity,
    r: float, g: float, b: float,
    angle_radians: float,
    small_angle_radians: float,
    angle_atten_pow: float,
    visible: bool
) -> None:
    light = re.world.LightComponent(entity.get_component("LightComponent"))
    if light:
        light.add_spot_light(
            lc.float3(r, g, b),
            angle_radians,
            small_angle_radians,
            angle_atten_pow,
            visible
        )
    else:
        raise Exception("LightComponent not found.")


def entity_light_get_luminance(entity: re.world.Entity) -> lc.float3:
    light = re.world.LightComponent(entity.get_component("LightComponent"))
    if light:
        return light.luminance()
    else:
        raise Exception("LightComponent not found.")


def entity_light_get_angle_radians(entity: re.world.Entity) -> float:
    light = re.world.LightComponent(entity.get_component("LightComponent"))
    if light:
        return light.angle_radians()
    else:
        raise Exception("LightComponent not found.")


def entity_light_get_small_angle_radians(entity: re.world.Entity) -> float:
    light = re.world.LightComponent(entity.get_component("LightComponent"))
    if light:
        return light.small_angle_radians()
    else:
        raise Exception("LightComponent not found.")


def entity_light_get_angle_atten_pow(entity: re.world.Entity) -> float:
    light = re.world.LightComponent(entity.get_component("LightComponent"))
    if light:
        return light.angle_atten_pow()
    else:
        raise Exception("LightComponent not found.")


def entity_camera_get_fov(entity: re.world.Entity) -> float:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        return cam.fov()
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_set_fov(entity: re.world.Entity, value: float) -> None:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        cam.set_fov(value)
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_get_near_plane(entity: re.world.Entity) -> float:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        return cam.near_plane()
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_set_near_plane(entity: re.world.Entity, value: float) -> None:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        cam.set_near_plane(value)
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_get_far_plane(entity: re.world.Entity) -> float:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        return cam.far_plane()
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_set_far_plane(entity: re.world.Entity, value: float) -> None:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        cam.set_far_plane(value)
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_get_focus_distance(entity: re.world.Entity) -> float:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        return cam.focus_distance()
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_set_focus_distance(entity: re.world.Entity, value: float) -> None:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        cam.set_focus_distance(value)
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_get_aperture(entity: re.world.Entity) -> float:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        return cam.aperture()
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_set_aperture(entity: re.world.Entity, value: float) -> None:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        cam.set_aperture(value)
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_get_aspect_ratio(entity: re.world.Entity) -> float:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        return cam.aspect_ratio()
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_set_aspect_ratio(entity: re.world.Entity, value: float) -> None:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        cam.set_aspect_ratio(value)
    else:
        raise Exception("CameraComponent not found.")


def entity_render_get_mat_count(entity: re.world.Entity) -> int:
    render = re.world.RenderComponent(entity.get_component("RenderComponent"))
    if render:
        return render.mat_count()
    else:
        raise Exception("RenderComponent not found.")


def entity_render_get_mat(entity: re.world.Entity, index: int) -> re.world.MaterialResource:
    render = re.world.RenderComponent(entity.get_component("RenderComponent"))
    if render:
        return render.get_material(index)
    else:
        raise Exception("RenderComponent not found.")


def entity_render_remove_object(entity: re.world.Entity) -> None:
    render = re.world.RenderComponent(entity.get_component("RenderComponent"))
    if render:
        render.remove_object()
    else:
        raise Exception("RenderComponent not found.")

####################### Resources

# TODO 