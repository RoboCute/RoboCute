"""
RoboCute Stub Generation Tool

This script generates .pyi stub files for compiled .pyd modules using nanobind-stubgen.

Usage:
    uv run generate_stub.py
    uv run generate_stub.py --module test_py_codegen --output src/rbc_ext
"""

import sys
import argparse
from pathlib import Path


def generate_stub(module_name: str, pyd_dir: Path, output_dir: Path):
    """
    Generate .pyi stub file for a given .pyd module.

    Args:
        module_name: Name of the module (without .pyd extension)
        pyd_dir: Directory containing the .pyd file
        output_dir: Directory where .pyi file should be generated
    """
    # Add pyd directory to Python path
    pyd_dir_abs = pyd_dir.resolve()
    if str(pyd_dir_abs) not in sys.path:
        sys.path.insert(0, str(pyd_dir_abs))

    print(f"Generating stub for module: {module_name}")
    print(f"  .pyd location: {pyd_dir_abs}")
    print(f"  Output directory: {output_dir.resolve()}")

    # Verify module can be imported
    try:
        __import__(module_name)
        print(f"  ✓ Module {module_name} imported successfully")
    except ImportError as e:
        print(f"  ✗ Error: Cannot import module {module_name}")
        print(f"    {e}")
        sys.exit(1)

    # Generate stub using nanobind-stubgen
    try:
        from nanobind_stubgen.__main__ import main as stubgen_main

        # Prepare arguments for stubgen
        original_argv = sys.argv.copy()
        sys.argv = ["nanobind-stubgen", module_name, "--out", str(output_dir)]

        print(f"  Running: nanobind-stubgen {module_name} --out {output_dir}")
        stubgen_main()

        sys.argv = original_argv
        print(f"  ✓ Stub file generated: {output_dir / f'{module_name}.pyi'}")

    except Exception as e:
        print(f"  ✗ Error generating stub: {e}")
        sys.exit(1)


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Generate .pyi stub files for RoboCute .pyd modules"
    )
    parser.add_argument(
        "--module",
        default="test_py_codegen",
        help="Module name (default: test_py_codegen)",
    )
    parser.add_argument(
        "--pyd-dir",
        type=Path,
        default=Path("src/rbc_ext/release"),
        help="Directory containing .pyd file (default: src/rbc_ext/release)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("src/rbc_ext/generated"),
        help="Output directory for .pyi file (default: src/rbc_ext)",
    )

    args = parser.parse_args()

    # Ensure output directory exists
    args.output.mkdir(parents=True, exist_ok=True)

    generate_stub(args.module, args.pyd_dir, args.output)
    print("\n✓ Stub generation completed successfully!")


if __name__ == "__main__":
    main()
