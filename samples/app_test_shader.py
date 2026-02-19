import os
import time
from pathlib import Path
import numpy as np
import math
import argparse

import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-b",
        "--backend",
        type=str,
        default="dx",
        help="graphics backend api type, dx/vk",
    )
    parser.add_argument(
        "-p",
        "--project",
        type=str,
        help="rbc project path, the directory containing rbc_project.json",
        required=True,
    )
    args = parser.parse_args()

    project_path = Path(args.project)
    app = rbc.app.App()  # rbc app singleton
    app.init(project_path, False)
    if not app.ctx:
        print("Context not Valid!")
        return
    clear_shader = lc.Shader('gui/clear_shader.bin')
    # Create an Image2D with 4 channels (RGBA), float dtype
    image_2d = lc.Image2D.empty(512, 512, 4, float)
    # Call clear_shader with the image and a magenta color (1, 0, 1, 1)
    clear_shader(image_2d, lc.float4(1, 0, 1, 1), dispatch_size=(512, 512, 1))
    lc.execute()
    lc.synchronize()

if __name__ == "__main__":
    main()
