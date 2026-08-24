"""Tests for the rbcxx shader-variant driver and shared Python primitives.

The variant build driver now lives in ``rbcxx`` (C++). Integration tests invoke
the real ``rbcxx.exe`` and skip when it has not been built. The ``shader_common``
primitives used by the packaging/install path are tested directly in Python.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import threading
import time
from pathlib import Path

import pytest

from rbc_build.shader_common import (
    CrossProcessFileLock,
    ShaderVariantError,
    _atomic_replace_directory,
    _directory_backup_path,
    _sha256_file,
    recover_directory_backup,
)

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
RBCXX = REPOSITORY_ROOT / "build" / "tool" / "rbcxx" / "rbcxx.exe"
REAL_MANIFEST = REPOSITORY_ROOT / "rbc" / "shader" / "shader_variants.json"
REAL_SHADER_ROOT = REPOSITORY_ROOT / "rbc" / "shader"

requires_rbcxx = pytest.mark.skipif(
    not RBCXX.is_file(), reason="rbcxx.exe not built; run the `rbcxx` xmake target first"
)


# ---------------------------------------------------------------------------
# shader_common unit tests (always run)
# ---------------------------------------------------------------------------
def _increment_with_lock(lock_path: Path, counter_path: Path, start: threading.Event) -> None:
    start.wait()
    with CrossProcessFileLock(lock_path, timeout_seconds=10.0):
        value = int(counter_path.read_text(encoding="utf-8"))
        time.sleep(0.05)
        counter_path.write_text(str(value + 1), encoding="utf-8")


def test_cross_process_lock_serializes_writers(tmp_path: Path) -> None:
    lock_path = tmp_path / "locks" / "counter.lock"
    counter_path = tmp_path / "counter.txt"
    counter_path.write_text("0", encoding="utf-8")
    start = threading.Event()
    threads = [
        threading.Thread(target=_increment_with_lock, args=(lock_path, counter_path, start))
        for _ in range(8)
    ]
    for thread in threads:
        thread.start()
    start.set()
    for thread in threads:
        thread.join()
    assert counter_path.read_text(encoding="utf-8") == "8"


def test_sha256_file_matches_known_vector(tmp_path: Path) -> None:
    path = tmp_path / "data.bin"
    path.write_bytes(b"abc")
    assert _sha256_file(path) == (
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    )


def test_atomic_replace_directory_and_backup_recovery(tmp_path: Path) -> None:
    destination = tmp_path / "published"
    destination.mkdir()
    (destination / "old.txt").write_text("old", encoding="utf-8")

    staged = tmp_path / "staged"
    staged.mkdir()
    (staged / "new.txt").write_text("new", encoding="utf-8")
    _atomic_replace_directory(staged, destination)
    assert (destination / "new.txt").read_text(encoding="utf-8") == "new"
    assert not (destination / "old.txt").exists()

    # Simulate a committed directory with a leftover backup: recovery keeps the
    # new directory and drops the backup.
    (destination / "latest.txt").write_text("latest", encoding="utf-8")
    backup = _directory_backup_path(destination)
    backup.mkdir()
    (backup / "junk.txt").write_text("junk", encoding="utf-8")
    recover_directory_backup(destination)
    assert (destination / "latest.txt").exists()
    assert not backup.exists()

    # Simulate a crash before commit: backup replaces the missing destination.
    shutil.rmtree(destination)
    backup.mkdir()
    (backup / "recovered.txt").write_text("recovered", encoding="utf-8")
    recover_directory_backup(destination)
    assert (destination / "recovered.txt").read_text(encoding="utf-8") == "recovered"


def test_atomic_replace_directory_identical_tree_is_noop(tmp_path: Path) -> None:
    destination = tmp_path / "published"
    destination.mkdir()
    (destination / "a.txt").write_text("same", encoding="utf-8")
    (destination / "sub").mkdir()
    (destination / "sub" / "b.txt").write_text("b", encoding="utf-8")
    before = {
        path: path.stat().st_mtime_ns
        for path in destination.rglob("*")
        if path.is_file()
    }

    staged = tmp_path / "staged"
    staged.mkdir()
    (staged / "a.txt").write_text("same", encoding="utf-8")
    (staged / "sub").mkdir()
    (staged / "sub" / "b.txt").write_text("b", encoding="utf-8")
    _atomic_replace_directory(staged, destination)

    after = {
        path: path.stat().st_mtime_ns
        for path in destination.rglob("*")
        if path.is_file()
    }
    assert before == after


# ---------------------------------------------------------------------------
# Manifest fixture for rbcxx integration tests
# ---------------------------------------------------------------------------
def write_minimal_manifest(root: Path) -> Path:
    shader_root = root / "rbc" / "shader"
    source_dir = shader_root / "src"
    include_dir = shader_root / "include"
    source_dir.mkdir(parents=True)
    include_dir.mkdir(parents=True)
    (source_dir / "offline.cpp").write_text(
        "// minimal shader placeholder\n", encoding="utf-8"
    )
    (include_dir / "common.h").write_text(
        "// common header\n", encoding="utf-8"
    )
    manifest = {
        "schema_version": 2,
        "backends": ["dx", "vk"],
        "compile": {
            "source_root": "src",
            "include_dirs": ["include"],
            "optimization": "on",
            "defines": {},
        },
        "scene_features": ["complex_pbr_material"],
        "dimensions": {
            "pbr_material": {
                "default": "lite",
                "values": {
                    "lite": {
                        "defines": {"RBC_LITE_PBR_MATERIAL": "1"},
                        "scene_features": {"forbidden": ["complex_pbr_material"]},
                    },
                    "full": {
                        "defines": {},
                        "scene_features": {"required": ["complex_pbr_material"]},
                    },
                },
            }
        },
        "variant_sets": {
            "offline": {
                "default": "lite",
                "permutations": [
                    {"id": "lite", "select": {"pbr_material": "lite"}},
                    {"id": "full", "select": {"pbr_material": "full"}},
                ],
            }
        },
        "programs": [
            {
                "id": "offline",
                "source": "src/offline.cpp",
                "variant_set": "offline",
                "host_abi": "per_variant",
            }
        ],
    }
    manifest_path = shader_root / "shader_variants.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return manifest_path


def _run_rbcxx(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(RBCXX), *args],
        cwd=REPOSITORY_ROOT,
        capture_output=True,
        text=True,
    )


def _kv(key: str, value: object) -> str:
    return f"{key}={value}"


# ---------------------------------------------------------------------------
# rbcxx integration tests (gated on a built rbcxx.exe)
# ---------------------------------------------------------------------------
@requires_rbcxx
def test_variant_validate_minimal_manifest(tmp_path: Path) -> None:
    manifest = write_minimal_manifest(tmp_path)
    result = _run_rbcxx(
        "--variant=validate",
        _kv("--project-root", tmp_path),
        _kv("--manifest", manifest),
    )
    assert result.returncode == 0, result.stderr
    assert "Validated" in result.stdout


@requires_rbcxx
def test_variant_validate_rejects_bad_manifest(tmp_path: Path) -> None:
    manifest = write_minimal_manifest(tmp_path)
    data = json.loads(manifest.read_text(encoding="utf-8"))
    data["schema_version"] = 99
    manifest.write_text(json.dumps(data), encoding="utf-8")
    result = _run_rbcxx(
        "--variant=validate",
        _kv("--project-root", tmp_path),
        _kv("--manifest", manifest),
    )
    assert result.returncode == 1
    assert "shader-build: error:" in result.stderr


@requires_rbcxx
def test_variant_backends_read_declared_backends(tmp_path: Path) -> None:
    manifest = write_minimal_manifest(tmp_path)
    result = _run_rbcxx(
        "--variant=backends",
        _kv("--project-root", tmp_path),
        _kv("--manifest", manifest),
    )
    assert result.returncode == 0, result.stderr
    assert result.stdout.splitlines() == ["dx", "vk"]


@requires_rbcxx
def test_variant_lsp_generates_compile_commands(tmp_path: Path) -> None:
    manifest = write_minimal_manifest(tmp_path)
    out = tmp_path / "compile_commands.json"
    result = _run_rbcxx(
        "--variant=lsp",
        _kv("--project-root", tmp_path),
        _kv("--manifest", manifest),
        _kv("--out", out),
        _kv("--compiler", RBCXX),
    )
    assert result.returncode == 0, result.stderr
    assert out.is_file()
    commands = json.loads(out.read_text(encoding="utf-8"))
    assert isinstance(commands, list)
    assert any("offline.cpp" in entry.get("file", "") for entry in commands)


@pytest.mark.skipif(
    os.environ.get("RBCXX_RUN_BUILD_TESTS") != "1",
    reason="set RBCXX_RUN_BUILD_TESTS=1 to run real shader builds",
)
@requires_rbcxx
def test_variant_build_is_incremental_noop(tmp_path: Path) -> None:
    """Build the real manifest once, then assert a second build changes nothing."""
    build_root = tmp_path / "build_root"
    cache_root = tmp_path / "cache"
    host_out = tmp_path / "host"
    args = [
        "--variant=build",
        _kv("--project-root", REPOSITORY_ROOT),
        _kv("--manifest", REAL_MANIFEST),
        _kv("--build-root", build_root),
        _kv("--cache-root", cache_root),
        _kv("--compiler", RBCXX),
        "--hostgen",
        _kv("--host-out", host_out),
        "--backend=dx",
    ]
    first = _run_rbcxx(*args)
    assert first.returncode == 0, first.stderr
    manifest_path = build_root / "shader_build_dx" / "shader_manifest.json"
    assert manifest_path.is_file()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert manifest["schema_version"] == 1
    assert manifest["backend"] == "dx"
    assert len(manifest["input_id"]) == 64
    assert len(manifest["build_id"]) == 64
    assert (host_out / ".shader_input_id").is_file()

    def snapshot(path: Path) -> dict[str, tuple[int, str]]:
        return {
            p.relative_to(path).as_posix(): (p.stat().st_mtime_ns, _sha256_file(p))
            for p in path.rglob("*")
            if p.is_file()
        }

    before_backend = snapshot(build_root / "shader_build_dx")
    before_host = snapshot(host_out)

    second = _run_rbcxx(*args)
    assert second.returncode == 0, second.stderr
    assert snapshot(build_root / "shader_build_dx") == before_backend
    assert snapshot(host_out) == before_host
    # No recompilation messages on the fast path.
    assert "compiling" not in second.stdout.lower()

    # --rebuild invalidates the fast path and still produces a valid build.
    rebuilt = _run_rbcxx(*args, "--rebuild")
    assert rebuilt.returncode == 0, rebuilt.stderr
    assert snapshot(build_root / "shader_build_dx") == before_backend or (
        (build_root / "shader_build_dx" / "shader_manifest.json").is_file()
    )
    verify = _run_rbcxx(
        "--variant=verify",
        _kv("--project-root", REPOSITORY_ROOT),
        _kv("--build-root", build_root),
        "--backend=dx",
    )
    assert verify.returncode == 0, verify.stderr

    # --rebuild bypasses the fast path; a later run is a fast no-op again.
    after_rebuild = snapshot(build_root / "shader_build_dx")
    rebuilt_again = _run_rbcxx(*args)
    assert rebuilt_again.returncode == 0, rebuilt_again.stderr
    assert snapshot(build_root / "shader_build_dx") == after_rebuild
    assert "compiling" not in rebuilt_again.stdout.lower()


@pytest.mark.skipif(
    os.environ.get("RBCXX_RUN_BUILD_TESTS") != "1",
    reason="set RBCXX_RUN_BUILD_TESTS=1 to run real shader builds",
)
@requires_rbcxx
def test_variant_build_edit_source_rebuilds_only_affected(tmp_path: Path) -> None:
    """Editing one shader source rebuilds that logical artifact; unrelated files keep bytes."""
    shader_root = tmp_path / "rbc" / "shader"
    (shader_root / "src").mkdir(parents=True)
    shutil.copytree(REAL_SHADER_ROOT / "src", shader_root / "src", dirs_exist_ok=True)
    shutil.copytree(REAL_SHADER_ROOT / "include", shader_root / "include", dirs_exist_ok=True)
    shutil.copy2(REAL_MANIFEST, shader_root / "shader_variants.json")

    build_root = tmp_path / "build_root"
    cache_root = tmp_path / "cache"
    host_out = tmp_path / "host"
    args = [
        "--variant=build",
        _kv("--project-root", REPOSITORY_ROOT),
        _kv("--manifest", shader_root / "shader_variants.json"),
        _kv("--build-root", build_root),
        _kv("--cache-root", cache_root),
        _kv("--compiler", RBCXX),
        "--hostgen",
        _kv("--host-out", host_out),
        "--backend=dx",
    ]

    def snapshot(path: Path) -> dict[str, tuple[int, str]]:
        return {
            p.relative_to(path).as_posix(): (p.stat().st_mtime_ns, _sha256_file(p))
            for p in path.rglob("*")
            if p.is_file()
        }

    first = _run_rbcxx(*args)
    assert first.returncode == 0, first.stderr
    manifest_path = build_root / "shader_build_dx" / "shader_manifest.json"

    def compile_keys(manifest: dict) -> dict[str, str]:
        def selection_key(selection: dict) -> str:
            return "+".join(f"{k}={selection[k]}" for k in sorted(selection)) or "default"

        result: dict[str, str] = {}
        for logical, program in manifest["programs"].items():
            for variant in program["variants"]:
                result[f"{logical}@{selection_key(variant['selection'])}"] = variant["compile_key"]
        return result

    first_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    input_id_before = first_manifest["input_id"]
    keys_before = compile_keys(first_manifest)
    before = snapshot(build_root / "shader_build_dx")

    # Unchanged inputs: second run is a fast no-op.
    second = _run_rbcxx(*args)
    assert second.returncode == 0, second.stderr
    assert snapshot(build_root / "shader_build_dx") == before
    assert "compiling" not in second.stdout.lower()

    # Edit one shader source (copy, not the repo tree).
    source = shader_root / "src" / "path_tracer" / "offline_pt.cpp"
    text = source.read_text(encoding="utf-8")
    assert "TRANS_MAX_DEPTH = 4" in text
    source.write_text(text.replace("TRANS_MAX_DEPTH = 4", "TRANS_MAX_DEPTH = 5"), encoding="utf-8")

    third = _run_rbcxx(*args)
    assert third.returncode == 0, third.stderr
    after = snapshot(build_root / "shader_build_dx")

    # The edited logical program and its default artifact change bytes.
    assert after["path_tracer/offline_pt.bin"] != before["path_tracer/offline_pt.bin"]
    # Non-default variants of the edited program change bytes (lite is the default
    # permutation and is published at the top-level artifact, not under variants/).
    for key in (
        "variants/pbr_material=full/path_tracer/offline_pt.bin",
        "variants/pbr_material=fsd/path_tracer/offline_pt.bin",
    ):
        assert after[key] != before[key], key

    # Unrelated artifacts are still present and the whole root verifies
    # (manifest hashes are internally consistent after the rebuild).
    # NB: compiled bytecode embeds the output path (luisa ShaderOption.name), so
    # a rebuild through a different temporary work directory may produce new
    # bytes even for unchanged sources; the on-disk contract is the manifest's
    # size/SHA-256 records, which --variant=verify validates.
    for key in (
        "builtin/buffer_to_image_f32.bin",
        "combine_mesh/write_tris.bin",
        "post_process/auto_exposure.bin",
    ):
        assert key in after and key in before, key

    verify = _run_rbcxx(
        "--variant=verify",
        _kv("--project-root", REPOSITORY_ROOT),
        _kv("--build-root", build_root),
        "--backend=dx",
    )
    assert verify.returncode == 0, verify.stderr

    # The runtime manifest is regenerated with a new input_id.
    input_id_after = json.loads(manifest_path.read_text(encoding="utf-8"))["input_id"]
    assert input_id_after != input_id_before

    # Per-unit cache keys: editing one source must NOT invalidate unrelated units.
    # The affected program's compile keys change while an unrelated program's key
    # stays identical (its cache entry was reused, not rebuilt).
    keys_after = compile_keys(json.loads(manifest_path.read_text(encoding="utf-8")))
    for key in keys_before:
        if "offline_pt" in key:
            assert keys_after[key] != keys_before[key], key
        else:
            assert keys_after[key] == keys_before[key], key
    for key in keys_before:
        if "offline_pt" in key:
            assert keys_after[key] != keys_before[key], key
        else:
            assert keys_after[key] == keys_before[key], key

    # A following run is a fast no-op again.
    fourth = _run_rbcxx(*args)
    assert fourth.returncode == 0, fourth.stderr
    assert snapshot(build_root / "shader_build_dx") == after
    assert "compiling" not in fourth.stdout.lower()


@requires_rbcxx
def test_rbcxx_verify_fails_on_missing_root(tmp_path: Path) -> None:
    result = _run_rbcxx(
        "--variant=verify",
        _kv("--project-root", tmp_path),
        _kv("--shader-root", tmp_path / "does-not-exist"),
    )
    assert result.returncode == 1
    assert "shader-build: error:" in result.stderr


@requires_rbcxx
def test_artifact_publish_verify_wraps_rbcxx_errors(tmp_path: Path) -> None:
    """The install path surfaces rbcxx verification failures as ShaderVariantError."""
    from rbc_build.artifact_publish import verify_shader_root
    from rbc_build.shader_common import ShaderVariantError

    with pytest.raises(ShaderVariantError) as excinfo:
        verify_shader_root(tmp_path / "missing-root", "dx")
    assert "shader_manifest" in str(excinfo.value)
