from __future__ import annotations

import json
import multiprocessing
import runpy
import shutil
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import Any

import pytest
import rbc_build.shader_variants as shader_variants
from rbc_build.artifact_publish import install_build_artifacts

from rbc_build.shader_variants import (
    CrossProcessFileLock,
    HOST_INPUT_ID_MARKER,
    RENDER_PLUGIN_INPUT_ID_MARKER,
    RUNTIME_MANIFEST_NAME,
    ShaderVariantError,
    _atomic_replace_directory,
    _build_input_id,
    _tree_digest,
    build_backend,
    build_hostgen,
    compiler_fingerprint,
    load_config,
    recover_directory_backup,
    verify_shader_backends,
    verify_shader_coherence,
    verify_shader_root,
)

build_and_copy = runpy.run_path(
    str(Path(__file__).resolve().parents[1] / "scripts" / "build_and_copy.py")
)
copy_shader = build_and_copy["copy_shader"]


PROGRAMS = (
    "path_tracer/offline_pt",
    "path_tracer/offline_pt_denoise",
    "path_tracer/pt_multi_bounce_offline",
)
DENOISE_PROGRAM = PROGRAMS[1]
DENOISE_DEFINE = "RBC_OFFLINE_PT_DENOISE"


def _increment_with_lock(
    lock_path: Path, counter_path: Path, start: Any
) -> None:
    start.wait()
    with CrossProcessFileLock(lock_path, timeout_seconds=10.0):
        value = int(counter_path.read_text(encoding="utf-8"))
        time.sleep(0.05)
        counter_path.write_text(str(value + 1), encoding="utf-8")


class FakeCompiler:
    def __init__(
        self,
        *,
        abi_drift: bool = False,
        fail_single: bool = False,
        omit_required_host: bool = False,
        record_host_defines: bool = False,
        emit_utility_host: bool = False,
        binary_salt: str = "",
        delay_seconds: float = 0.0,
    ):
        self.abi_drift = abi_drift
        self.fail_single = fail_single
        self.omit_required_host = omit_required_host
        self.record_host_defines = record_host_defines
        self.emit_utility_host = emit_utility_host
        self.binary_salt = binary_salt
        self.delay_seconds = delay_seconds
        self.calls: list[tuple[list[str], Path]] = []
        self.input_is_file: list[bool] = []
        self._calls_lock = threading.Lock()

    @staticmethod
    def _argument(command: list[str], name: str) -> str | None:
        prefix = f"--{name}="
        for argument in command:
            if argument.startswith(prefix):
                return argument[len(prefix) :]
        return None

    def __call__(self, command_value: Any, cwd: Path) -> None:
        command = list(command_value)
        input_path = Path(self._argument(command, "in") or "")
        with self._calls_lock:
            self.calls.append((command, cwd))
            self.input_is_file.append(input_path.is_file())
            call_number = len(self.calls)
        if self.delay_seconds:
            time.sleep(self.delay_seconds)
        output_path = Path(self._argument(command, "out") or "")
        hostgen_value = self._argument(command, "hostgen")
        defines = tuple(
            sorted(
                argument[len("--D=") :]
                for argument in command
                if argument.startswith("--D=")
            )
        )

        def write_host_interface(generated: Path, logical: str) -> None:
            if self.omit_required_host and logical == PROGRAMS[0]:
                return
            interface = "args(buffer<float>);dispatch(uint2);"
            if (
                self.abi_drift
                and "RBC_LITE_PBR_MATERIAL=1" not in defines
                and logical == PROGRAMS[0]
            ):
                interface = "args(buffer<float>,buffer<uint>);dispatch(uint2);"
            generated.parent.mkdir(parents=True, exist_ok=True)
            defines_comment = (
                f"// defines: {defines!r}\r\n"
                if self.record_host_defines
                else ""
            )
            generated.write_text(
                f"// {logical}\r\n{defines_comment}{interface}\r\n",
                encoding="utf-8",
            )

        if input_path.is_file():
            if self.fail_single:
                raise RuntimeError("synthetic variant compiler failure")
            if hostgen_value is not None:
                generated = Path(hostgen_value)
                normalized = generated.as_posix()
                logical = next(
                    (
                        program
                        for program in PROGRAMS
                        if normalized.endswith(f"/{program}.inl")
                    ),
                    input_path.stem,
                )
                write_host_interface(generated, logical)
                return
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_bytes(
                f"single:{self.binary_salt}:{input_path.name}:{defines}".encode(
                    "ascii"
                )
            )
            return

        source_files = sorted(input_path.rglob("*.cpp"))
        if hostgen_value is not None:
            hostgen = Path(hostgen_value)
            for source in source_files:
                logical = source.relative_to(input_path).with_suffix("").as_posix()
                if logical == "utility" and not self.emit_utility_host:
                    continue
                generated = hostgen / f"{logical}.inl"
                write_host_interface(generated, logical)
            return

        for source in source_files:
            relative = source.relative_to(input_path).with_suffix(".bin")
            artifact = output_path / relative
            artifact.parent.mkdir(parents=True, exist_ok=True)
            artifact.write_bytes(
                (
                    f"default:{self.binary_salt}:{call_number}:"
                    f"{relative.as_posix()}:{defines}"
                ).encode("ascii")
            )


def _source_manifest(*, second_dimension: bool = False) -> dict[str, Any]:
    manifest: dict[str, Any] = {
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
                        "scene_features": {
                            "forbidden": ["complex_pbr_material"]
                        },
                    },
                    "full": {
                        "defines": {},
                        "scene_features": {
                            "required": ["complex_pbr_material"]
                        },
                    },
                },
            }
        },
        "variant_sets": {
            "offline_pbr": {
                "default": "lite",
                "permutations": [
                    {
                        "id": "lite",
                        "select": {"pbr_material": "lite"},
                    },
                    {
                        "id": "full",
                        "select": {"pbr_material": "full"},
                    },
                ],
            }
        },
        "programs": [
            {
                "id": program,
                "source": f"src/{program}.cpp",
                "variant_set": "offline_pbr",
                "host_abi": "stable",
            }
            for program in PROGRAMS
        ],
    }
    if second_dimension:
        manifest["dimensions"]["quality"] = {
            "default": "balanced",
            "values": {
                "balanced": {"defines": {"RBC_QUALITY_BALANCED": "1"}},
                "high": {"defines": {"RBC_QUALITY_HIGH": "1"}},
            },
        }
    return manifest


def _per_variant_source_manifest() -> dict[str, Any]:
    manifest = _source_manifest()
    manifest["scene_features"].append("free_space_diffraction")
    material_values = manifest["dimensions"]["pbr_material"]["values"]
    material_values["lite"]["scene_features"]["forbidden"].append(
        "free_space_diffraction"
    )
    material_values["full"]["scene_features"]["forbidden"] = [
        "free_space_diffraction"
    ]
    material_values["fsd"] = {
        "defines": {"RBC_ENABLE_FREE_SPACE_DIFFRACTION": "1"},
        "scene_features": {"required": ["free_space_diffraction"]},
    }
    manifest["variant_sets"]["offline_pbr"]["permutations"].append(
        {
            "id": "fsd",
            "select": {"pbr_material": "fsd"},
        }
    )
    for program in manifest["programs"]:
        program["host_abi"] = "per_variant"
        if program["id"] == DENOISE_PROGRAM:
            program["source"] = f"src/{PROGRAMS[0]}.cpp"
            program["defines"] = {DENOISE_DEFINE: None}
    return manifest


def _project(
    tmp_path: Path, *, second_dimension: bool = False
) -> tuple[Path, Path]:
    shader_root = tmp_path / "rbc" / "shader"
    (shader_root / "include").mkdir(parents=True)
    (shader_root / "include" / "common.hpp").write_text(
        "#pragma once\n", encoding="utf-8"
    )
    for program in PROGRAMS:
        source = shader_root / "src" / f"{program}.cpp"
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_text("int kernel() { return 0; }\n", encoding="utf-8")
    (shader_root / "src" / "utility.cpp").write_text(
        "int kernel() { return 0; }\n", encoding="utf-8"
    )
    manifest = shader_root / "shader_variants.json"
    manifest.write_text(
        json.dumps(_source_manifest(second_dimension=second_dimension)),
        encoding="utf-8",
    )
    compiler = tmp_path / "build" / "tool" / "rbcxx" / "rbcxx.exe"
    compiler.parent.mkdir(parents=True)
    compiler.write_bytes(b"fake compiler")
    return manifest, compiler


def _directory_backend_calls(fake: FakeCompiler, backend: str) -> int:
    return sum(
        FakeCompiler._argument(command, "backend") == backend
        and not input_is_file
        for (command, _), input_is_file in zip(fake.calls, fake.input_is_file)
    )


def test_build_emits_runtime_manifest_and_reuses_variant_cache(
    tmp_path: Path,
) -> None:
    manifest_path, compiler = _project(tmp_path)
    config = load_config(manifest_path)
    build_root = tmp_path / "out"
    cache_root = tmp_path / "cache"
    fake = FakeCompiler()

    shader_root = build_backend(
        config,
        backend="dx",
        build_root=build_root,
        cache_root=cache_root,
        compiler_path=compiler,
        runner=fake,
    )
    runtime = verify_shader_root(shader_root, "dx")
    first_build_id = runtime["build_id"]
    assert (shader_root / RUNTIME_MANIFEST_NAME).is_file()
    assert set(runtime) >= {
        "schema_version",
        "backend",
        "build_id",
        "input_id",
        "programs",
        "families",
    }
    assert runtime["families"] == {
        "offline_pbr": {
            "programs": list(PROGRAMS),
            "rules": [
                {
                    "selection": {"pbr_material": "lite"},
                    "required_features": [],
                    "forbidden_features": ["complex_pbr_material"],
                },
                {
                    "selection": {"pbr_material": "full"},
                    "required_features": ["complex_pbr_material"],
                    "forbidden_features": [],
                },
            ],
        }
    }

    for program in PROGRAMS:
        default = shader_root / f"{program}.bin"
        full = shader_root / "variants" / "pbr_material=full" / f"{program}.bin"
        assert b"RBC_LITE_PBR_MATERIAL=1" in default.read_bytes()
        assert b"RBC_LITE_PBR_MATERIAL=1" not in full.read_bytes()
        program_manifest = runtime["programs"][program]
        assert program_manifest["default_selection"] == {"pbr_material": "lite"}
        assert len(program_manifest["variants"]) == 2

    single_file_calls = sum(fake.input_is_file)
    assert single_file_calls == len(PROGRAMS)
    assert _directory_backend_calls(fake, "dx") == 1

    build_backend(
        config,
        backend="dx",
        build_root=build_root,
        cache_root=cache_root,
        compiler_path=compiler,
        runner=fake,
    )
    second_single_file_calls = sum(fake.input_is_file)
    assert second_single_file_calls == single_file_calls
    assert _directory_backend_calls(fake, "dx") == 1
    second_runtime = verify_shader_root(shader_root, "dx")
    assert second_runtime["build_id"] == first_build_id


def test_shared_source_program_defines_build_every_variant(
    tmp_path: Path,
) -> None:
    manifest_path, compiler = _project(tmp_path)
    manifest_path.write_text(
        json.dumps(_per_variant_source_manifest()),
        encoding="utf-8",
    )
    (manifest_path.parent / "src" / f"{DENOISE_PROGRAM}.cpp").unlink()
    config = load_config(manifest_path)
    fake = FakeCompiler()
    build_root = tmp_path / "out"
    cache_root = tmp_path / "cache"

    shader_root = build_backend(
        config,
        backend="dx",
        build_root=build_root,
        cache_root=cache_root,
        compiler_path=compiler,
        runner=fake,
    )
    runtime = verify_shader_root(shader_root, "dx")

    artifact_roots = {
        "lite": shader_root,
        "full": shader_root / "variants" / "pbr_material=full",
        "fsd": shader_root / "variants" / "pbr_material=fsd",
    }
    for artifact_root in artifact_roots.values():
        ordinary = (artifact_root / f"{PROGRAMS[0]}.bin").read_bytes()
        denoise = (artifact_root / f"{DENOISE_PROGRAM}.bin").read_bytes()
        assert DENOISE_DEFINE.encode("ascii") not in ordinary
        assert DENOISE_DEFINE.encode("ascii") in denoise

    assert b"RBC_LITE_PBR_MATERIAL=1" in (
        artifact_roots["lite"] / f"{DENOISE_PROGRAM}.bin"
    ).read_bytes()
    assert b"RBC_ENABLE_FREE_SPACE_DIFFRACTION=1" in (
        artifact_roots["fsd"] / f"{DENOISE_PROGRAM}.bin"
    ).read_bytes()
    assert len(runtime["programs"][DENOISE_PROGRAM]["variants"]) == 3

    first_call_count = len(fake.calls)
    build_backend(
        config,
        backend="dx",
        build_root=build_root,
        cache_root=cache_root,
        compiler_path=compiler,
        runner=fake,
    )
    assert len(fake.calls) == first_call_count


def test_verify_detects_tampered_artifact(tmp_path: Path) -> None:
    manifest_path, compiler = _project(tmp_path)
    shader_root = build_backend(
        load_config(manifest_path),
        backend="dx",
        build_root=tmp_path / "out",
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=FakeCompiler(),
    )
    artifact = (
        shader_root
        / "variants"
        / "pbr_material=full"
        / "path_tracer"
        / "offline_pt.bin"
    )
    artifact.write_bytes(b"tampered")
    with pytest.raises(ShaderVariantError, match="mismatch"):
        verify_shader_root(shader_root, "dx")


def test_failed_variant_compile_does_not_replace_existing_build(
    tmp_path: Path,
) -> None:
    manifest_path, compiler = _project(tmp_path)
    build_root = tmp_path / "out"
    existing = build_root / "shader_build_dx"
    existing.mkdir(parents=True)
    marker = existing / "keep.txt"
    marker.write_text("old build", encoding="utf-8")

    with pytest.raises(RuntimeError, match="synthetic"):
        build_backend(
            load_config(manifest_path),
            backend="dx",
            build_root=build_root,
            cache_root=tmp_path / "cache",
            compiler_path=compiler,
            runner=FakeCompiler(fail_single=True),
        )
    assert marker.read_text(encoding="utf-8") == "old build"


def test_stable_host_abi_mismatch_fails_before_publish(tmp_path: Path) -> None:
    manifest_path, compiler = _project(tmp_path)
    build_root = tmp_path / "out"
    with pytest.raises(ShaderVariantError, match="Stable host ABI changed"):
        build_backend(
            load_config(manifest_path),
            backend="dx",
            build_root=build_root,
            cache_root=tmp_path / "cache",
            compiler_path=compiler,
            runner=FakeCompiler(abi_drift=True),
        )
    assert not (build_root / "shader_build_dx").exists()


def test_stable_shared_source_hostgen_uses_program_defines(
    tmp_path: Path,
) -> None:
    manifest_path, compiler = _project(tmp_path)
    manifest = _source_manifest()
    denoise = next(
        program
        for program in manifest["programs"]
        if program["id"] == DENOISE_PROGRAM
    )
    denoise["source"] = f"src/{PROGRAMS[0]}.cpp"
    denoise["defines"] = {DENOISE_DEFINE: None}
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    (manifest_path.parent / "src" / f"{DENOISE_PROGRAM}.cpp").unlink()
    fake = FakeCompiler()
    host_output = tmp_path / "host"

    build_hostgen(
        load_config(manifest_path),
        host_output=host_output,
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=fake,
    )

    assert (host_output / f"{DENOISE_PROGRAM}.inl").is_file()
    isolated_host_calls = [
        command
        for (command, _), input_is_file in zip(fake.calls, fake.input_is_file)
        if input_is_file and FakeCompiler._argument(command, "hostgen") is not None
    ]
    assert isolated_host_calls
    assert all(f"--D={DENOISE_DEFINE}" in command for command in isolated_host_calls)
    assert all(
        f"--D={DENOISE_DEFINE}" not in command
        for (command, _), input_is_file in zip(fake.calls, fake.input_is_file)
        if not input_is_file
    )


def test_per_variant_hostgen_isolated_cached_and_atomically_merged(
    tmp_path: Path,
) -> None:
    manifest_path, compiler = _project(tmp_path)
    manifest_path.write_text(
        json.dumps(_per_variant_source_manifest()),
        encoding="utf-8",
    )
    (manifest_path.parent / "src" / f"{DENOISE_PROGRAM}.cpp").unlink()
    config = load_config(manifest_path)
    host_output = tmp_path / "host"
    host_output.mkdir()
    preserved = host_output / "manual.inl"
    preserved.write_text("manual host helper\n", encoding="utf-8")
    unlisted_interface = host_output / "utility.inl"
    unlisted_interface.write_text("stale utility interface\n", encoding="utf-8")
    fake = FakeCompiler(
        abi_drift=True,
        record_host_defines=True,
        emit_utility_host=True,
    )

    build_hostgen(
        config,
        host_output=host_output,
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=fake,
    )

    expected_defines = {
        "lite": ("RBC_LITE_PBR_MATERIAL=1",),
        "full": (),
        "fsd": ("RBC_ENABLE_FREE_SPACE_DIFFRACTION=1",),
    }
    for permutation, defines in expected_defines.items():
        for program in PROGRAMS:
            generated = (
                host_output
                / "variants"
                / "offline_pbr"
                / permutation
                / f"{program}.inl"
            )
            assert generated.is_file()
            program_defines = defines
            if program == DENOISE_PROGRAM:
                program_defines = tuple(sorted((*defines, DENOISE_DEFINE)))
            assert f"// defines: {program_defines!r}" in (
                generated.read_text(encoding="utf-8")
            )
            assert not (host_output / f"{program}.inl").exists()

    unlisted_content = unlisted_interface.read_text(encoding="utf-8")
    assert "// utility" in unlisted_content
    assert "// defines: ('RBC_LITE_PBR_MATERIAL=1',)" in unlisted_content

    first_call_count = len(fake.calls)
    # The default selection is lite, so its hostgen tree is reused for both
    # ordinary root interfaces and the lite variant ABI.
    assert first_call_count == 2 * len(expected_defines)
    compiler_caches = {
        FakeCompiler._argument(command, "cache_dir")
        for command, _cwd in fake.calls
    }
    assert len(compiler_caches) == len(expected_defines)
    stale_variant = (
        host_output / "variants" / "offline_pbr" / "stale.inl"
    )
    stale_variant.write_text("stale\n", encoding="utf-8")
    legacy_root = host_output / f"{PROGRAMS[0]}.inl"
    legacy_root.parent.mkdir(parents=True, exist_ok=True)
    legacy_root.write_text("legacy\n", encoding="utf-8")

    build_hostgen(
        config,
        host_output=host_output,
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=fake,
    )

    assert len(fake.calls) == first_call_count
    assert preserved.read_text(encoding="utf-8") == "manual host helper\n"
    assert unlisted_interface.read_text(encoding="utf-8") == unlisted_content
    assert not stale_variant.exists()
    assert not legacy_root.exists()


def test_hostgen_requires_declared_interfaces_and_preserves_existing_files(
    tmp_path: Path,
) -> None:
    manifest_path, compiler = _project(tmp_path)
    config = load_config(manifest_path)
    host_output = tmp_path / "host"
    host_output.mkdir()
    preserved = host_output / "manual.inl"
    preserved.write_text("manual host helper\n", encoding="utf-8")
    fake = FakeCompiler()

    build_hostgen(
        config,
        host_output=host_output,
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=fake,
    )
    build_hostgen(
        config,
        host_output=host_output,
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=fake,
    )

    assert preserved.read_text(encoding="utf-8") == "manual host helper\n"
    assert not (host_output / "utility.inl").exists()
    for program in PROGRAMS:
        assert (host_output / f"{program}.inl").is_file()
    assert not (host_output / "families").exists()


def test_hostgen_rejects_stale_required_file_and_keys_cache_by_inputs(
    tmp_path: Path,
) -> None:
    manifest_path, compiler = _project(tmp_path)
    config = load_config(manifest_path)
    host_output = tmp_path / "host"
    cache_root = tmp_path / "cache"
    good = FakeCompiler()
    build_hostgen(
        config,
        host_output=host_output,
        cache_root=cache_root,
        compiler_path=compiler,
        runner=good,
    )
    host_objects = cache_root / "host_objects"
    first_objects = {path.name for path in host_objects.iterdir() if path.is_dir()}
    assert len(first_objects) == 1
    shutil.rmtree(next(path for path in host_objects.iterdir() if path.is_dir()))

    required = host_output / f"{PROGRAMS[0]}.inl"
    required.write_text("stale interface\n", encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="in this invocation"):
        build_hostgen(
            config,
            host_output=host_output,
            cache_root=cache_root,
            compiler_path=compiler,
            runner=FakeCompiler(omit_required_host=True),
        )
    assert required.read_text(encoding="utf-8") == "stale interface\n"

    build_hostgen(
        config,
        host_output=host_output,
        cache_root=cache_root,
        compiler_path=compiler,
        runner=FakeCompiler(),
    )

    (config.shader_root / "include" / "common.hpp").write_text(
        "#pragma once\n// changed\n", encoding="utf-8"
    )
    changed = FakeCompiler()
    build_hostgen(
        config,
        host_output=host_output,
        cache_root=cache_root,
        compiler_path=compiler,
        runner=changed,
    )
    changed_objects = {path.name for path in host_objects.iterdir() if path.is_dir()}
    assert first_objects < changed_objects


def test_manifest_selection_is_projected_to_program_family(tmp_path: Path) -> None:
    manifest_path, compiler = _project(tmp_path, second_dimension=True)
    shader_root = build_backend(
        load_config(manifest_path),
        backend="dx",
        build_root=tmp_path / "out",
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=FakeCompiler(),
    )
    runtime = verify_shader_root(shader_root, "dx")
    program = runtime["programs"][PROGRAMS[0]]
    assert program["default_selection"] == {"pbr_material": "lite"}
    assert [variant["selection"] for variant in program["variants"]] == [
        {"pbr_material": "lite"},
        {"pbr_material": "full"},
    ]
    assert (
        shader_root
        / "variants"
        / "pbr_material=full"
        / f"{PROGRAMS[0]}.bin"
    ).is_file()
    assert runtime["programs"]["utility"]["default_selection"] == {}
    assert runtime["programs"]["utility"]["variants"][0]["selection"] == {}


def test_build_id_includes_actual_artifact_hashes(tmp_path: Path) -> None:
    manifest_path, compiler = _project(tmp_path)
    config = load_config(manifest_path)
    first = build_backend(
        config,
        backend="dx",
        build_root=tmp_path / "out-a",
        cache_root=tmp_path / "cache-a",
        compiler_path=compiler,
        runner=FakeCompiler(binary_salt="a"),
    )
    second = build_backend(
        config,
        backend="dx",
        build_root=tmp_path / "out-b",
        cache_root=tmp_path / "cache-b",
        compiler_path=compiler,
        runner=FakeCompiler(binary_salt="b"),
    )
    assert verify_shader_root(first, "dx")["build_id"] != verify_shader_root(
        second, "dx"
    )["build_id"]


def test_concurrent_backend_builds_share_cache_without_publish_errors(
    tmp_path: Path,
) -> None:
    manifest_path, compiler = _project(tmp_path)
    config = load_config(manifest_path)
    fake = FakeCompiler(delay_seconds=0.005)

    def build(backend: str) -> Path:
        return build_backend(
            config,
            backend=backend,
            build_root=tmp_path / "out",
            cache_root=tmp_path / "cache",
            compiler_path=compiler,
            runner=fake,
        )

    with ThreadPoolExecutor(max_workers=3) as executor:
        outputs = [
            future.result()
            for future in (
                executor.submit(build, "dx"),
                executor.submit(build, "vk"),
                executor.submit(build, "dx"),
            )
        ]
    assert [verify_shader_root(path)["backend"] for path in outputs] == [
        "dx",
        "vk",
        "dx",
    ]
    assert _directory_backend_calls(fake, "dx") == 1
    assert _directory_backend_calls(fake, "vk") == 1


def test_build_retries_when_live_inputs_change(tmp_path: Path) -> None:
    manifest_path, compiler = _project(tmp_path)
    source = manifest_path.parent / "src" / f"{PROGRAMS[0]}.cpp"
    fake = FakeCompiler()
    mutated = False

    def mutate_once(command: Any, cwd: Path) -> None:
        nonlocal mutated
        fake(command, cwd)
        if not mutated:
            source.write_text("int kernel() { return 1; }\n", encoding="utf-8")
            mutated = True

    shader_root = build_backend(
        load_config(manifest_path),
        backend="dx",
        build_root=tmp_path / "out",
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=mutate_once,
    )

    runtime = verify_shader_root(shader_root, "dx")
    assert mutated
    assert runtime["input_id"] == _build_input_id(
        _tree_digest(load_config(manifest_path)), compiler_fingerprint(compiler)
    )
    assert not any((tmp_path / "cache" / "input_snapshots").iterdir())


def test_hostgen_retries_when_live_inputs_change(tmp_path: Path) -> None:
    manifest_path, compiler = _project(tmp_path)
    source = manifest_path.parent / "src" / f"{PROGRAMS[0]}.cpp"
    fake = FakeCompiler()
    mutated = False

    def mutate_once(command: Any, cwd: Path) -> None:
        nonlocal mutated
        fake(command, cwd)
        if not mutated:
            source.write_text("int kernel() { return 3; }\n", encoding="utf-8")
            mutated = True

    host_output = tmp_path / "host"
    build_hostgen(
        load_config(manifest_path),
        host_output=host_output,
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=mutate_once,
    )

    assert mutated
    assert (host_output / ".shader_input_id").read_text(
        encoding="ascii"
    ).strip() == _build_input_id(
        _tree_digest(load_config(manifest_path)), compiler_fingerprint(compiler)
    )


def test_repeated_input_changes_preserve_previous_build(tmp_path: Path) -> None:
    manifest_path, compiler = _project(tmp_path)
    config = load_config(manifest_path)
    build_root = tmp_path / "out"
    existing = build_backend(
        config,
        backend="dx",
        build_root=build_root,
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=FakeCompiler(),
    )
    previous_manifest = (existing / RUNTIME_MANIFEST_NAME).read_bytes()
    source = manifest_path.parent / "src" / f"{PROGRAMS[0]}.cpp"
    source.write_text("int kernel() { return 10; }\n", encoding="utf-8")
    mutation = 10
    fake = FakeCompiler()

    def mutate_repeatedly(command: Any, cwd: Path) -> None:
        nonlocal mutation
        fake(command, cwd)
        mutation += 1
        source.write_text(
            f"int kernel() {{ return {mutation}; }}\n", encoding="utf-8"
        )

    with pytest.raises(ShaderVariantError, match="changed repeatedly"):
        build_backend(
            config,
            backend="dx",
            build_root=build_root,
            cache_root=tmp_path / "cache",
            compiler_path=compiler,
            rebuild=True,
            runner=mutate_repeatedly,
        )

    assert (existing / RUNTIME_MANIFEST_NAME).read_bytes() == previous_manifest
    verify_shader_root(existing, "dx")


def test_verify_rejects_backends_from_different_inputs(tmp_path: Path) -> None:
    manifest_path, compiler = _project(tmp_path)
    config = load_config(manifest_path)
    build_root = tmp_path / "out"
    cache_root = tmp_path / "cache"
    build_backend(
        config,
        backend="dx",
        build_root=build_root,
        cache_root=cache_root,
        compiler_path=compiler,
        runner=FakeCompiler(),
    )
    source = manifest_path.parent / "src" / f"{PROGRAMS[0]}.cpp"
    source.write_text("int kernel() { return 2; }\n", encoding="utf-8")
    build_backend(
        config,
        backend="vk",
        build_root=build_root,
        cache_root=cache_root,
        compiler_path=compiler,
        runner=FakeCompiler(),
    )

    with pytest.raises(ShaderVariantError, match="different inputs"):
        verify_shader_backends(build_root, ["dx", "vk"])


def test_verify_shader_coherence_binds_host_and_render_plugin(
    tmp_path: Path,
) -> None:
    manifest_path, compiler = _project(tmp_path)
    config = load_config(manifest_path)
    build_root = tmp_path / "out"
    for backend in ("dx", "vk"):
        build_backend(
            config,
            backend=backend,
            build_root=build_root,
            cache_root=tmp_path / "cache",
            compiler_path=compiler,
            runner=FakeCompiler(),
        )
    expected = verify_shader_backends(build_root, ["dx", "vk"])["dx"][
        "input_id"
    ]
    different = ("0" if expected[0] != "0" else "1") + expected[1:]
    host_output = tmp_path / "host"
    host_output.mkdir()
    host_marker = host_output / HOST_INPUT_ID_MARKER
    plugin_marker = tmp_path / RENDER_PLUGIN_INPUT_ID_MARKER

    host_marker.write_text(different + "\n", encoding="ascii")
    plugin_marker.write_text(expected + "\n", encoding="ascii")
    with pytest.raises(ShaderVariantError, match="generations do not match"):
        verify_shader_coherence(
            build_root, ["dx", "vk"], host_output, plugin_marker
        )

    host_marker.write_text(expected + "\n", encoding="ascii")
    plugin_marker.write_text(different + "\n", encoding="ascii")
    with pytest.raises(ShaderVariantError, match="generations do not match"):
        verify_shader_coherence(
            build_root, ["dx", "vk"], host_output, plugin_marker
        )

    plugin_marker.write_text(expected + "\n", encoding="ascii")
    assert (
        verify_shader_coherence(
            build_root, ["dx", "vk"], host_output, plugin_marker
        )
        == expected
    )


def test_atomic_artifact_install_preserves_old_root_on_failure(
    tmp_path: Path,
) -> None:
    manifest_path, compiler = _project(tmp_path)
    config = load_config(manifest_path)
    build_root = tmp_path / "build"
    cache_root = tmp_path / "cache"
    for backend in ("dx", "vk"):
        build_backend(
            config,
            backend=backend,
            build_root=build_root,
            cache_root=cache_root,
            compiler_path=compiler,
            runner=FakeCompiler(),
        )
    target_dir = build_root / "release"
    target_dir.mkdir()
    (target_dir / "runtime.dll").write_bytes(b"new runtime")
    expected_input_id = verify_shader_backends(build_root, ["dx", "vk"])[
        "dx"
    ]["input_id"]
    host_output = tmp_path / "host"
    host_output.mkdir()
    (host_output / HOST_INPUT_ID_MARKER).write_text(
        expected_input_id + "\n", encoding="ascii"
    )
    (target_dir / RENDER_PLUGIN_INPUT_ID_MARKER).write_text(
        expected_input_id + "\n", encoding="ascii"
    )

    extension = tmp_path / "extension" / "_C"
    extension.mkdir(parents=True)
    (extension / "runtime.dll").write_bytes(b"old runtime")
    (extension / "removed.dll").write_bytes(b"removed")
    (extension / "runtime.pyi").write_text("preserve\n", encoding="utf-8")

    def fail_publish(staged: Path, destination: Path) -> None:
        assert (staged / "runtime.dll").read_bytes() == b"new runtime"
        assert (staged / "runtime.pyi").is_file()
        assert (
            staged / RENDER_PLUGIN_INPUT_ID_MARKER
        ).read_text(encoding="ascii").strip() == expected_input_id
        raise OSError("synthetic publish failure")

    with pytest.raises(OSError, match="synthetic publish failure"):
        install_build_artifacts(
            target_dir,
            extension,
            ["dx", "vk"],
            host_output=host_output,
            publisher=fail_publish,
        )

    assert (extension / "runtime.dll").read_bytes() == b"old runtime"
    assert (extension / "removed.dll").is_file()
    assert (extension / "runtime.pyi").is_file()

    install_build_artifacts(
        target_dir,
        extension,
        ["dx", "vk"],
        host_output=host_output,
    )
    assert (extension / "runtime.dll").read_bytes() == b"new runtime"
    assert not (extension / "removed.dll").exists()
    assert (extension / "runtime.pyi").is_file()
    assert (
        extension / RENDER_PLUGIN_INPUT_ID_MARKER
    ).read_text(encoding="ascii").strip() == expected_input_id
    assert not (extension / ".shader_locks").exists()
    verify_shader_backends(extension, ["dx", "vk"])

    different = (
        ("0" if expected_input_id[0] != "0" else "1")
        + expected_input_id[1:]
    )
    (target_dir / RENDER_PLUGIN_INPUT_ID_MARKER).write_text(
        different + "\n", encoding="ascii"
    )
    with pytest.raises(ShaderVariantError, match="generations do not match"):
        install_build_artifacts(
            target_dir,
            extension,
            ["dx", "vk"],
            host_output=host_output,
        )
    assert (
        extension / RENDER_PLUGIN_INPUT_ID_MARKER
    ).read_text(encoding="ascii").strip() == expected_input_id


def test_cross_process_lock_serializes_writers(tmp_path: Path) -> None:
    context = multiprocessing.get_context("spawn")
    counter = tmp_path / "counter.txt"
    counter.write_text("0", encoding="utf-8")
    lock_path = tmp_path / "counter.lock"
    start = context.Event()
    processes = [
        context.Process(
            target=_increment_with_lock,
            args=(lock_path, counter, start),
        )
        for _ in range(2)
    ]
    for process in processes:
        process.start()
    start.set()
    for process in processes:
        process.join(10)
        assert process.exitcode == 0
    assert counter.read_text(encoding="utf-8") == "2"


def test_committed_directory_survives_backup_cleanup_failure(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    destination = tmp_path / "published"
    destination.mkdir()
    (destination / "generation.txt").write_text("old", encoding="utf-8")
    staged = tmp_path / "staged"
    staged.mkdir()
    (staged / "generation.txt").write_text("new", encoding="utf-8")
    backup = destination.with_name(f".{destination.name}.backup")
    remove_path = shader_variants._remove_path

    def fail_backup_cleanup(path: Path) -> None:
        if path == backup:
            raise PermissionError("synthetic loaded DLL")
        remove_path(path)

    monkeypatch.setattr(shader_variants, "_remove_path", fail_backup_cleanup)
    shader_variants._atomic_replace_directory(staged, destination)

    assert (destination / "generation.txt").read_text(encoding="utf-8") == "new"
    assert (backup / "generation.txt").read_text(encoding="utf-8") == "old"


def test_backup_recovery_and_locked_atomic_publish(tmp_path: Path) -> None:
    destination = tmp_path / "shader_build_dx"
    destination.mkdir()
    (destination / "old.bin").write_bytes(b"old")
    backup = tmp_path / ".shader_build_dx.backup"
    destination.replace(backup)

    recover_directory_backup(destination)
    assert (destination / "old.bin").read_bytes() == b"old"
    assert not backup.exists()

    staged = tmp_path / "staged"
    staged.mkdir()
    (staged / "new.bin").write_bytes(b"new")
    with CrossProcessFileLock(tmp_path / "publish.lock"):
        _atomic_replace_directory(staged, destination)
    assert not (destination / "old.bin").exists()
    assert (destination / "new.bin").read_bytes() == b"new"
    assert not backup.exists()


def test_copy_shader_verifies_before_replacing_destination(tmp_path: Path) -> None:
    manifest_path, compiler = _project(tmp_path)
    config = load_config(manifest_path)
    build_root = tmp_path / "build"
    shader_root = build_backend(
        config,
        backend="dx",
        build_root=build_root,
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=FakeCompiler(),
    )
    target_dir = build_root / "release"
    target_dir.mkdir()
    extension_root = tmp_path / "extension"
    existing = extension_root / "shader_build_dx"
    existing.mkdir(parents=True)
    marker = existing / "keep.txt"
    marker.write_text("keep", encoding="utf-8")
    (shader_root / f"{PROGRAMS[0]}.bin").write_bytes(b"corrupt")

    with pytest.raises(ShaderVariantError, match="mismatch"):
        copy_shader("dx", target_dir, extension_root)
    assert marker.read_text(encoding="utf-8") == "keep"
    with pytest.raises(ShaderVariantError, match="mismatch"):
        verify_shader_backends(build_root, ["dx", "vk"])
    with pytest.raises(ShaderVariantError, match="Cannot read"):
        verify_shader_backends(build_root, ["vk"])

    build_backend(
        config,
        backend="dx",
        build_root=build_root,
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=FakeCompiler(),
    )
    copied = copy_shader("dx", target_dir, extension_root)
    assert copied > 0
    assert not marker.exists()
    verify_shader_root(existing, "dx")
    assert not existing.with_name(f".{existing.name}.backup").exists()


def test_validate_rejects_non_global_default_permutation(tmp_path: Path) -> None:
    manifest_path, _ = _project(tmp_path)
    manifest = _source_manifest()
    manifest["variant_sets"]["offline_pbr"]["default"] = "full"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="global default"):
        load_config(manifest_path)


def test_program_defines_are_validated_and_change_input_id(
    tmp_path: Path,
) -> None:
    manifest_path, compiler = _project(tmp_path)
    manifest = _source_manifest()
    denoise = next(
        program
        for program in manifest["programs"]
        if program["id"] == DENOISE_PROGRAM
    )
    denoise["source"] = f"src/{PROGRAMS[0]}.cpp"
    denoise["defines"] = {DENOISE_DEFINE: None}
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    (manifest_path.parent / "src" / f"{DENOISE_PROGRAM}.cpp").unlink()

    first_config = load_config(manifest_path)
    compiler_info = compiler_fingerprint(compiler)
    first_input_id = _build_input_id(_tree_digest(first_config), compiler_info)
    denoise["defines"] = {f"{DENOISE_DEFINE}_CHANGED": "1"}
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    second_config = load_config(manifest_path)
    second_input_id = _build_input_id(_tree_digest(second_config), compiler_info)
    assert second_input_id != first_input_id

    denoise["defines"] = {"RBC_LITE_PBR_MATERIAL": "1"}
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="program.*defines"):
        load_config(manifest_path)


def test_program_alias_rejects_physical_logical_id_collision(
    tmp_path: Path,
) -> None:
    manifest_path, _ = _project(tmp_path)
    manifest = _source_manifest()
    denoise = next(
        program
        for program in manifest["programs"]
        if program["id"] == DENOISE_PROGRAM
    )
    denoise["source"] = f"src/{PROGRAMS[0]}.cpp"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="collides"):
        load_config(manifest_path)


def test_scene_feature_rules_reject_ambiguity_and_gaps(tmp_path: Path) -> None:
    manifest_path, _ = _project(tmp_path)

    ambiguous = _source_manifest()
    ambiguous["dimensions"]["pbr_material"]["values"]["full"][
        "scene_features"
    ] = {}
    manifest_path.write_text(json.dumps(ambiguous), encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="ambiguous scene feature rules"):
        load_config(manifest_path)

    gap = _source_manifest()
    gap["dimensions"]["pbr_material"]["values"]["full"][
        "scene_features"
    ]["required"].append("ray_tracing_material")
    gap["scene_features"].append("ray_tracing_material")
    manifest_path.write_text(json.dumps(gap), encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="no scene feature rule"):
        load_config(manifest_path)


def test_scene_feature_rules_reject_merged_conflicts(tmp_path: Path) -> None:
    manifest_path, _ = _project(tmp_path)
    manifest = _source_manifest()
    manifest["dimensions"]["policy"] = {
        "default": "standard",
        "values": {
            "standard": {
                "defines": {},
                "scene_features": {
                    "forbidden": ["complex_pbr_material"]
                },
            }
        },
    }
    for permutation in manifest["variant_sets"]["offline_pbr"]["permutations"]:
        permutation["select"]["policy"] = "standard"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="both requires and forbids"):
        load_config(manifest_path)


def test_multiple_families_project_only_their_dimensions(tmp_path: Path) -> None:
    manifest_path, compiler = _project(tmp_path)
    manifest = _source_manifest()
    manifest["scene_features"].append("high_quality_denoise")
    manifest["dimensions"]["denoise_quality"] = {
        "default": "quick",
        "values": {
            "quick": {
                "defines": {"RBC_DENOISE_QUICK": "1"},
                "scene_features": {
                    "forbidden": ["high_quality_denoise"]
                },
            },
            "high": {
                "defines": {"RBC_DENOISE_HIGH": "1"},
                "scene_features": {"required": ["high_quality_denoise"]},
            },
        },
    }
    manifest["variant_sets"]["denoise"] = {
        "default": "quick_lite",
        "permutations": [
            {
                "id": "quick_lite",
                "select": {
                    "denoise_quality": "quick",
                    "pbr_material": "lite",
                },
            },
            {
                "id": "high_lite",
                "select": {
                    "denoise_quality": "high",
                    "pbr_material": "lite",
                },
            },
            {
                "id": "quick_full",
                "select": {
                    "denoise_quality": "quick",
                    "pbr_material": "full",
                },
            },
            {
                "id": "high_full",
                "select": {
                    "denoise_quality": "high",
                    "pbr_material": "full",
                },
            },
        ],
    }
    extra_program = "post/denoise"
    extra_source = manifest_path.parent / "src" / f"{extra_program}.cpp"
    extra_source.parent.mkdir(parents=True, exist_ok=True)
    extra_source.write_text("int kernel() { return 0; }\n", encoding="utf-8")
    manifest["programs"].append(
        {
            "id": extra_program,
            "source": f"src/{extra_program}.cpp",
            "variant_set": "denoise",
            "host_abi": "stable",
        }
    )
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

    shader_root = build_backend(
        load_config(manifest_path),
        backend="dx",
        build_root=tmp_path / "out",
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=FakeCompiler(),
    )
    runtime = verify_shader_root(shader_root, "dx")

    assert runtime["families"]["offline_pbr"]["programs"] == list(PROGRAMS)
    assert runtime["families"]["denoise"]["programs"] == [extra_program]
    assert {
        tuple(sorted(rule["selection"].items()))
        for rule in runtime["families"]["denoise"]["rules"]
    } == {
        (
            ("denoise_quality", quality),
            ("pbr_material", material),
        )
        for quality in ("quick", "high")
        for material in ("lite", "full")
    }
    assert [
        variant["selection"]
        for variant in runtime["programs"][extra_program]["variants"]
    ] == [
        {"denoise_quality": "quick", "pbr_material": "lite"},
        {"denoise_quality": "high", "pbr_material": "full"},
        {"denoise_quality": "high", "pbr_material": "lite"},
        {"denoise_quality": "quick", "pbr_material": "full"},
    ]
    assert runtime["programs"][PROGRAMS[0]]["default_selection"] == {
        "pbr_material": "lite"
    }
    assert runtime["programs"]["utility"]["default_selection"] == {}


def test_verify_rejects_family_program_and_rule_drift(tmp_path: Path) -> None:
    manifest_path, compiler = _project(tmp_path)
    shader_root = build_backend(
        load_config(manifest_path),
        backend="dx",
        build_root=tmp_path / "out",
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=FakeCompiler(),
    )
    runtime_path = shader_root / RUNTIME_MANIFEST_NAME
    original = runtime_path.read_bytes()

    runtime = json.loads(original)
    runtime["families"]["offline_pbr"]["programs"].pop()
    runtime_path.write_text(json.dumps(runtime), encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="belongs to no family"):
        verify_shader_root(shader_root, "dx")

    runtime_path.write_bytes(original)
    runtime = json.loads(original)
    runtime["families"]["offline_pbr"]["rules"][0]["selection"] = {
        "pbr_material": "broken"
    }
    runtime_path.write_text(json.dumps(runtime), encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="do not match artifacts"):
        verify_shader_root(shader_root, "dx")


def test_verify_reports_missing_artifact(tmp_path: Path) -> None:
    manifest_path, compiler = _project(tmp_path)
    shader_root = build_backend(
        load_config(manifest_path),
        backend="dx",
        build_root=tmp_path / "out",
        cache_root=tmp_path / "cache",
        compiler_path=compiler,
        runner=FakeCompiler(),
    )
    (
        shader_root
        / "variants"
        / "pbr_material=full"
        / f"{PROGRAMS[0]}.bin"
    ).unlink()
    with pytest.raises(ShaderVariantError, match="Missing runtime shader artifact"):
        verify_shader_root(shader_root, "dx")


def test_validate_rejects_family_without_programs(tmp_path: Path) -> None:
    manifest_path, _ = _project(tmp_path)
    manifest = _source_manifest()
    manifest["variant_sets"]["unused"] = {
        "default": "lite",
        "permutations": [
            {
                "id": "lite",
                "select": {"pbr_material": "lite"},
            },
            {
                "id": "full",
                "select": {"pbr_material": "full"},
            },
        ],
    }
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="not referenced by any program"):
        load_config(manifest_path)


def test_validate_rejects_undeclared_and_unused_scene_features(
    tmp_path: Path,
) -> None:
    manifest_path, _ = _project(tmp_path)

    undeclared = _source_manifest()
    undeclared["scene_features"] = []
    manifest_path.write_text(json.dumps(undeclared), encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="undeclared scene features"):
        load_config(manifest_path)

    unused = _source_manifest()
    unused["scene_features"].append("unused_feature")
    manifest_path.write_text(json.dumps(unused), encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="not used by any variant set"):
        load_config(manifest_path)


def test_scene_feature_exhaustive_validation_has_a_bound(tmp_path: Path) -> None:
    manifest_path, _ = _project(tmp_path)
    manifest = _source_manifest()
    extra_features = [f"feature_{index}" for index in range(16)]
    manifest["scene_features"].extend(extra_features)
    manifest["dimensions"]["pbr_material"]["values"]["lite"][
        "scene_features"
    ]["forbidden"].extend(extra_features)
    manifest["dimensions"]["pbr_material"]["values"]["full"][
        "scene_features"
    ]["required"].extend(extra_features)
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(ShaderVariantError, match="supports at most 16"):
        load_config(manifest_path)
