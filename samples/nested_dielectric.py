"""Render focused nested-dielectric regression scenes.

``gallery`` compares priority order, equal-priority nesting, overlapping
boundaries, absorption, IOR, and dispersion in one frame. The focused scenes
keep the same tests larger, while ``camera-inside`` isolates initial-medium
probing; compare the default with ``--no-probe-initial-medium``.
"""

from __future__ import annotations

import argparse
import json
import math
import shutil
from pathlib import Path

import numpy as np

import robocute as rbc
import robocute.rbc_ext as rbce
import robocute.rbc_ext.luisa as lc
from mesh_builder import MeshBuilder
from robocute.project import ProjectConfigSchema


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ENVMAP = ROOT / "samples" / "assets" / "studio_small_08_1k.exr"
SCENARIOS = (
    "gallery",
    "priority",
    "equal",
    "intersecting",
    "optics",
    "drink",
    "camera-inside",
)

# Radius at bottom, radius at top, bottom Y, top Y. The liquid profile is
# derived from these values so the contact geometry cannot silently drift.
TUMBLER_OUTER_PROFILE = (1.12, 1.34, -1.55, 1.55)
TUMBLER_INNER_PROFILE = (0.98, 1.18, -1.30, 1.55)
DRINK_GLASS_OVERLAP = 0.01


def _parse_resolution(value: str) -> tuple[int, int]:
    try:
        width, height = (int(part) for part in value.lower().split("x", 1))
    except ValueError as error:
        raise argparse.ArgumentTypeError("expected WIDTHxHEIGHT") from error
    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError("resolution must be positive")
    return width, height


def _profile_radius(
    profile: tuple[float, float, float, float], y: float
) -> float:
    bottom_radius, top_radius, bottom_y, top_y = profile
    t = (y - bottom_y) / (top_y - bottom_y)
    return bottom_radius + t * (top_radius - bottom_radius)


def _material(description: dict[str, object]) -> rbce.world.MaterialResource:
    resource = rbce.world.MaterialResource()
    resource.load_from_json(json.dumps(description))
    return resource


def _glass(
    ior: float,
    color: tuple[float, float, float],
    depth: float,
    priority: int,
    *,
    roughness: float = 0.015,
    dispersion_scale: float = 0.0,
    abbe_number: float = 20.0,
) -> rbce.world.MaterialResource:
    return _material(
        {
            "type": "pbr",
            "weight_base": 1.0,
            "weight_specular": 1.0,
            "weight_transmission": 1.0,
            "base_albedo": [1.0, 1.0, 1.0],
            "specular_color": [1.0, 1.0, 1.0],
            "specular_roughness": roughness,
            "specular_ior": ior,
            "geometry_thin_walled": False,
            "geometry_nested_priority": priority,
            "transmission_color": list(color),
            "transmission_depth": depth,
            "transmission_scatter": [0.0, 0.0, 0.0],
            "transmission_dispersion_scale": dispersion_scale,
            "transmission_dispersion_abbe_number": abbe_number,
        }
    )


def _sphere(rings: int = 32, sectors: int = 64) -> rbce.world.MeshResource:
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    triangles: list[tuple[int, int, int]] = []

    for ring in range(rings + 1):
        phi = math.pi * ring / rings
        radius = math.sin(phi)
        y = math.cos(phi)
        for sector in range(sectors + 1):
            theta = 2.0 * math.pi * sector / sectors
            normal = (radius * math.cos(theta), y, radius * math.sin(theta))
            positions.append(normal)
            normals.append(normal)

    stride = sectors + 1
    for ring in range(rings):
        for sector in range(sectors):
            upper = ring * stride + sector
            lower = upper + stride
            if ring != 0:
                triangles.append((upper, upper + 1, lower))
            if ring != rings - 1:
                triangles.append((upper + 1, lower + 1, lower))

    builder = MeshBuilder(
        vertex_count=len(positions),
        triangle_count=len(triangles),
        submesh_offsets=np.array([0], dtype=np.uint32),
        has_normal=True,
    )
    builder.set_positions(np.asarray(positions, dtype=np.float32))
    builder.set_normals(np.asarray(normals, dtype=np.float32))
    builder.set_triangles(np.asarray(triangles, dtype=np.uint32))
    mesh = builder.get_mesh()
    mesh.install()
    return mesh


def _quad_xy() -> rbce.world.MeshResource:
    builder = MeshBuilder(
        vertex_count=4,
        triangle_count=2,
        submesh_offsets=np.array([0], dtype=np.uint32),
        has_normal=True,
    )
    builder.set_positions(
        np.asarray(
            [(-0.5, -0.5, 0.0), (0.5, -0.5, 0.0),
             (-0.5, 0.5, 0.0), (0.5, 0.5, 0.0)],
            dtype=np.float32,
        )
    )
    builder.set_normals(
        np.asarray([(0.0, 0.0, -1.0)] * 4, dtype=np.float32)
    )
    builder.set_triangles(np.asarray([(0, 2, 1), (1, 2, 3)], dtype=np.uint32))
    mesh = builder.get_mesh()
    mesh.install()
    return mesh


def _checked_mesh(
    positions: list[tuple[float, float, float]],
    normals: list[tuple[float, float, float]],
    triangles: list[tuple[int, int, int]],
) -> rbce.world.MeshResource:
    position_array = np.asarray(positions, dtype=np.float32)
    normal_array = np.asarray(normals, dtype=np.float32)
    triangle_array = np.asarray(triangles, dtype=np.uint32)
    edges_a = position_array[triangle_array[:, 1]] - position_array[
        triangle_array[:, 0]
    ]
    edges_b = position_array[triangle_array[:, 2]] - position_array[
        triangle_array[:, 0]
    ]
    geometric_normals = np.cross(edges_a, edges_b)
    declared_normals = normal_array[triangle_array].sum(axis=1)
    orientation = np.einsum("ij,ij->i", geometric_normals, declared_normals)
    if np.any(orientation <= 1.0e-8):
        index = int(np.flatnonzero(orientation <= 1.0e-8)[0])
        raise ValueError(f"triangle {index} has inward or degenerate winding")

    builder = MeshBuilder(
        vertex_count=len(positions),
        triangle_count=len(triangles),
        submesh_offsets=np.array([0], dtype=np.uint32),
        has_normal=True,
    )
    builder.set_positions(position_array)
    builder.set_normals(normal_array)
    builder.set_triangles(triangle_array)
    mesh = builder.get_mesh()
    mesh.install()
    return mesh


def _closed_frustum(
    bottom_radius: float,
    top_radius: float,
    bottom_y: float,
    top_y: float,
    sectors: int = 96,
) -> rbce.world.MeshResource:
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    triangles: list[tuple[int, int, int]] = []
    slope = (top_radius - bottom_radius) / (top_y - bottom_y)
    normal_scale = 1.0 / math.sqrt(1.0 + slope * slope)

    bottom_ring: list[int] = []
    top_ring: list[int] = []
    for radius, y, ring in (
        (bottom_radius, bottom_y, bottom_ring),
        (top_radius, top_y, top_ring),
    ):
        for sector in range(sectors):
            theta = 2.0 * math.pi * sector / sectors
            cosine, sine = math.cos(theta), math.sin(theta)
            ring.append(len(positions))
            positions.append((radius * cosine, y, radius * sine))
            normals.append(
                (
                    cosine * normal_scale,
                    -slope * normal_scale,
                    sine * normal_scale,
                )
            )

    for sector in range(sectors):
        next_sector = (sector + 1) % sectors
        triangles.append(
            (bottom_ring[sector], top_ring[sector], bottom_ring[next_sector])
        )
        triangles.append(
            (bottom_ring[next_sector], top_ring[sector], top_ring[next_sector])
        )

    for y, radius, upward in (
        (bottom_y, bottom_radius, False),
        (top_y, top_radius, True),
    ):
        normal = (0.0, 1.0 if upward else -1.0, 0.0)
        center = len(positions)
        positions.append((0.0, y, 0.0))
        normals.append(normal)
        ring: list[int] = []
        for sector in range(sectors):
            theta = 2.0 * math.pi * sector / sectors
            ring.append(len(positions))
            positions.append((radius * math.cos(theta), y, radius * math.sin(theta)))
            normals.append(normal)
        for sector in range(sectors):
            next_sector = (sector + 1) % sectors
            triangle = (
                (center, ring[next_sector], ring[sector])
                if upward
                else (center, ring[sector], ring[next_sector])
            )
            triangles.append(triangle)

    return _checked_mesh(positions, normals, triangles)


def _tumbler_shell(sectors: int = 128) -> rbce.world.MeshResource:
    outer_bottom_radius, outer_top_radius, outer_bottom_y, outer_top_y = (
        TUMBLER_OUTER_PROFILE
    )
    inner_bottom_radius, inner_top_radius, inner_bottom_y, inner_top_y = (
        TUMBLER_INNER_PROFILE
    )
    if outer_top_y != inner_top_y:
        raise ValueError("tumbler wall profiles must share a top plane")
    top_y = outer_top_y
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    triangles: list[tuple[int, int, int]] = []

    def wall_ring(
        radius: float,
        y: float,
        slope: float,
        inward: bool,
    ) -> list[int]:
        ring: list[int] = []
        sign = -1.0 if inward else 1.0
        normal_scale = 1.0 / math.sqrt(1.0 + slope * slope)
        for sector in range(sectors):
            theta = 2.0 * math.pi * sector / sectors
            cosine, sine = math.cos(theta), math.sin(theta)
            ring.append(len(positions))
            positions.append((radius * cosine, y, radius * sine))
            normals.append(
                (
                    sign * cosine * normal_scale,
                    sign * -slope * normal_scale,
                    sign * sine * normal_scale,
                )
            )
        return ring

    outer_slope = (outer_top_radius - outer_bottom_radius) / (
        top_y - outer_bottom_y
    )
    inner_slope = (inner_top_radius - inner_bottom_radius) / (
        top_y - inner_bottom_y
    )
    outer_bottom = wall_ring(
        outer_bottom_radius, outer_bottom_y, outer_slope, False
    )
    outer_top = wall_ring(outer_top_radius, top_y, outer_slope, False)
    inner_bottom = wall_ring(
        inner_bottom_radius, inner_bottom_y, inner_slope, True
    )
    inner_top = wall_ring(inner_top_radius, top_y, inner_slope, True)

    for sector in range(sectors):
        next_sector = (sector + 1) % sectors
        triangles.extend(
            (
                (outer_bottom[sector], outer_top[sector], outer_bottom[next_sector]),
                (outer_bottom[next_sector], outer_top[sector], outer_top[next_sector]),
                (inner_bottom[sector], inner_bottom[next_sector], inner_top[sector]),
                (inner_bottom[next_sector], inner_top[next_sector], inner_top[sector]),
            )
        )

    # The underside is a full disk; the cavity floor is a separate +Y disk.
    for y, radius, upward in (
        (outer_bottom_y, outer_bottom_radius, False),
        (inner_bottom_y, inner_bottom_radius, True),
    ):
        normal = (0.0, 1.0 if upward else -1.0, 0.0)
        center = len(positions)
        positions.append((0.0, y, 0.0))
        normals.append(normal)
        ring: list[int] = []
        for sector in range(sectors):
            theta = 2.0 * math.pi * sector / sectors
            ring.append(len(positions))
            positions.append((radius * math.cos(theta), y, radius * math.sin(theta)))
            normals.append(normal)
        for sector in range(sectors):
            next_sector = (sector + 1) % sectors
            triangles.append(
                (center, ring[next_sector], ring[sector])
                if upward
                else (center, ring[sector], ring[next_sector])
            )

    # The top annulus closes the glass medium between the two wall surfaces.
    rim_normal = (0.0, 1.0, 0.0)
    rim_inner: list[int] = []
    rim_outer: list[int] = []
    for sector in range(sectors):
        theta = 2.0 * math.pi * sector / sectors
        cosine, sine = math.cos(theta), math.sin(theta)
        rim_inner.append(len(positions))
        positions.append((inner_top_radius * cosine, top_y, inner_top_radius * sine))
        normals.append(rim_normal)
        rim_outer.append(len(positions))
        positions.append((outer_top_radius * cosine, top_y, outer_top_radius * sine))
        normals.append(rim_normal)
    for sector in range(sectors):
        next_sector = (sector + 1) % sectors
        triangles.extend(
            (
                (rim_inner[sector], rim_inner[next_sector], rim_outer[sector]),
                (rim_inner[next_sector], rim_outer[next_sector], rim_outer[sector]),
            )
        )

    return _checked_mesh(positions, normals, triangles)


def _rounded_box(subdivisions: int = 8) -> rbce.world.MeshResource:
    half_extent = 0.5
    radius = 0.13
    core_extent = half_extent - radius
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    triangles: list[tuple[int, int, int]] = []
    faces = (
        ((half_extent, 0.0, 0.0), (0.0, half_extent, 0.0), (0.0, 0.0, half_extent)),
        ((-half_extent, 0.0, 0.0), (0.0, half_extent, 0.0), (0.0, 0.0, -half_extent)),
        ((0.0, half_extent, 0.0), (0.0, 0.0, half_extent), (half_extent, 0.0, 0.0)),
        ((0.0, -half_extent, 0.0), (0.0, 0.0, half_extent), (-half_extent, 0.0, 0.0)),
        ((0.0, 0.0, half_extent), (half_extent, 0.0, 0.0), (0.0, half_extent, 0.0)),
        ((0.0, 0.0, -half_extent), (half_extent, 0.0, 0.0), (0.0, -half_extent, 0.0)),
    )

    for center, axis_u, axis_v in faces:
        base = len(positions)
        for v_index in range(subdivisions + 1):
            v = 2.0 * v_index / subdivisions - 1.0
            for u_index in range(subdivisions + 1):
                u = 2.0 * u_index / subdivisions - 1.0
                nominal = np.asarray(center, dtype=np.float64)
                nominal += u * np.asarray(axis_u, dtype=np.float64)
                nominal += v * np.asarray(axis_v, dtype=np.float64)
                core = np.clip(nominal, -core_extent, core_extent)
                delta = nominal - core
                normal = delta / np.linalg.norm(delta)
                rounded = core + radius * normal
                positions.append(tuple(float(value) for value in rounded))
                normals.append(tuple(float(value) for value in normal))
        stride = subdivisions + 1
        for v_index in range(subdivisions):
            for u_index in range(subdivisions):
                lower = base + v_index * stride + u_index
                upper = lower + stride
                triangles.extend(
                    (
                        (lower, lower + 1, upper),
                        (lower + 1, upper + 1, upper),
                    )
                )

    return _checked_mesh(positions, normals, triangles)


def _axis_angle(
    axis: tuple[float, float, float], angle: float
) -> tuple[float, float, float, float]:
    length = math.sqrt(sum(component * component for component in axis))
    sine = math.sin(angle * 0.5) / length
    return (
        axis[0] * sine,
        axis[1] * sine,
        axis[2] * sine,
        math.cos(angle * 0.5),
    )


def _look_at_rotation(
    position: tuple[float, float, float],
    target: tuple[float, float, float],
) -> tuple[float, float, float, float]:
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


def _entity(
    scene: rbce.world.Scene,
    name: str,
    mesh: rbce.world.MeshResource,
    material: rbce.world.MaterialResource,
    position: tuple[float, float, float],
    scale: tuple[float, float, float],
    rotation: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0),
) -> rbce.world.Entity:
    entity = scene.add_entity()
    entity.set_name(name)
    transform = rbce.world.TransformComponent(
        entity.add_component("TransformComponent")
    )
    transform.set_pos(lc.double3(*position), False)
    transform.set_rotation(lc.float4(*rotation), False)
    transform.set_scale(lc.double3(*scale), False)

    materials = lc.capsule_vector()
    materials.emplace_back(material._handle)
    render = rbce.world.RenderComponent(entity.add_component("RenderComponent"))
    render.update_object(materials, mesh)
    return entity


def _nested_pair(
    scene: rbce.world.Scene,
    sphere: rbce.world.MeshResource,
    name: str,
    center: tuple[float, float, float],
    radius: float,
    outer: rbce.world.MaterialResource,
    inner: rbce.world.MaterialResource,
    inner_offset: tuple[float, float, float] = (0.12, 0.04, -0.08),
) -> None:
    _entity(scene, f"{name}_outer", sphere, outer, center, (radius,) * 3)
    _entity(
        scene,
        f"{name}_inner",
        sphere,
        inner,
        tuple(center[i] + inner_offset[i] * radius for i in range(3)),
        (radius * 0.58,) * 3,
    )


def _overlapping_pair(
    scene: rbce.world.Scene,
    sphere: rbce.world.MeshResource,
    name: str,
    center: tuple[float, float, float],
    radius: float,
    left: rbce.world.MaterialResource,
    right: rbce.world.MaterialResource,
) -> None:
    # Neither sphere contains the other. Rays exercise enter A, enter B,
    # exit A, exit B as well as the reverse order across the silhouette.
    offset = radius * 0.42
    scale = (radius * 0.78, radius, radius * 0.82)
    _entity(
        scene,
        f"{name}_left",
        sphere,
        left,
        (center[0] - offset, center[1], center[2] - 0.08 * radius),
        scale,
    )
    _entity(
        scene,
        f"{name}_right",
        sphere,
        right,
        (center[0] + offset, center[1], center[2] + 0.08 * radius),
        scale,
    )


def _reference_stripes(
    scene: rbce.world.Scene,
    panel: rbce.world.MeshResource,
    *,
    z: float,
    width: float,
    height: float,
    count: int,
    brightness: float = 1.0,
    center_x: float = 0.0,
) -> None:
    colors = (
        (9.0, 0.3, 0.18),
        (0.18, 6.0, 9.0),
        (9.0, 7.5, 0.25),
        (7.5, 7.5, 7.5),
    )
    materials = [
        _material(
            {
                "type": "pbr",
                "base_albedo": [0.0, 0.0, 0.0],
                "emission_luminance": [
                    component * brightness for component in color
                ],
            }
        )
        for color in colors
    ]
    stripe_width = width / count
    for index in range(count):
        x = center_x + (index - 0.5 * (count - 1)) * stripe_width
        _entity(
            scene,
            f"reference_stripe_{index}",
            panel,
            materials[index % len(materials)],
            (x, 0.0, z),
            (stripe_width * 0.82, height, 1.0),
        )


def _material_set() -> dict[str, rbce.world.MaterialResource]:
    return {
        "outer_low": _glass(1.33, (0.78, 0.94, 1.0), 3.0, priority=0),
        "outer_high": _glass(1.33, (0.78, 0.94, 1.0), 3.0, priority=3),
        "inner_high": _glass(1.68, (1.0, 0.48, 0.08), 0.75, priority=3),
        "inner_low": _glass(1.68, (1.0, 0.48, 0.08), 0.75, priority=0),
        "equal_outer": _glass(1.28, (0.55, 0.86, 1.0), 1.8, priority=1),
        "equal_inner": _glass(1.62, (1.0, 0.42, 0.65), 0.65, priority=1),
        "overlap_low": _glass(1.31, (0.35, 1.0, 0.58), 1.2, priority=-2),
        "overlap_high": _glass(1.56, (0.85, 0.42, 1.0), 0.9, priority=2),
        "water": _glass(1.333, (0.86, 0.97, 1.0), 5.0, priority=0),
        "flint": _glass(
            1.72,
            (1.0, 1.0, 1.0),
            8.0,
            priority=3,
            dispersion_scale=1.0,
            abbe_number=18.0,
        ),
        "amber": _glass(1.48, (1.0, 0.28, 0.035), 0.48, priority=0),
        "bubble": _glass(1.01, (1.0, 1.0, 1.0), 20.0, priority=3),
        "ice": _glass(
            1.52,
            (0.7, 0.92, 1.0),
            2.8,
            priority=0,
            roughness=0.035,
            dispersion_scale=0.8,
            abbe_number=32.0,
        ),
        "tumbler_glass": _glass(
            1.5,
            (0.98, 0.995, 1.0),
            12.0,
            priority=1,
            roughness=0.015,
            dispersion_scale=0.12,
            abbe_number=52.0,
        ),
        "drink_liquid": _glass(
            1.333,
            (1.0, 0.45, 0.06),
            5.0,
            priority=0,
            roughness=0.012,
        ),
        "drink_ice": _glass(
            1.31,
            (0.93, 0.98, 1.0),
            8.0,
            priority=2,
            roughness=0.14,
            dispersion_scale=0.08,
            abbe_number=42.0,
        ),
        "drink_air": _glass(
            1.0003,
            (1.0, 1.0, 1.0),
            50.0,
            priority=3,
        ),
        "table": _material(
            {
                "type": "pbr",
                "base_albedo": [0.38, 0.40, 0.43],
                "specular_roughness": 0.42,
                "emission_luminance": [0.08, 0.09, 0.11],
            }
        ),
        "drink_backdrop": _material(
            {
                "type": "pbr",
                "base_albedo": [0.66, 0.69, 0.74],
                "specular_roughness": 0.75,
                "emission_luminance": [0.58, 0.62, 0.70],
            }
        ),
        "drink_backdrop_warm": _material(
            {
                "type": "pbr",
                "base_albedo": [0.68, 0.38, 0.26],
                "specular_roughness": 0.8,
                "emission_luminance": [0.48, 0.25, 0.17],
            }
        ),
        "drink_backdrop_cool": _material(
            {
                "type": "pbr",
                "base_albedo": [0.25, 0.49, 0.64],
                "specular_roughness": 0.8,
                "emission_luminance": [0.17, 0.37, 0.56],
            }
        ),
        "drink_key_light": _material(
            {
                "type": "pbr",
                "base_albedo": [0.0, 0.0, 0.0],
                "emission_luminance": [8.0, 8.0, 8.0],
            }
        ),
        "drink_fill_light": _material(
            {
                "type": "pbr",
                "base_albedo": [0.0, 0.0, 0.0],
                "emission_luminance": [3.0, 3.0, 3.0],
            }
        ),
        "drink_rim_light": _material(
            {
                "type": "pbr",
                "base_albedo": [0.0, 0.0, 0.0],
                "emission_luminance": [5.0, 5.0, 5.0],
            }
        ),
        "camera_medium": _glass(
            1.43,
            (0.3, 0.78, 1.0),
            1.35,
            priority=0,
            dispersion_scale=0.35,
            abbe_number=35.0,
        ),
        "camera_inner": _glass(
            1.66,
            (1.0, 0.3, 0.06),
            0.6,
            priority=3,
            dispersion_scale=0.65,
            abbe_number=22.0,
        ),
    }


def _build_scene(
    scene: rbce.world.Scene,
    sphere: rbce.world.MeshResource,
    panel: rbce.world.MeshResource,
    scenario: str,
) -> tuple[tuple[float, float, float], float]:
    material = _material_set()

    if scenario == "gallery":
        # Top row: both priority orders, equal-priority LIFO, then overlap.
        _nested_pair(
            scene, sphere, "inner_wins", (-3.0, 1.05, 6.5), 0.72,
            material["outer_low"], material["inner_high"],
        )
        _nested_pair(
            scene, sphere, "outer_wins", (-1.0, 1.05, 6.5), 0.72,
            material["outer_high"], material["inner_low"],
        )
        _nested_pair(
            scene, sphere, "equal_priority", (1.0, 1.05, 6.5), 0.72,
            material["equal_outer"], material["equal_inner"],
            inner_offset=(0.25, -0.08, -0.12),
        )
        _overlapping_pair(
            scene, sphere, "intersecting", (3.0, 1.05, 6.5), 0.78,
            material["overlap_low"], material["overlap_high"],
        )

        # Bottom row: low/high IOR with dispersion, absorption with an air
        # pocket, and a rough dispersive ice-like ellipsoid.
        _nested_pair(
            scene, sphere, "water_and_flint", (-2.2, -1.05, 6.4), 0.8,
            material["water"], material["flint"],
        )
        _nested_pair(
            scene, sphere, "amber_and_bubble", (0.0, -1.05, 6.4), 0.8,
            material["amber"], material["bubble"],
            inner_offset=(-0.22, 0.08, -0.1),
        )
        _entity(
            scene,
            "dispersive_ice",
            sphere,
            material["ice"],
            (2.45, -1.05, 6.4),
            (0.62, 0.92, 0.48),
        )
        _reference_stripes(scene, panel, z=9.3, width=9.0, height=5.4, count=13)
        return (0.0, 0.0, 0.0), math.radians(48.0)

    if scenario == "priority":
        _nested_pair(
            scene, sphere, "inner_wins", (-1.35, 0.0, 4.8), 1.05,
            material["outer_low"], material["inner_high"],
        )
        _nested_pair(
            scene, sphere, "outer_wins", (1.35, 0.0, 4.8), 1.05,
            material["outer_high"], material["inner_low"],
        )
    elif scenario == "equal":
        _nested_pair(
            scene, sphere, "equal_priority", (0.0, 0.0, 4.8), 1.3,
            material["equal_outer"], material["equal_inner"],
            inner_offset=(0.28, -0.12, -0.16),
        )
    elif scenario == "intersecting":
        _overlapping_pair(
            scene, sphere, "intersecting", (0.0, 0.0, 4.8), 1.35,
            material["overlap_low"], material["overlap_high"],
        )
    elif scenario == "optics":
        _nested_pair(
            scene, sphere, "water_and_flint", (-1.75, 0.0, 5.1), 1.05,
            material["water"], material["flint"],
        )
        _nested_pair(
            scene, sphere, "amber_and_bubble", (0.75, 0.0, 5.1), 1.05,
            material["amber"], material["bubble"],
            inner_offset=(-0.22, 0.08, -0.1),
        )
        _entity(
            scene,
            "dispersive_ice",
            sphere,
            material["ice"],
            (2.45, 0.0, 5.1),
            (0.62, 1.18, 0.5),
        )
    elif scenario == "drink":
        tumbler = _tumbler_shell()
        liquid_bottom_y = TUMBLER_INNER_PROFILE[2] - DRINK_GLASS_OVERLAP
        liquid_top_y = 0.72
        liquid_bottom_radius = (
            _profile_radius(TUMBLER_INNER_PROFILE, liquid_bottom_y)
            + DRINK_GLASS_OVERLAP
        )
        liquid_top_radius = (
            _profile_radius(TUMBLER_INNER_PROFILE, liquid_top_y)
            + DRINK_GLASS_OVERLAP
        )
        for y, radius in (
            (liquid_bottom_y, liquid_bottom_radius),
            (liquid_top_y, liquid_top_radius),
        ):
            if not (
                _profile_radius(TUMBLER_INNER_PROFILE, y)
                < radius
                < _profile_radius(TUMBLER_OUTER_PROFILE, y)
            ):
                raise ValueError("liquid side must overlap only the glass wall")
        if not (
            TUMBLER_OUTER_PROFILE[2]
            < liquid_bottom_y
            < TUMBLER_INNER_PROFILE[2]
        ):
            raise ValueError("liquid bottom must overlap only the glass base")

        # Deliberately overlap the closed liquid volume with the glass. Glass
        # has the higher priority, so the embedded liquid boundary is a false
        # hit and glass-to-liquid refraction occurs on the glass inner wall.
        liquid = _closed_frustum(
            bottom_radius=liquid_bottom_radius,
            top_radius=liquid_top_radius,
            bottom_y=liquid_bottom_y,
            top_y=liquid_top_y,
        )
        ice = _rounded_box()
        _entity(
            scene,
            "drink_tumbler_glass",
            tumbler,
            material["tumbler_glass"],
            (0.0, 0.0, 5.5),
            (1.0, 1.0, 1.0),
        )
        _entity(
            scene,
            "drink_liquid",
            liquid,
            material["drink_liquid"],
            (0.0, 0.0, 5.5),
            (1.0, 1.0, 1.0),
        )

        # Both closed pieces cross the liquid plane. They remain spatially
        # separate so no ray needs more than liquid + one ice medium at once.
        ice_instances = (
            (
                "left",
                (-0.36, 0.72, 5.38),
                (0.44, 0.48, 0.44),
                _axis_angle((0.0, 0.0, 1.0), 0.22),
            ),
            (
                "right",
                (0.32, 0.77, 5.54),
                (0.48, 0.44, 0.46),
                _axis_angle((0.0, 0.0, 1.0), -0.32),
            ),
        )
        for name, position, scale, rotation in ice_instances:
            _entity(
                scene,
                f"drink_ice_{name}",
                ice,
                material["drink_ice"],
                position,
                scale,
                rotation,
            )

        # This bubble is wholly above the water plane, avoiding a third active
        # medium while still testing an ice-to-air nested boundary.
        _entity(
            scene,
            "drink_ice_air_bubble",
            sphere,
            material["drink_air"],
            (-0.36, 0.87, 5.38),
            (0.06, 0.06, 0.06),
        )
        _entity(
            scene,
            "drink_table",
            panel,
            material["table"],
            (0.0, -1.57, 5.5),
            (7.0, 5.0, 1.0),
            _axis_angle((1.0, 0.0, 0.0), math.pi * 0.5),
        )
        _entity(
            scene,
            "drink_backdrop",
            panel,
            material["drink_backdrop"],
            (0.0, 0.15, 9.25),
            (20.0, 7.0, 1.0),
        )
        backdrop_bands = (
            ("warm_left", -2.35, 0.65, material["drink_backdrop_warm"]),
            ("cool_left", -0.82, 0.46, material["drink_backdrop_cool"]),
            ("warm_right", 0.92, 0.52, material["drink_backdrop_warm"]),
            ("cool_right", 2.42, 0.72, material["drink_backdrop_cool"]),
        )
        for name, x, width, band_material in backdrop_bands:
            _entity(
                scene,
                f"drink_backdrop_{name}",
                panel,
                band_material,
                (x, 0.15, 9.20),
                (width, 5.2, 1.0),
            )

        # Large off-camera emitters provide readable front faces and a cool
        # rim without turning the backdrop itself into a clipped white light.
        _entity(
            scene,
            "drink_front_key_light",
            panel,
            material["drink_key_light"],
            (-3.45, 1.65, 3.55),
            (2.0, 2.8, 1.0),
            _axis_angle((0.0, 1.0, 0.0), math.pi),
        )
        _entity(
            scene,
            "drink_front_fill_light",
            panel,
            material["drink_fill_light"],
            (3.75, 0.65, 3.9),
            (1.8, 2.5, 1.0),
            _axis_angle((0.0, 1.0, 0.0), math.pi),
        )
        _entity(
            scene,
            "drink_back_rim_light",
            panel,
            material["drink_rim_light"],
            (0.0, 3.45, 8.0),
            (4.0, 1.0, 1.0),
        )
        return (1.25, 0.9, 0.0), math.radians(38.0)
    elif scenario == "camera-inside":
        # The camera starts off-center inside this absorbing volume. The inner
        # solid forces a nested transition before the enclosing volume exits.
        _entity(
            scene,
            "camera_containing_medium",
            sphere,
            material["camera_medium"],
            (0.8, 0.0, 3.0),
            (4.2, 3.7, 4.2),
        )
        _entity(
            scene,
            "camera_inside_nested_solid",
            sphere,
            material["camera_inner"],
            (-0.6, -0.1, 3.7),
            (0.82, 1.0, 0.72),
        )
        _reference_stripes(scene, panel, z=8.6, width=7.0, height=5.0, count=11)
        return (0.0, 0.0, 0.0), math.radians(52.0)

    _reference_stripes(scene, panel, z=8.0, width=7.0, height=4.8, count=11)
    return (0.0, 0.0, 0.0), math.radians(45.0)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", choices=("dx", "vk"), default="dx")
    parser.add_argument("--scenario", choices=SCENARIOS, default="gallery")
    parser.add_argument("--resolution", type=_parse_resolution, default=(1152, 768))
    parser.add_argument("--spp", type=int, default=128)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--envmap", type=Path, default=DEFAULT_ENVMAP)
    parser.add_argument(
        "--no-envmap",
        action="store_true",
        help="use the analytic sky and sun only",
    )
    parser.add_argument("--sky-angle", type=float, default=225.0)
    parser.add_argument("--window", action="store_true")
    parser.add_argument(
        "--probe-initial-medium",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="probe media containing the camera (enabled by default)",
    )
    args = parser.parse_args()
    if args.spp <= 0:
        parser.error("--spp must be positive")

    probe_suffix = "" if args.probe_initial_medium else "_no_probe"
    output_path = args.output or (
        ROOT / "build" / f"nested_dielectric_{args.scenario}{probe_suffix}.png"
    )
    world_path = ROOT / "build" / f"nested_dielectric_{args.scenario}_world"
    world_path.mkdir(parents=True, exist_ok=True)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    envmap_path = args.envmap.resolve()
    if not args.no_envmap and not envmap_path.is_file():
        parser.error(f"environment map does not exist: {envmap_path}")

    app = rbc.app.App()
    app.init(
        backend_name=args.backend,
        project_path=None,
        world_path=world_path,
        require_render=False,
    )
    app._scene = rbce.world.Scene()

    sphere = _sphere()
    panel = _quad_xy()
    camera_position, camera_fov = _build_scene(
        app.scene, sphere, panel, args.scenario
    )

    app.init_render()
    app.init_display(
        args.resolution[0],
        args.resolution[1],
        display_title=f"RoboCute Nested Dielectric: {args.scenario}",
        create_window=args.window,
        window_resizable=args.window,
    )
    if not args.window:
        app._tick_stage = rbce.world.TickStage.OffineCapturing

    settings = app.display_cam.render_settings()
    settings.set_offline_origin_bounce(4)
    settings.set_offline_indirect_bounce(8)
    settings.set_probe_initial_medium(args.probe_initial_medium)
    settings.set_sky_angle(args.sky_angle)
    settings.set_sky_color(lc.float3(0.12, 0.15, 0.2))
    settings.set_sun_color(lc.float3(1.0, 0.95, 0.86))
    settings.set_sun_dir(lc.float3(-0.35, -0.8, -0.45))
    settings.set_sun_intensity(2.0)
    settings.set_global_exposure(1.12 if args.scenario == "drink" else 0.85)

    if not args.no_envmap:
        environment_cache = world_path / "environment"
        environment_cache.mkdir(parents=True, exist_ok=True)
        environment_assets = environment_cache / "assets"
        environment_assets.mkdir(parents=True, exist_ok=True)
        ProjectConfigSchema(name="nested_dielectric_environment").save(
            environment_cache
        )
        cached_envmap = environment_assets / envmap_path.name
        shutil.copy2(envmap_path, cached_envmap)
        environment_project = rbce.world.Project()
        environment_project.init(str(environment_cache))
        environment_texture = environment_project.import_texture(
            cached_envmap.name, 1, False
        )
        if not environment_texture:
            raise RuntimeError(f"failed to import environment map: {envmap_path}")
        environment_texture.set_skybox()

    camera = app.get_display_transform()
    camera.set_pos(lc.double3(*camera_position), False)
    camera_rotation = (
        _look_at_rotation(camera_position, (0.0, 0.1, 5.5))
        if args.scenario == "drink"
        else (0.0, 0.0, 0.0, 1.0)
    )
    camera.set_rotation(lc.float4(*camera_rotation), False)
    app.display_cam.set_fov(camera_fov)
    app.display_cam.set_near_plane(0.02)
    app.display_cam.set_far_plane(30.0)

    saved = False

    def finish_headless_render() -> None:
        nonlocal saved
        if not args.window and not saved and app.frame_index >= args.spp:
            app.ctx.save_display_image_to(str(output_path.resolve()))
            print(f"Saved {output_path.resolve()} at {app.frame_index} spp")
            saved = True
            app.call_exit()

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
