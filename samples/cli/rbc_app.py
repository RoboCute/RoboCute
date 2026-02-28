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
import mat_builtin as mat

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
    app._requires_reset = True


def display_cam_rotate(euler_x: float, euler_y: float, euler_z: float) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    transform = app.get_display_transform()
    if transform:
        quat = euler_to_quaternion(euler_x, euler_y, euler_z)
        new_rotation = lc.float4(quat[0], quat[1], quat[2], quat[3])
        transform.set_rotation(new_rotation, False)
    app._requires_reset = True
# Entity


def editing_select_object(uv_x: float, uv_y: float) -> Generator[Optional[re.world.Entity], None, None]:
    global app
    if app is None:
        app = rbc.app.App()
    if app.ctx is None:
        yield None
        return

    click_name = "__tui_agent_click__"
    app.ctx.editing_add_click_requires(click_name, lc.float2(uv_x, uv_y))

    select_query: re.world.SelectQuery = None
    while True:
        select_query = app.ctx.editing_query_click_requires(click_name)
        if not select_query.valid():
            yield None
        else:
            break
    comp = select_query.get()
    if not comp:
        yield None
    else:
        yield comp.entity()

def entity_transform_set_position(
    entity: re.world.Entity, x: float, y: float, z: float
) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        transform.set_pos(lc.double3(x, y, z), False)
        app._requires_reset = True
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
    global app
    if app is None:
        app = rbc.app.App()
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        current_pos = transform.position()
        new_pos = lc.double3(
            current_pos.x + x, current_pos.y + y, current_pos.z + z)
        transform.set_pos(new_pos, False)
        app._requires_reset = True
    else:
        raise Exception("TransformComponent not found.")


def entity_transform_set_rotation(
    entity: re.world.Entity, x: float, y: float, z: float, w: float
) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        transform.set_rotation(lc.float4(x, y, z, w), False)
        app._requires_reset = True
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
    global app
    if app is None:
        app = rbc.app.App()
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        quat = euler_to_quaternion(euler_x, euler_y, euler_z)
        transform.set_rotation(
            lc.float4(quat[0], quat[1], quat[2], quat[3]), False)
        app._requires_reset = True
    else:
        raise Exception("TransformComponent not found.")


def entity_transform_set_scale(
    entity: re.world.Entity, x: float, y: float, z: float
) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        transform.set_scale(lc.double3(x, y, z), False)
        app._requires_reset = True
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
    global app
    if app is None:
        app = rbc.app.App()
    transform = re.world.TransformComponent(
        entity.get_component("TransformComponent")
    )
    if transform:
        current_scale = transform.scale()
        new_scale = lc.double3(
            current_scale.x + x, current_scale.y + y, current_scale.z + z
        )
        transform.set_scale(new_scale, False)
        app._requires_reset = True
    else:
        raise Exception("TransformComponent not found.")


def entity_light_add_point_light(
    entity: re.world.Entity, r: float, g: float, b: float, visible: bool
) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    light = re.world.LightComponent(entity.get_component("LightComponent"))
    if light:
        light.add_point_light(lc.float3(r, g, b), visible)
        app._requires_reset = True
    else:
        raise Exception("LightComponent not found.")


def entity_light_add_area_light(
    entity: re.world.Entity, r: float, g: float, b: float, visible: bool
) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    light = re.world.LightComponent(entity.get_component("LightComponent"))
    if light:
        light.add_area_light(lc.float3(r, g, b), visible)
        app._requires_reset = True
    else:
        raise Exception("LightComponent not found.")


def entity_light_add_disk_light(
    entity: re.world.Entity, r: float, g: float, b: float, visible: bool
) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    light = re.world.LightComponent(entity.get_component("LightComponent"))
    if light:
        light.add_disk_light(lc.float3(r, g, b), visible)
        app._requires_reset = True
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
    global app
    if app is None:
        app = rbc.app.App()
    light = re.world.LightComponent(entity.get_component("LightComponent"))
    if light:
        light.add_spot_light(
            lc.float3(r, g, b),
            angle_radians,
            small_angle_radians,
            angle_atten_pow,
            visible
        )
        app._requires_reset = True
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
    global app
    if app is None:
        app = rbc.app.App()
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        cam.set_fov(value)
        app._requires_reset = True
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_get_near_plane(entity: re.world.Entity) -> float:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        return cam.near_plane()
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_set_near_plane(entity: re.world.Entity, value: float) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        cam.set_near_plane(value)
        app._requires_reset = True
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_get_far_plane(entity: re.world.Entity) -> float:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        return cam.far_plane()
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_set_far_plane(entity: re.world.Entity, value: float) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        cam.set_far_plane(value)
        app._requires_reset = True
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_get_focus_distance(entity: re.world.Entity) -> float:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        return cam.focus_distance()
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_set_focus_distance(entity: re.world.Entity, value: float) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        cam.set_focus_distance(value)
        app._requires_reset = True
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_get_aperture(entity: re.world.Entity) -> float:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        return cam.aperture()
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_set_aperture(entity: re.world.Entity, value: float) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        cam.set_aperture(value)
        app._requires_reset = True
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_get_aspect_ratio(entity: re.world.Entity) -> float:
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        return cam.aspect_ratio()
    else:
        raise Exception("CameraComponent not found.")


def entity_camera_set_aspect_ratio(entity: re.world.Entity, value: float) -> None:
    global app
    if app is None:
        app = rbc.app.App()
    cam = re.world.CameraComponent(entity.get_component("CameraComponent"))
    if cam:
        cam.set_aspect_ratio(value)
        app._requires_reset = True
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


def resource_material_load_from_openpbr(
    mat_res: re.world.MaterialResource, openpbr: mat.OpenPBRInterface
) -> None:
    """Load material properties from a JSON string.

    Args:
        mat_res: The MaterialResource to load data into.
        json_str: A JSON string containing material properties.

    Raises:
        Exception: If the material resource is invalid or JSON parsing fails.
    """
    if not mat_res:
        raise Exception("Invalid MaterialResource.")
    mat_res.load_from_json(mat.openpbr_dump_to_json(openpbr))


def resource_material_dump_openpbr(mat_res: re.world.MaterialResource) -> mat.OpenPBRInterface:
    """Dump material properties to a JSON string.

    Args:
        mat: The MaterialResource to dump data from.

    Returns:
        A JSON string containing all material properties.

    Raises:
        Exception: If the material resource is invalid.
    """
    if not mat_res:
        raise Exception("Invalid MaterialResource.")
    js = mat_res.dump_json()
    pbr = mat.OpenPBRInterface()
    mat.openpbr_load_from_json(pbr, js)
    return pbr
