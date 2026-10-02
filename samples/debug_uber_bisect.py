"""Bisect the white-output uber post filter.

Renders with PathTracingPreview while tweaking render settings that feed the
uber shader (auto exposure on/off, global exposure, ACES knobs) and reports
image stats for each configuration. Each config runs in a fresh subprocess
(single-init per process).
"""
import argparse
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

CONFIGS = {
    "baseline": "",
    "no_auto_exposure": "rs.set_use_auto_exposure(False)",
    "no_auto_exp_half": "rs.set_use_auto_exposure(False); rs.set_global_exposure(0.5)",
    "no_auto_exp_quarter": "rs.set_use_auto_exposure(False); rs.set_global_exposure(0.25)",
    "gamma_only": "rs.set_use_auto_exposure(False); rs.set_global_exposure(0.5); rs.set_use_linear_sdr(False)",
}

SNIPPET = """
import sys
from pathlib import Path
sys.path.insert(0, {samples!r})
import numpy as np
import robocute as rbc
import robocute.rbc_ext as rbce
import robocute.rbc_ext.luisa as lc
import app_graphics_scene as demo

app = rbc.app.App()
app.init(project_path=Path(r"D:\\ws\\repos\\RoboCute-repo\\rbc-project-default"), backend_name="dx")
tex = app._project.import_texture("test_grid.png", 1, False)
res = lc.uint2(960, 540)
app.init_display(res.x, res.y, create_window=False)
transform = app.get_display_transform()
if transform:
    transform.set_pos(lc.double3(0, 0, -1), False)
demo.app = app
demo.make_cube_mesh(app.scene, tex=tex)
rs = app.display_cam.render_settings()
{setup}
app.set_user_callback(lambda: None)
app.run(limit_frame=64)
img = app.display_image()
arr = np.empty((img.height, img.width, img.channel), dtype=np.float32)
img.copy_to(arr)
rgb = arr[..., :3]
print("RESULT mean={{:.3f}} min={{:.3f}} max={{:.3f}}".format(rgb.mean(), rgb.min(), rgb.max()))
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-n", "--frames", type=int, default=64)
    args = parser.parse_args()
    samples = str(Path(__file__).parent)

    for name, setup in CONFIGS.items():
        code = SNIPPET.format(samples=samples, setup=setup)
        r = subprocess.run(
            [sys.executable, "-c", code],
            capture_output=True,
            text=True,
            cwd=REPO,
            timeout=180,
        )
        result = [ln for ln in r.stdout.splitlines() if ln.startswith("RESULT")]
        err = [ln for ln in (r.stderr or "").splitlines()
               if "error" in ln.lower() or "assert" in ln.lower()]
        print(f"{name}: {result[-1] if result else 'NO RESULT'} "
              f"(rc={r.returncode}) {err[:1]}", flush=True)


if __name__ == "__main__":
    main()
