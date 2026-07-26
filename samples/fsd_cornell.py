"""Render the FSD paper's star-aperture Cornell-box validation scene.

The source scene is ``scenes/cornell-box/box.xml`` from
https://github.com/ssteinberg/fsdBSDFpaper. That scene uses a 100 um test
wavelength. RoboCute samples visible wavelengths, so every geometric length is
scaled by 620 nm / 100 um. This preserves the dimensionless geometry-to-
wavelength ratios without extending RoboCute's visible spectrum.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image

import robocute as rbc
import robocute.rbc_ext as rbce
import robocute.rbc_ext.luisa as lc
from mesh_builder import MeshBuilder


ROOT = Path(__file__).resolve().parents[1]
REFERENCE_STAR_ASSET = ROOT / "samples" / "assets" / "fsd" / "star.obj"

# The Mitsuba reference scene renders at 100 um. Scaling all lengths to a
# representative visible wavelength preserves k*x and every other
# dimensionless quantity used by the FSD model.
REFERENCE_WAVELENGTH_METERS = 100.0e-6
VISIBLE_REFERENCE_WAVELENGTH_METERS = 620.0e-9
WAVELENGTH_GEOMETRY_SCALE = (
    VISIBLE_REFERENCE_WAVELENGTH_METERS / REFERENCE_WAVELENGTH_METERS
)

REFERENCE_ROOM_HALF_EXTENT = 1.0
REFERENCE_ROOM_HEIGHT = 2.0
REFERENCE_ROOM_BACK_Z = -1.0
REFERENCE_ROOM_FRONT_Z = 1.0
REFERENCE_CAMERA_POSITION = (0.0, 1.0, 6.8)
REFERENCE_CAMERA_FOV_DEGREES = 19.75
REFERENCE_CAMERA_NEAR_PLANE = 0.01
REFERENCE_CAMERA_FAR_PLANE = 10.0

REFERENCE_SCREEN_X = -0.33
REFERENCE_SCREEN_CENTER_Y = 1.0
REFERENCE_SCREEN_CENTER_Z = 0.0
# star.obj has X/Y half-extents 1.8192485/1.8995025. The XML's rotations map
# those axes to screen Y/Z before applying scales 0.315/0.23.
REFERENCE_STAR_OBJ_HALF_EXTENT_X = 1.8192485
REFERENCE_STAR_OBJ_HALF_EXTENT_Y = 1.8995025
REFERENCE_STAR_OBJ_THICKNESS_SCALE = 0.01
REFERENCE_STAR_OBJ_SCALE_X = 0.315
REFERENCE_STAR_OBJ_SCALE_Y = 0.23
REFERENCE_STAR_OBJ_VERTEX_COUNT = 169
REFERENCE_STAR_OBJ_TRIANGLE_COUNT = 310
REFERENCE_SCREEN_HALF_HEIGHT = (
    REFERENCE_STAR_OBJ_HALF_EXTENT_X * REFERENCE_STAR_OBJ_SCALE_X
)
REFERENCE_SCREEN_HALF_WIDTH = (
    REFERENCE_STAR_OBJ_HALF_EXTENT_Y * REFERENCE_STAR_OBJ_SCALE_Y
)
# The central aperture vertices in star.obj reach |X|=0.027085 and
# |Y|=0.033786. Preserve the resulting anisotropy instead of inventing a
# circular replacement.
REFERENCE_STAR_APERTURE_RADIUS_X = 0.027085
REFERENCE_STAR_APERTURE_RADIUS_Y = 0.033786
REFERENCE_STAR_APERTURE_OUTER_RADIUS_Y = (
    REFERENCE_STAR_APERTURE_RADIUS_X * REFERENCE_STAR_OBJ_SCALE_X
)
REFERENCE_STAR_APERTURE_OUTER_RADIUS_Z = (
    REFERENCE_STAR_APERTURE_RADIUS_Y * REFERENCE_STAR_OBJ_SCALE_Y
)

# RBC samples this wavelength interval (spectrum_args.hpp). Validation crops
# use the upper bound so the longest-wavelength FSD lobe/search radius fits.
RBC_SAMPLED_WAVELENGTH_MAX_METERS = 830.0e-9
REFERENCE_EQUIVALENT_MAX_WAVELENGTH = (
    RBC_SAMPLED_WAVELENGTH_MAX_METERS / WAVELENGTH_GEOMETRY_SCALE
)
REFERENCE_APERTURE_DIAMETER_Y = 2.0 * REFERENCE_STAR_APERTURE_OUTER_RADIUS_Y
REFERENCE_APERTURE_DIAMETER_Z = 2.0 * REFERENCE_STAR_APERTURE_OUTER_RADIUS_Z
REFERENCE_SCREEN_TO_RECEIVER_DISTANCE = (
    REFERENCE_ROOM_HALF_EXTENT - REFERENCE_SCREEN_X
)
REFERENCE_VALIDATION_CAMERA_X = 0.5 * (
    REFERENCE_SCREEN_X + REFERENCE_ROOM_HALF_EXTENT
)
REFERENCE_VALIDATION_CAMERA_TO_TARGET_DISTANCE = (
    0.5 * REFERENCE_SCREEN_TO_RECEIVER_DISTANCE
)
VALIDATION_DIFFRACTION_ORDER = 2.0
FSD_QUERY_RADIUS_WAVELENGTH_FACTOR = 75.0

REFERENCE_SPOT_POSITION = (-0.99, 1.0, 0.0)
REFERENCE_SPOT_TARGET = (5.0, 1.0, 0.0)
REFERENCE_SPOT_CUTOFF_DEGREES = 12.5
# Mitsuba's default beam width is three quarters of the cutoff angle.
REFERENCE_SPOT_INNER_DEGREES = 0.75 * REFERENCE_SPOT_CUTOFF_DEGREES
REFERENCE_SPOT_RADIANT_INTENSITY = 20_000.0
REFERENCE_SPOT_TO_SCREEN_DISTANCE = (
    REFERENCE_SCREEN_X - REFERENCE_SPOT_POSITION[0]
)
# The reference spot is delta-position. RBC samples a finite sphere and forms
# 1-cos(theta) in float32, so choose the smallest proxy cone that retains four
# float32 ulps in that term. This is a numerical bound, not an emitter radius
# taken from the reference's unrelated decorative cylinder.
RBC_SPOT_CONE_COSINE_STABILITY_ULPS = 4.0
RBC_FLOAT32_EPSILON = float(np.finfo(np.float32).eps)
RBC_SPOT_PROXY_COSINE_DIFFERENCE = (
    RBC_SPOT_CONE_COSINE_STABILITY_ULPS * RBC_FLOAT32_EPSILON
)
RBC_SPOT_PROXY_ANGULAR_RADIUS = math.acos(
    1.0 - RBC_SPOT_PROXY_COSINE_DIFFERENCE
)
RBC_SPOT_PROXY_RADIUS = REFERENCE_SPOT_TO_SCREEN_DISTANCE * math.sin(
    RBC_SPOT_PROXY_ANGULAR_RADIUS
)
RBC_SPOT_PROXY_SOLID_ANGLE = (
    2.0 * math.pi * RBC_SPOT_PROXY_COSINE_DIFFERENCE
)
# Match Mitsuba's on-axis I/d^2 irradiance with RBC's L*solid_angle estimator.
RBC_SPOT_RADIANCE = REFERENCE_SPOT_RADIANT_INTENSITY / (
    REFERENCE_SPOT_TO_SCREEN_DISTANCE**2 * RBC_SPOT_PROXY_SOLID_ANGLE
)
SPOT_ANGLE_ATTENUATION_POWER = 1.0
REFERENCE_FSD_SCALE = 1.0

SRGB_LINEAR_THRESHOLD = 0.0031308
SRGB_LINEAR_SLOPE = 12.92
SRGB_POWER_SCALE = 1.055
SRGB_POWER_EXPONENT = 1.0 / 2.4
SRGB_POWER_OFFSET = 0.055


def _receiver_validation_half_extent() -> float:
    half_extents = []
    for aperture_diameter in (
        REFERENCE_APERTURE_DIAMETER_Y,
        REFERENCE_APERTURE_DIAMETER_Z,
    ):
        feature_angle = math.asin(
            VALIDATION_DIFFRACTION_ORDER
            * REFERENCE_EQUIVALENT_MAX_WAVELENGTH
            / aperture_diameter
        )
        half_extents.append(
            REFERENCE_SCREEN_TO_RECEIVER_DISTANCE * math.tan(feature_angle)
        )
    return max(half_extents)


def _camera_config(
    view: str,
) -> tuple[
    tuple[float, float, float],
    tuple[float, float, float],
    float,
]:
    if view == "reference":
        return (
            REFERENCE_CAMERA_POSITION,
            (0.0, REFERENCE_SCREEN_CENTER_Y, REFERENCE_SCREEN_CENTER_Z),
            REFERENCE_CAMERA_FOV_DEGREES,
        )

    position = (
        REFERENCE_VALIDATION_CAMERA_X,
        REFERENCE_SCREEN_CENTER_Y,
        REFERENCE_SCREEN_CENTER_Z,
    )
    if view == "receiver":
        target = (
            REFERENCE_ROOM_HALF_EXTENT,
            REFERENCE_SCREEN_CENTER_Y,
            REFERENCE_SCREEN_CENTER_Z,
        )
        half_extent = _receiver_validation_half_extent()
    else:
        target = (
            REFERENCE_SCREEN_X,
            REFERENCE_SCREEN_CENTER_Y,
            REFERENCE_SCREEN_CENTER_Z,
        )
        maximum_query_radius = (
            FSD_QUERY_RADIUS_WAVELENGTH_FACTOR
            * REFERENCE_EQUIVALENT_MAX_WAVELENGTH
        )
        half_extent = max(
            REFERENCE_STAR_APERTURE_OUTER_RADIUS_Y,
            REFERENCE_STAR_APERTURE_OUTER_RADIUS_Z,
        ) + maximum_query_radius

    fov_radians = 2.0 * math.atan(
        half_extent / REFERENCE_VALIDATION_CAMERA_TO_TARGET_DISTANCE
    )
    return position, target, math.degrees(fov_radians)


def _save_display_outputs(
    app: rbc.app.App,
    output: Path,
    resolution: tuple[int, int],
    linear_output: Path | None,
    tone_map: str,
) -> None:
    if tone_map == "none" and linear_output is None:
        app.ctx.save_display_image_to(str(output))
        return

    display = app.display_image(float)
    pixels = np.empty(
        (resolution[1], resolution[0], 4),
        dtype=np.float32,
    )
    display.copy_to(pixels)
    linear_rgb = np.maximum(pixels[..., :3], 0.0)
    if linear_output is not None:
        linear_output.parent.mkdir(parents=True, exist_ok=True)
        np.save(linear_output, linear_rgb)

    if tone_map == "none":
        app.ctx.save_display_image_to(str(output))
        return

    # Use the reference emitter intensity as a fixed scene scale so FSD and
    # control renders share an exposure, then apply global Reinhard and sRGB.
    normalized = linear_rgb / REFERENCE_SPOT_RADIANT_INTENSITY
    mapped = normalized / (1.0 + normalized)
    srgb = np.where(
        mapped <= SRGB_LINEAR_THRESHOLD,
        SRGB_LINEAR_SLOPE * mapped,
        SRGB_POWER_SCALE * np.power(mapped, SRGB_POWER_EXPONENT)
        - SRGB_POWER_OFFSET,
    )
    encoded = np.uint8(np.clip(np.rint(srgb * 255.0), 0.0, 255.0))
    Image.fromarray(encoded, mode="RGB").save(output)


def _scaled(value: float) -> float:
    return value * WAVELENGTH_GEOMETRY_SCALE


def _scaled_point(
    point: tuple[float, float, float],
) -> tuple[float, float, float]:
    return tuple(_scaled(component) for component in point)


def _parse_resolution(value: str) -> tuple[int, int]:
    try:
        width, height = (int(part) for part in value.lower().split("x", 1))
    except ValueError as error:
        raise argparse.ArgumentTypeError("expected WIDTHxHEIGHT") from error
    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError("resolution must be positive")
    return width, height


def _unit_interval(value: str) -> float:
    try:
        result = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("expected a number in [0, 1]") from error
    if not 0.0 <= result <= 1.0:
        raise argparse.ArgumentTypeError("expected a number in [0, 1]")
    return result


def _material(description: dict[str, object]) -> rbce.world.MaterialResource:
    material = rbce.world.MaterialResource()
    material.load_from_json(json.dumps(description))
    return material


def _build_mesh(
    positions: list[tuple[float, float, float]],
    normals: list[tuple[float, float, float]] | None,
    triangles: list[tuple[int, int, int]],
    submesh_offsets: list[int],
) -> rbce.world.MeshResource:
    builder = MeshBuilder(
        vertex_count=len(positions),
        triangle_count=len(triangles),
        submesh_offsets=np.asarray(submesh_offsets, dtype=np.uint32),
        has_normal=normals is not None,
    )
    builder.set_positions(np.asarray(positions, dtype=np.float32))
    if normals is not None:
        builder.set_normals(np.asarray(normals, dtype=np.float32))
    builder.set_triangles(np.asarray(triangles, dtype=np.uint32))
    mesh = builder.get_mesh()
    mesh.install()
    return mesh


def _build_cornell_room() -> rbce.world.MeshResource:
    half_extent = _scaled(REFERENCE_ROOM_HALF_EXTENT)
    height = _scaled(REFERENCE_ROOM_HEIGHT)
    back_z = _scaled(REFERENCE_ROOM_BACK_Z)
    front_z = _scaled(REFERENCE_ROOM_FRONT_Z)
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    triangles: list[tuple[int, int, int]] = []
    submesh_offsets: list[int] = []

    def add_quad(
        vertices: tuple[
            tuple[float, float, float],
            tuple[float, float, float],
            tuple[float, float, float],
            tuple[float, float, float],
        ],
        normal: tuple[float, float, float],
    ) -> None:
        submesh_offsets.append(len(triangles))
        base = len(positions)
        positions.extend(vertices)
        normals.extend([normal] * 4)
        triangles.extend(
            ((base, base + 1, base + 2), (base + 1, base + 3, base + 2))
        )

    # Each quad faces into the open-front box. Submesh order matches the
    # material list in main: floor, ceiling, back, left, right.
    add_quad(
        (
            (-half_extent, 0.0, back_z),
            (-half_extent, 0.0, front_z),
            (half_extent, 0.0, back_z),
            (half_extent, 0.0, front_z),
        ),
        (0.0, 1.0, 0.0),
    )
    add_quad(
        (
            (-half_extent, height, back_z),
            (half_extent, height, back_z),
            (-half_extent, height, front_z),
            (half_extent, height, front_z),
        ),
        (0.0, -1.0, 0.0),
    )
    add_quad(
        (
            (-half_extent, 0.0, back_z),
            (half_extent, 0.0, back_z),
            (-half_extent, height, back_z),
            (half_extent, height, back_z),
        ),
        (0.0, 0.0, 1.0),
    )
    add_quad(
        (
            (-half_extent, 0.0, front_z),
            (-half_extent, 0.0, back_z),
            (-half_extent, height, front_z),
            (-half_extent, height, back_z),
        ),
        (1.0, 0.0, 0.0),
    )
    add_quad(
        (
            (half_extent, 0.0, back_z),
            (half_extent, 0.0, front_z),
            (half_extent, height, back_z),
            (half_extent, height, front_z),
        ),
        (-1.0, 0.0, 0.0),
    )
    return _build_mesh(positions, normals, triangles, submesh_offsets)


def _build_star_aperture_screen() -> rbce.world.MeshResource:
    """Load the reference screen while preserving its shared OBJ topology."""

    positions: list[tuple[float, float, float]] = []
    triangles: list[tuple[int, int, int]] = []
    for line_number, line in enumerate(
        REFERENCE_STAR_ASSET.read_text(encoding="utf-8").splitlines(),
        start=1,
    ):
        fields = line.split()
        if not fields or fields[0].startswith("#"):
            continue
        if fields[0] == "v":
            if len(fields) != 4:
                raise ValueError(
                    f"{REFERENCE_STAR_ASSET}:{line_number}: invalid vertex"
                )
            obj_x, obj_y, obj_z = (float(value) for value in fields[1:])
            # Reference XML: Rz(90), Ry(90), scale(.01, .315, .23),
            # then translate(-.33, 1, 0). This maps OBJ (x, y, z) to
            # screen-space (z, x, y), including the original thickness.
            positions.append(
                _scaled_point(
                    (
                        REFERENCE_SCREEN_X
                        + REFERENCE_STAR_OBJ_THICKNESS_SCALE * obj_z,
                        REFERENCE_SCREEN_CENTER_Y
                        + REFERENCE_STAR_OBJ_SCALE_X * obj_x,
                        REFERENCE_SCREEN_CENTER_Z
                        + REFERENCE_STAR_OBJ_SCALE_Y * obj_y,
                    )
                )
            )
        elif fields[0] == "f":
            if len(fields) != 4:
                raise ValueError(
                    f"{REFERENCE_STAR_ASSET}:{line_number}: "
                    "only triangular faces are supported"
                )
            try:
                indices = [
                    int(vertex.split("/", 1)[0]) - 1
                    for vertex in fields[1:]
                ]
            except ValueError as error:
                raise ValueError(
                    f"{REFERENCE_STAR_ASSET}:{line_number}: invalid face"
                ) from error
            if any(index < 0 for index in indices):
                raise ValueError(
                    f"{REFERENCE_STAR_ASSET}:{line_number}: "
                    "relative OBJ indices are not supported"
                )
            triangles.append((indices[0], indices[1], indices[2]))

    if len(positions) != REFERENCE_STAR_OBJ_VERTEX_COUNT:
        raise ValueError(
            f"{REFERENCE_STAR_ASSET}: expected "
            f"{REFERENCE_STAR_OBJ_VERTEX_COUNT} vertices, got {len(positions)}"
        )
    if len(triangles) != REFERENCE_STAR_OBJ_TRIANGLE_COUNT:
        raise ValueError(
            f"{REFERENCE_STAR_ASSET}: expected "
            f"{REFERENCE_STAR_OBJ_TRIANGLE_COUNT} triangles, "
            f"got {len(triangles)}"
        )
    if max(max(triangle) for triangle in triangles) >= len(positions):
        raise ValueError(f"{REFERENCE_STAR_ASSET}: face index out of range")

    # faceNormals=true in the Mitsuba scene. Omitting vertex normals gives RBC
    # geometric face normals without splitting the shared adjacency vertices.
    return _build_mesh(positions, None, triangles, [0])


def _make_render_entity(
    scene: rbce.world.Scene,
    name: str,
    mesh: rbce.world.MeshResource,
    materials: list[rbce.world.MaterialResource],
) -> rbce.world.Entity:
    entity = scene.add_entity()
    entity.set_name(name)
    transform = rbce.world.TransformComponent(
        entity.add_component("TransformComponent")
    )
    transform.set_pos(lc.double3(0.0, 0.0, 0.0), False)
    transform.set_rotation(lc.float4(0.0, 0.0, 0.0, 1.0), False)

    material_vector = lc.capsule_vector()
    for material in materials:
        material_vector.emplace_back(material._handle)
    render = rbce.world.RenderComponent(entity.add_component("RenderComponent"))
    render.update_object(material_vector, mesh)
    return entity


def _look_at_rotation(
    position: tuple[float, float, float],
    target: tuple[float, float, float],
) -> tuple[float, float, float, float]:
    """Return a quaternion that maps RoboCute's local +Z to target."""

    direction = tuple(target[i] - position[i] for i in range(3))
    horizontal = math.hypot(direction[0], direction[2])
    yaw = math.atan2(direction[0], direction[2])
    pitch = math.atan2(-direction[1], horizontal)
    sin_yaw = math.sin(yaw * 0.5)
    cos_yaw = math.cos(yaw * 0.5)
    sin_pitch = math.sin(pitch * 0.5)
    cos_pitch = math.cos(pitch * 0.5)
    return (
        cos_yaw * sin_pitch,
        sin_yaw * cos_pitch,
        -sin_yaw * sin_pitch,
        cos_yaw * cos_pitch,
    )


def _make_reference_spot_light(scene: rbce.world.Scene) -> rbce.world.Entity:
    position = _scaled_point(REFERENCE_SPOT_POSITION)
    target = _scaled_point(REFERENCE_SPOT_TARGET)
    source_diameter = _scaled(2.0 * RBC_SPOT_PROXY_RADIUS)

    entity = scene.add_entity()
    entity.set_name("reference_spot_light")
    transform = rbce.world.TransformComponent(
        entity.add_component("TransformComponent")
    )
    transform.set_pos(lc.double3(*position), False)
    transform.set_rotation(
        lc.float4(*_look_at_rotation(position, target)), False
    )
    transform.set_scale(lc.double3(*(source_diameter,) * 3), False)

    light = rbce.world.LightComponent(entity.add_component("LightComponent"))
    light.add_spot_light(
        lc.float3(*(RBC_SPOT_RADIANCE,) * 3),
        math.radians(2.0 * REFERENCE_SPOT_CUTOFF_DEGREES),
        math.radians(2.0 * REFERENCE_SPOT_INNER_DEGREES),
        SPOT_ANGLE_ATTENUATION_POWER,
        False,
    )
    return entity


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", choices=("dx", "vk"), default="dx")
    parser.add_argument("--resolution", type=_parse_resolution, default=(768, 768))
    parser.add_argument("--spp", type=int, default=256)
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "build" / "fsd_cornell.png",
    )
    parser.add_argument(
        "--fsd-weight",
        type=_unit_interval,
        default=REFERENCE_FSD_SCALE,
        help="physical FSD scale; use 0 for a diffuse control render",
    )
    parser.add_argument(
        "--view",
        choices=("receiver", "reference", "screen-edge"),
        default="receiver",
        help="receiver pattern crop, paper composition, or BSDF edge diagnostic",
    )
    parser.add_argument(
        "--tone-map",
        choices=("reinhard", "none"),
        default="reinhard",
        help="PNG mapping; none preserves RBC's direct linear-to-8-bit save",
    )
    parser.add_argument(
        "--linear-output",
        type=Path,
        help="optional .npy output containing the unclipped linear RGB image",
    )
    parser.add_argument("--window", action="store_true")
    args = parser.parse_args()
    if args.spp <= 0:
        parser.error("--spp must be positive")

    world_path = ROOT / "build" / "fsd_cornell_world"
    world_path.mkdir(parents=True, exist_ok=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.linear_output is not None:
        args.linear_output.parent.mkdir(parents=True, exist_ok=True)

    app = rbc.app.App()
    app.init(
        backend_name=args.backend,
        project_path=None,
        world_path=world_path,
        require_render=False,
    )
    app._scene = rbce.world.Scene()

    gray_floor = _material(
        {"type": "pbr", "base_albedo": [0.05, 0.05, 0.05]}
    )
    gray_ceiling = _material(
        {"type": "pbr", "base_albedo": [0.05, 0.05, 0.05]}
    )
    gray_back = _material(
        {"type": "pbr", "base_albedo": [0.05, 0.05, 0.05]}
    )
    dark_left = _material(
        {"type": "pbr", "base_albedo": [0.10, 0.10, 0.10]}
    )
    bright_right = _material(
        {"type": "pbr", "base_albedo": [0.50, 0.50, 0.50]}
    )
    fsd_screen = _material(
        {
            "type": "pbr",
            "weight_base": 1.0,
            "base_albedo": [0.10, 0.10, 0.10],
            "specular_roughness": 1.0,
            "weight_free_space_diffraction": args.fsd_weight,
        }
    )

    room_mesh = _build_cornell_room()
    screen_mesh = _build_star_aperture_screen()
    _make_render_entity(
        app.scene,
        "cornell_room",
        room_mesh,
        [gray_floor, gray_ceiling, gray_back, dark_left, bright_right],
    )
    _make_render_entity(app.scene, "star_aperture_screen", screen_mesh, [fsd_screen])
    _make_reference_spot_light(app.scene)

    app.init_render()
    app.init_display(
        args.resolution[0],
        args.resolution[1],
        display_title="RoboCute FSD Cornell",
        create_window=args.window,
        window_resizable=args.window,
    )
    if not args.window:
        app._tick_stage = rbce.world.TickStage.OffineCapturing

    settings = app.display_cam.render_settings()
    settings.set_offline_origin_bounce(4)
    settings.set_offline_indirect_bounce(7)
    settings.set_sky_color(lc.float3(0.0, 0.0, 0.0))
    settings.set_sun_intensity(0.0)

    reference_camera_position, reference_camera_target, camera_fov_degrees = (
        _camera_config(args.view)
    )
    camera_position = _scaled_point(reference_camera_position)
    camera_target = _scaled_point(reference_camera_target)
    camera = app.get_display_transform()
    camera.set_pos(lc.double3(*camera_position), False)
    camera.set_rotation(
        lc.float4(*_look_at_rotation(camera_position, camera_target)), False
    )
    app.display_cam.set_fov(math.radians(camera_fov_degrees))
    app.display_cam.set_near_plane(_scaled(REFERENCE_CAMERA_NEAR_PLANE))
    app.display_cam.set_far_plane(_scaled(REFERENCE_CAMERA_FAR_PLANE))

    saved = False

    def finish_headless_render() -> None:
        nonlocal saved
        if not args.window and not saved and app.frame_index >= args.spp:
            _save_display_outputs(
                app,
                args.output.resolve(),
                args.resolution,
                args.linear_output.resolve()
                if args.linear_output is not None
                else None,
                args.tone_map,
            )
            print(
                f"Saved {args.output.resolve()} at {app.frame_index} spp "
                f"(FSD weight {args.fsd_weight:g})"
            )
            saved = True
            app.call_exit()

    print(
        "FSD Cornell scale: "
        f"{WAVELENGTH_GEOMETRY_SCALE:g} "
        f"({REFERENCE_WAVELENGTH_METERS * 1.0e6:g} um -> "
        f"{VISIBLE_REFERENCE_WAVELENGTH_METERS * 1.0e9:g} nm)"
    )
    print(
        f"View: {args.view}, vertical FOV {camera_fov_degrees:.6f} deg; "
        f"spot proxy radius {RBC_SPOT_PROXY_RADIUS:.9g} reference units, "
        f"angular radius {math.degrees(RBC_SPOT_PROXY_ANGULAR_RADIUS):.6f} deg"
    )
    app.set_user_callback(finish_headless_render)
    if args.window:
        app.ctx.enable_camera_control()
        app.run()
    else:
        app.run(limit_frame=args.spp + 180)
        if not saved:
            raise RuntimeError("render stopped before reaching the requested spp")


if __name__ == "__main__":
    main()
