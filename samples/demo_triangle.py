"""Minimal triangle rendering demo.

Renders a single RGB-interpolated triangle with a compute shader
(``rbc/shader/src/demo/triangle.cpp``) and saves the result as a PNG.

Usage:
    uv run python samples/demo_triangle.py -b dx -o samples/screenshot/demo_triangle.png
"""

import argparse
from pathlib import Path

import numpy as np
from PIL import Image

import robocute as rbc
import robocute.rbc_ext.luisa as lc


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-b", "--backend", type=str, default="dx", help="graphics backend api type, dx/vk"
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        default=str(Path(__file__).parent / "screenshot" / "demo_triangle.png"),
        help="output png file path",
    )
    parser.add_argument("--width", type=int, default=512)
    parser.add_argument("--height", type=int, default=512)
    args = parser.parse_args()

    app = rbc.app.App()  # rbc app singleton
    app.init(args.backend, None, ".")
    if not app.ctx:
        print("Context not Valid!")
        return

    # Load compiled shader (lazy) and dispatch once
    shader = lc.Shader("demo/triangle.bin")
    width, height = args.width, args.height
    img = lc.Image2D.empty(width, height, 4, float)
    shader(img, dispatch_size=(width, height))

    # Read back and save
    arr = np.empty((height, width, 4), dtype=np.float32)
    img.copy_to(arr)
    rgb = np.clip(arr[:, :, :3] * 255.0, 0, 255).astype(np.uint8)
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgb, mode="RGB").save(out_path)
    print(f"Triangle rendered -> {out_path}")


if __name__ == "__main__":
    main()
