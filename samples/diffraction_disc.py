"""Render a compact-disc scene with RoboCute's diffraction material."""

from __future__ import annotations

import argparse
import json
import math
import shutil
import time
from pathlib import Path

import numpy as np

import robocute as rbc
import robocute.rbc_ext as re
import robocute.rbc_ext.luisa as lc
from mesh_builder import MeshBuilder


ROOT = Path(__file__).resolve().parents[1]
DIFFRACTION_TYPES = {
    "sinusoidal": 0,
    "rectangular": 1,
    "linear": 2,
}
MATERIAL_MODES = ("lite", "full", "auto")


def _normalized(value: np.ndarray) -> np.ndarray:
    return value / np.linalg.norm(value)


def _material(description: dict[str, object]) -> re.world.MaterialResource:
    material = re.world.MaterialResource()
    material.load_from_json(json.dumps(description))
    return material


def _disc_material_description(
    diffraction_type: str,
    lobe_count: int,
    full: bool,
) -> dict[str, object]:
    description: dict[str, object] = {
        "type": "pbr",
        "weight_base": 1.0,
        "weight_specular": 1.0,
        "weight_metallic": 1.0,
        "base_albedo": [0.91, 0.92, 0.93],
        "specular_color": [1.0, 1.0, 1.0],
        "specular_roughness": 0.0,
    }
    if full:
        description.update(
            {
                "weight_diffraction": 1.0,
                "weight_coat": 1.0,
                "coat_color": [1.0, 1.0, 1.0],
                "coat_roughness": 0.0,
                "coat_ior": 1.55,
                "diffraction_color": [1.0, 1.0, 1.0],
                "diffraction_thickness": 0.065,
                "diffraction_inv_pitch_x": 0.625,
                "diffraction_inv_pitch_y": 0.0,
                "diffraction_angle": 0.0,
                "diffraction_lobe_count": lobe_count,
                "diffraction_type": DIFFRACTION_TYPES[diffraction_type],
            }
        )
    return description


def _make_entity(
    scene: re.world.Scene,
    name: str,
    mesh: re.world.MeshResource,
    materials: list[re.world.MaterialResource],
) -> re.world.Entity:
    entity = scene.add_entity()
    entity.set_name(name)
    transform = re.world.TransformComponent(
        entity.add_component("TransformComponent")
    )
    transform.set_pos(lc.double3(0.0, 0.0, 0.0), False)
    transform.set_rotation(lc.float4(0.0, 0.0, 0.0, 1.0), False)

    material_vector = lc.capsule_vector()
    for material in materials:
        material_vector.emplace_back(material._handle)
    render = re.world.RenderComponent(entity.add_component("RenderComponent"))
    render.update_object(material_vector, mesh)
    return entity


def _build_disc(
    center: tuple[float, float, float],
    normal: tuple[float, float, float],
    outer_radius: float,
    segments: int = 512,
) -> re.world.MeshResource:
    center_vector = np.asarray(center, dtype=np.float64)
    normal_vector = _normalized(np.asarray(normal, dtype=np.float64))
    up = np.array([0.0, 1.0, 0.0], dtype=np.float64)
    axis_u = _normalized(-np.cross(up, normal_vector))
    axis_v = np.cross(normal_vector, axis_u)

    hole_radius = outer_radius * 0.125
    label_radius = outer_radius * 0.26
    # A 120 mm CD is 1.2 mm thick: half-thickness / radius = 0.01.
    half_thickness = outer_radius * 0.01

    positions: list[np.ndarray] = []
    normals: list[np.ndarray] = []
    tangents: list[np.ndarray] = []

    def add_ring(
        radius: float,
        normal_offset: float,
        normal_kind: str,
    ) -> int:
        start = len(positions)
        for index in range(segments):
            phi = 2.0 * math.pi * index / segments
            radial = math.cos(phi) * axis_u + math.sin(phi) * axis_v
            tangent = radial
            if normal_kind == "front":
                vertex_normal = normal_vector
            elif normal_kind == "back":
                vertex_normal = -normal_vector
            elif normal_kind == "outer":
                vertex_normal = radial
                tangent = -math.sin(phi) * axis_u + math.cos(phi) * axis_v
            else:
                vertex_normal = -radial
                tangent = -math.sin(phi) * axis_u + math.cos(phi) * axis_v
            positions.append(
                center_vector
                + radius * radial
                + normal_offset * normal_vector
            )
            normals.append(vertex_normal)
            tangents.append(np.append(tangent, 1.0))
        return start

    front_hole = add_ring(hole_radius, half_thickness, "front")
    front_label = add_ring(label_radius, half_thickness, "front")
    front_outer = add_ring(outer_radius, half_thickness, "front")
    back_hole = add_ring(hole_radius, -half_thickness, "back")
    back_label = add_ring(label_radius, -half_thickness, "back")
    back_outer = add_ring(outer_radius, -half_thickness, "back")
    side_outer_front = add_ring(outer_radius, half_thickness, "outer")
    side_outer_back = add_ring(outer_radius, -half_thickness, "outer")
    side_inner_front = add_ring(hole_radius, half_thickness, "inner")
    side_inner_back = add_ring(hole_radius, -half_thickness, "inner")

    diffraction_triangles: list[tuple[int, int, int]] = []
    metal_triangles: list[tuple[int, int, int]] = []

    def front_annulus(
        triangles: list[tuple[int, int, int]], inner: int, outer: int
    ) -> None:
        for index in range(segments):
            next_index = (index + 1) % segments
            triangles.extend(
                [
                    (outer + index, outer + next_index, inner + index),
                    (outer + next_index, inner + next_index, inner + index),
                ]
            )

    def back_annulus(inner: int, outer: int) -> None:
        for index in range(segments):
            next_index = (index + 1) % segments
            metal_triangles.extend(
                [
                    (outer + index, inner + index, outer + next_index),
                    (outer + next_index, inner + index, inner + next_index),
                ]
            )

    front_annulus(diffraction_triangles, front_label, front_outer)
    front_annulus(metal_triangles, front_hole, front_label)
    back_annulus(back_hole, back_label)
    back_annulus(back_label, back_outer)

    for index in range(segments):
        next_index = (index + 1) % segments
        metal_triangles.extend(
            [
                (
                    side_outer_front + index,
                    side_outer_back + index,
                    side_outer_front + next_index,
                ),
                (
                    side_outer_front + next_index,
                    side_outer_back + index,
                    side_outer_back + next_index,
                ),
                (
                    side_inner_front + index,
                    side_inner_front + next_index,
                    side_inner_back + index,
                ),
                (
                    side_inner_front + next_index,
                    side_inner_back + next_index,
                    side_inner_back + index,
                ),
            ]
        )

    all_triangles = diffraction_triangles + metal_triangles
    builder = MeshBuilder(
        vertex_count=len(positions),
        triangle_count=len(all_triangles),
        submesh_offsets=np.array(
            [0, len(diffraction_triangles)], dtype=np.uint32
        ),
        has_normal=True,
        has_tangent=True,
    )
    builder.set_positions(np.asarray(positions, dtype=np.float32))
    builder.set_normals(np.asarray(normals, dtype=np.float32))
    builder.set_tangents(np.asarray(tangents, dtype=np.float32))
    builder.triangle_indices[:] = np.asarray(
        all_triangles, dtype=np.uint32
    ).reshape(-1)
    mesh = builder.get_mesh()
    mesh.install()
    return mesh


def _build_studio() -> re.world.MeshResource:
    positions: list[np.ndarray] = []
    submeshes: list[list[tuple[int, int, int]]] = [
        [],
        [],
        [],
        [],
        [],
        [],
    ]

    def add_quad(
        submesh: int,
        center: tuple[float, float, float],
        axis_u: tuple[float, float, float],
        axis_v: tuple[float, float, float],
    ) -> None:
        base = len(positions)
        c = np.asarray(center, dtype=np.float32)
        u = np.asarray(axis_u, dtype=np.float32)
        v = np.asarray(axis_v, dtype=np.float32)
        positions.extend([c - u - v, c + u - v, c - u + v, c + u + v])
        submeshes[submesh].extend(
            [(base, base + 1, base + 2), (base + 1, base + 3, base + 2)]
        )

    # Warm tabletop and background, matching the PLTFalcor CD composition.
    add_quad(0, (0.0, 0.55, 7.0), (-7.0, 0.0, 0.0), (0.0, 4.5, 0.0))
    add_quad(0, (0.0, -1.34, 4.0), (7.0, 0.0, 0.0), (0.0, 0.0, -5.5))

    # A dark jewel case sits behind both discs. The inset is offset toward the
    # camera to avoid coplanar overlap with the outer case plate.
    add_quad(4, (-0.62, -0.08, 6.68), (-1.42, -0.08, -0.04), (-0.06, 0.94, 0.03))
    add_quad(5, (-0.62, -0.08, 6.64), (-1.31, -0.07, -0.035), (-0.05, 0.84, 0.025))

    # A warm key, cool fill, and neutral top light all sit in the front
    # hemisphere and point obliquely toward the main disc.
    add_quad(1, (-2.8, 1.8, 1.4), (0.886, 0.0, -1.084), (0.257, 0.779, 0.21))
    add_quad(2, (2.8, 1.3, 1.6), (0.848, 0.0, 0.848), (-0.143, 0.722, 0.143))
    add_quad(3, (0.0, 3.0, 2.2), (1.5, 0.0, 0.0), (0.0, 0.424, 0.68))
    # A broad softbox behind the camera fills directions that the three
    # off-axis lights cannot cover on a perfectly smooth reflector.
    add_quad(3, (-0.45, 1.2, -0.65), (3.8, 0.0, 0.0), (0.0, 2.2, 0.0))

    triangles: list[tuple[int, int, int]] = []
    offsets: list[int] = []
    for submesh in submeshes:
        offsets.append(len(triangles))
        triangles.extend(submesh)

    builder = MeshBuilder(
        vertex_count=len(positions),
        triangle_count=len(triangles),
        submesh_offsets=np.asarray(offsets, dtype=np.uint32),
    )
    builder.set_positions(np.asarray(positions, dtype=np.float32))
    builder.triangle_indices[:] = np.asarray(
        triangles, dtype=np.uint32
    ).reshape(-1)
    mesh = builder.get_mesh()
    mesh.install()
    return mesh


def _parse_resolution(value: str) -> tuple[int, int]:
    try:
        width, height = (int(part) for part in value.lower().split("x", 1))
    except ValueError as error:
        raise argparse.ArgumentTypeError("resolution must look like 1280x960") from error
    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError("resolution dimensions must be positive")
    return width, height


class _DiscControlPanel:
    def __init__(self, selected_mode: str, active_mode: str, cycle_seconds: float):
        import tkinter as tk
        from tkinter import ttk

        self._tk = tk
        self._root = tk.Tk()
        self._root.title("RoboCute Disc Controls")
        self._root.geometry("390x220")
        self._root.minsize(390, 220)
        self._root.protocol("WM_DELETE_WINDOW", self._request_close)

        self._mode = tk.StringVar(value=selected_mode)
        self._cycle_seconds = tk.DoubleVar(value=cycle_seconds)
        self._status = tk.StringVar(value="")
        self._last_selection = selected_mode
        self._auto_mode = active_mode
        self._last_cycle = time.monotonic()
        self._last_poll = 0.0
        self._last_status_update = 0.0
        self._save_requested = False
        self._close_requested = False
        self._closed = False

        style = ttk.Style(self._root)
        if "vista" in style.theme_names():
            style.theme_use("vista")

        content = ttk.Frame(self._root, padding=16)
        content.pack(fill="both", expand=True)

        ttk.Label(content, text="Disc material (shader follows automatically)").pack(
            anchor="w"
        )
        modes = ttk.Frame(content)
        modes.pack(fill="x", pady=(6, 14))
        for label, value in (
            ("Simple PBR", "lite"),
            ("Diffraction PBR", "full"),
            ("Auto Cycle", "auto"),
        ):
            ttk.Radiobutton(
                modes,
                text=label,
                value=value,
                variable=self._mode,
            ).pack(side="left", padx=(0, 12))

        ttk.Label(content, text="Cycle interval").pack(anchor="w")
        ttk.Scale(
            content,
            from_=1.0,
            to=12.0,
            orient="horizontal",
            variable=self._cycle_seconds,
        ).pack(fill="x", pady=(4, 12))

        footer = ttk.Frame(content)
        footer.pack(fill="x")
        ttk.Label(footer, textvariable=self._status).pack(side="left")
        ttk.Button(
            footer,
            text="Save Frame",
            command=self._request_save,
        ).pack(side="right")

    def _request_save(self) -> None:
        self._save_requested = True

    def _request_close(self) -> None:
        self._close_requested = True
        self.close()

    @property
    def close_requested(self) -> bool:
        return self._close_requested

    def consume_save_request(self) -> bool:
        requested = self._save_requested
        self._save_requested = False
        return requested

    def poll(self, now: float) -> str | None:
        if self._closed:
            return None
        if now - self._last_poll < 1.0 / 60.0:
            return None
        self._last_poll = now
        try:
            self._root.update_idletasks()
            self._root.update()
        except self._tk.TclError:
            self._close_requested = True
            self._closed = True
            return None

        selection = self._mode.get()
        if selection != self._last_selection:
            self._last_selection = selection
            self._last_cycle = now
            if selection != "auto":
                self._auto_mode = selection
                return selection

        if selection == "auto" and now - self._last_cycle >= max(
            1.0, self._cycle_seconds.get()
        ):
            self._last_cycle = now
            self._auto_mode = "lite" if self._auto_mode == "full" else "full"
            return self._auto_mode
        return None

    def update_status(self, active_mode: str, spp: int, now: float) -> None:
        if not self._closed and now - self._last_status_update >= 0.1:
            self._last_status_update = now
            self._status.set(f"{active_mode.title()} shader  |  {spp} spp")

    def close(self) -> None:
        if self._closed:
            return
        self._closed = True
        try:
            self._root.destroy()
        except self._tk.TclError:
            pass


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", choices=("dx", "vk"), default="dx")
    parser.add_argument(
        "--diffraction-type",
        choices=tuple(DIFFRACTION_TYPES),
        default="rectangular",
    )
    parser.add_argument("--resolution", type=_parse_resolution, default=(1280, 720))
    parser.add_argument("--spp", type=int, default=96)
    parser.add_argument("--lobe-count", type=int, choices=range(1, 8), default=3)
    parser.add_argument(
        "--envmap",
        type=Path,
        default=ROOT / "samples" / "assets" / "studio_small_08_1k.exr",
    )
    parser.add_argument(
        "--no-envmap",
        action="store_true",
        help="use the built-in studio lights without an environment texture",
    )
    parser.add_argument("--sky-angle", type=float, default=240.0)
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "docs" / "design" / "images" / "diffraction_disc.png",
    )
    parser.add_argument(
        "--window",
        action="store_true",
        help="open the interactive render and material control windows",
    )
    parser.add_argument(
        "--material-mode",
        choices=MATERIAL_MODES,
        help="disc material; windowed controls default to auto",
    )
    parser.add_argument(
        "--cycle-seconds",
        type=float,
        default=4.0,
        help="seconds between material changes in auto mode",
    )
    parser.add_argument(
        "--no-controls",
        action="store_true",
        help="open only the render window",
    )
    parser.add_argument(
        "--max-frames",
        type=int,
        help="stop after this many frames",
    )
    args = parser.parse_args()
    if args.spp <= 0:
        parser.error("--spp must be positive")
    if args.cycle_seconds <= 0.0:
        parser.error("--cycle-seconds must be positive")
    if args.max_frames is not None and args.max_frames <= 0:
        parser.error("--max-frames must be positive")
    selected_material_mode = args.material_mode or (
        "auto" if args.window and not args.no_controls else "full"
    )
    if selected_material_mode == "auto" and (
        not args.window or args.no_controls
    ):
        parser.error("--material-mode auto requires --window with controls")
    active_material_mode = (
        "full" if selected_material_mode == "auto" else selected_material_mode
    )

    world_path = ROOT / "build" / "diffraction_disc_world"
    world_path.mkdir(parents=True, exist_ok=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    envmap_path = args.envmap.resolve()
    if not args.no_envmap and not envmap_path.is_file():
        parser.error(f"environment map does not exist: {envmap_path}")

    app = rbc.app.App()
    app.init(backend_name=args.backend, project_path=None, world_path=world_path)
    app._scene = re.world.Scene()
    app.init_display(
        args.resolution[0],
        args.resolution[1],
        display_title="RoboCute Diffraction Disc",
        create_window=args.window,
        window_resizable=args.window,
    )
    render_settings = app.display_cam.render_settings()
    render_settings.set_offline_origin_bounce(4)
    render_settings.set_offline_indirect_bounce(8)
    render_settings.set_sky_angle(args.sky_angle)

    if not args.no_envmap:
        environment_cache = world_path / "environment"
        environment_cache.mkdir(parents=True, exist_ok=True)
        cached_envmap = environment_cache / envmap_path.name
        shutil.copy2(envmap_path, cached_envmap)
        environment_project = re.world.Project()
        environment_project.init(str(environment_cache))
        environment_texture = environment_project.import_texture(
            cached_envmap.name, 1, False
        )
        if not environment_texture:
            raise RuntimeError(
                f"failed to import environment map: {envmap_path}"
            )
        environment_texture.set_skybox()

    camera_pitch = math.radians(26.0)
    camera_transform = app.get_display_transform()
    camera_transform.set_pos(lc.double3(0.05, 1.5, 0.3), False)
    camera_transform.set_rotation(
        lc.float4(
            math.sin(camera_pitch * 0.5),
            0.0,
            0.0,
            math.cos(camera_pitch * 0.5),
        ),
        False,
    )
    app.display_cam.set_fov(math.radians(44.0))
    app.display_cam.set_near_plane(0.05)
    app.display_cam.set_far_plane(30.0)

    disc_material_descriptions = {
        mode: _disc_material_description(
            args.diffraction_type,
            args.lobe_count,
            full=mode == "full",
        )
        for mode in ("lite", "full")
    }
    diffraction_material = _material(
        disc_material_descriptions[active_material_mode]
    )
    edge_material = _material(
        {
            "type": "pbr",
            "weight_metallic": 0.96,
            "base_albedo": [0.055, 0.052, 0.047],
            "specular_color": [0.7, 0.69, 0.66],
            "specular_roughness": 0.0,
        }
    )
    studio_materials = [
        _material(
            {
                "type": "pbr",
                "base_albedo": [0.15, 0.12, 0.075],
                "specular_roughness": 0.68,
            }
        ),
        _material(
            {
                "type": "pbr",
                "base_albedo": [0.0, 0.0, 0.0],
                "emission_luminance": [18.0, 12.5, 7.75],
            }
        ),
        _material(
            {
                "type": "pbr",
                "base_albedo": [0.0, 0.0, 0.0],
                "emission_luminance": [7.0, 9.75, 15.5],
            }
        ),
        _material(
            {
                "type": "pbr",
                "base_albedo": [0.0, 0.0, 0.0],
                "emission_luminance": [8.75, 8.5, 8.0],
            }
        ),
        _material(
            {
                "type": "pbr",
                "weight_metallic": 0.75,
                "base_albedo": [0.07, 0.065, 0.06],
                "specular_color": [0.52, 0.48, 0.4],
                "specular_roughness": 0.12,
            }
        ),
        _material(
            {
                "type": "pbr",
                "weight_metallic": 0.25,
                "base_albedo": [0.012, 0.014, 0.018],
                "specular_color": [0.32, 0.34, 0.38],
                "specular_roughness": 0.055,
            }
        ),
    ]

    main_disc_mesh = _build_disc(
        center=(0.55, -0.52, 4.4),
        normal=(-0.38, 0.78, -0.5),
        outer_radius=1.28,
    )
    # This upright disc normal bisects its view and main-disc directions so the
    # two polished surfaces can appear in each other's reflection paths.
    secondary_disc_mesh = _build_disc(
        center=(-1.2, -0.0634, 4.05),
        normal=(0.8708, 0.0840, -0.4845),
        outer_radius=1.28,
    )
    studio_mesh = _build_studio()
    _make_entity(
        app.scene,
        "main_diffraction_disc",
        main_disc_mesh,
        [diffraction_material, edge_material],
    )
    _make_entity(
        app.scene,
        "secondary_diffraction_disc",
        secondary_disc_mesh,
        [diffraction_material, edge_material],
    )
    _make_entity(app.scene, "studio", studio_mesh, studio_materials)

    saved = False
    control_panel = None

    def save_frame() -> None:
        nonlocal saved
        app.ctx.save_display_image_to(str(args.output.resolve()))
        print(
            f"Saved {args.output.resolve()} at {app.frame_index} spp "
            f"({active_material_mode}, {args.diffraction_type})"
        )
        saved = True

    def update_interactive_scene() -> bool | None:
        nonlocal active_material_mode
        if control_panel is not None:
            now = time.monotonic()
            requested_mode = control_panel.poll(now)
            if control_panel.close_requested:
                app.call_exit()
            if control_panel.consume_save_request():
                save_frame()
            material_changed = (
                requested_mode is not None
                and requested_mode != active_material_mode
            )
            if material_changed:
                active_material_mode = requested_mode
                diffraction_material.load_from_json(
                    json.dumps(disc_material_descriptions[active_material_mode])
                )
                diffraction_material.install()
                print(f"Disc material: {active_material_mode}")
            control_panel.update_status(active_material_mode, app.frame_index, now)
            if material_changed:
                app.display_cam.set_frame_index(0)
                return False
            return None

        if not saved and app.frame_index >= args.spp:
            save_frame()
            if not args.window:
                app.call_exit()
        return None

    try:
        if args.window and not args.no_controls:
            control_panel = _DiscControlPanel(
                selected_material_mode,
                active_material_mode,
                args.cycle_seconds,
            )

        app.set_user_callback(update_interactive_scene)
        if args.window:
            app.ctx.enable_camera_control()
            app.ctx.control_camera_add_rotate(0.0, -camera_pitch, 0.0)
            controls_hint = (
                " Material controls are in the companion window."
                if control_panel is not None
                else ""
            )
            print(
                "Interactive camera: click the render window, then keep RMB held "
                "while dragging or pressing WASD/QE."
                f"{controls_hint}"
            )
            app.run(limit_frame=args.max_frames)
        else:
            app.run(limit_frame=args.max_frames or args.spp + 180)
    except KeyboardInterrupt:
        pass
    finally:
        if control_panel is not None:
            control_panel.close()
    if not saved and not args.window:
        raise RuntimeError("render stopped before reaching the requested sample count")


if __name__ == "__main__":
    main()
