"""Non-interactive debug driver for samples/app_graphics_scene.py.

Reproduces the exact scene setup of app_graphics_scene.py, but:
  - never blocks on ``input()`` (the blocking call that leaves the
    freshly-created window un-painted / un-pumped in the original sample)
  - no TUI / cli executor threads
  - optionally headless (create_window=False)
  - renders a fixed number of frames, saves the display image, then exits

Usage:
  uv run python samples/debug_scene_noninteractive.py -p <project> -b dx \
      [--headless] [-n frames] [-o out.png] [--verbose-tick]
"""
import argparse
import faulthandler
import sys
import time
from pathlib import Path

# mesh_builder / mat_builtin / app_graphics_scene live in samples/
sys.path.insert(0, str(Path(__file__).parent))

import robocute as rbc
import robocute.rbc_ext as rbce
import robocute.rbc_ext.luisa as lc

import app_graphics_scene as demo  # noqa: E402  (reuses make_cube_mesh)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-b", "--backend", type=str, default="dx")
    parser.add_argument("-p", "--project", type=str, required=True)
    parser.add_argument("-n", "--frames", type=int, default=64)
    parser.add_argument(
        "-o",
        "--out",
        type=str,
        default=str(Path(__file__).parent / "screenshot" / "debug_noninteractive.png"),
    )
    parser.add_argument("--headless", action="store_true")
    parser.add_argument(
        "--hang-timeout",
        type=float,
        default=180.0,
        help="dump python stacks and abort if run() takes longer than this",
    )
    parser.add_argument("--verbose-tick", action="store_true")
    parser.add_argument(
        "--stage",
        type=str,
        default="PathTracingPreview",
        help="tick stage: PathTracingPreview / RasterPreview / OffineCapturing / PresentOfflineResult",
    )
    args = parser.parse_args()

    faulthandler.enable()
    faulthandler.dump_traceback_later(args.hang_timeout, exit=True)

    app = rbc.app.App()
    app.init(project_path=Path(args.project), backend_name=args.backend)
    print("init done", flush=True)

    tex = app._project.import_texture("test_grid.png", 1, False)
    print("tex", tex.size(), flush=True)
    assert app.ctx, "Context not valid"

    resolution = lc.uint2(960, 540)
    app.init_display(
        resolution.x, resolution.y, create_window=not args.headless
    )
    assert app.display_cam, "Display cam not valid"
    print(f"display ready (window={not args.headless})", flush=True)

    transform = app.get_display_transform()
    if transform:
        transform.set_pos(lc.double3(0, 0, -1), False)

    demo.app = app  # make_cube_mesh uses the module-global ``app``
    entity = demo.make_cube_mesh(app.scene, tex=tex)
    print("cube mesh created", flush=True)

    if app._window_created:
        app.ctx.enable_camera_control()

    def tick():
        if args.verbose_tick and app.real_frame_index % 10 == 0:
            print(f"frame {app.real_frame_index} (accum {app.frame_index})", flush=True)
        return None

    app._tick_stage = getattr(rbce.world.TickStage, args.stage)
    app.set_user_callback(tick)
    t0 = time.time()
    app.run(limit_frame=args.frames)
    print(f"run done in {time.time() - t0:.2f}s", flush=True)

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    app.ctx.save_display_image_to(str(out))
    print("saved", out, flush=True)


if __name__ == "__main__":
    main()
