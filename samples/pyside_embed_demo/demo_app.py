"""Simple launcher for the PySide6 + LuisaCompute embedding demo."""

from __future__ import annotations

import argparse
import sys

from main_window import run_demo


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Run the single-process PySide6 + LuisaCompute viewport demo."
    )
    parser.add_argument(
        "--program-path",
        default=None,
        help="Directory used by LuisaCompute to locate runtime resources "
             "(defaults to RBC_BUILTIN_RUNTIME env var or src/robocute/rbc_ext/_C).",
    )
    parser.add_argument(
        "--backend",
        default="dx",
        choices=["dx", "d3d12", "vk", "vulkan", "metal"],
        help="RHI backend used by the LC viewport widget.",
    )
    args = parser.parse_args(argv)
    return run_demo(program_path=args.program_path, backend=args.backend, argv=argv)


if __name__ == "__main__":
    sys.exit(main())
