from __future__ import annotations

import json
import shutil
import subprocess
from pathlib import Path

import pytest


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
SHADER_CMAKE_MODULE = REPOSITORY_ROOT / "cmake" / "rbc_shader_compile.cmake"


@pytest.mark.skipif(
    any(shutil.which(tool) is None for tool in ("cmake", "ninja", "uv")),
    reason="CMake shader target test requires cmake, ninja, and uv",
)
def test_cmake_shader_targets_follow_manifest_backends(tmp_path: Path) -> None:
    shader_root = tmp_path / "rbc" / "shader"
    shader_root.mkdir(parents=True)
    manifest_path = shader_root / "shader_variants.json"
    manifest_path.write_text(
        json.dumps({"backends": ["mock"]}), encoding="utf-8"
    )
    (tmp_path / "consumer.cpp").write_text("int consumer = 0;\n", encoding="utf-8")
    (tmp_path / "CMakeLists.txt").write_text(
        "\n".join(
            (
                "cmake_minimum_required(VERSION 3.20)",
                "project(shader_manifest_targets LANGUAGES CXX)",
                "add_library(consumer STATIC consumer.cpp)",
                f'include("{SHADER_CMAKE_MODULE.as_posix()}")',
                "rbc_add_shader_hostgen(consumer)",
                "",
            )
        ),
        encoding="utf-8",
    )

    build_dir = tmp_path / "build"
    subprocess.run(
        ["cmake", "-S", str(tmp_path), "-B", str(build_dir), "-G", "Ninja"],
        check=True,
        capture_output=True,
        text=True,
    )
    targets = subprocess.run(
        ["cmake", "--build", str(build_dir), "--target", "help"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout

    assert "rbc_shader_compile_mock" in targets
    assert "rbc_shader_compile_dx" not in targets
    assert "rbc_shader_compile_vk" not in targets
    assert "rbc_shader_hostgen" in targets
    hostgen_graph = subprocess.run(
        ["ninja", "-C", str(build_dir), "-t", "query", "rbc_shader_hostgen"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    assert "rbc_shader_compile_mock" in hostgen_graph

    manifest_path.write_text(
        json.dumps({"backends": ["replacement"]}), encoding="utf-8"
    )
    regenerated_targets = subprocess.run(
        ["cmake", "--build", str(build_dir), "--target", "help"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout

    assert "rbc_shader_compile_replacement" in regenerated_targets
    assert "rbc_shader_compile_mock" not in regenerated_targets
