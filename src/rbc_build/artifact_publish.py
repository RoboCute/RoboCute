"""Verified, atomic publication of one complete extension generation.

Shader variant build/verify logic lives in the ``rbcxx`` C++ tool; this module
keeps the staging/atomic-publish orchestration and calls ``rbcxx.exe`` for
strict verification and coherence checks.
"""

from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import tempfile
from collections.abc import Callable, Sequence
from pathlib import Path

from rbc_build.shader_common import (
    CrossProcessFileLock,
    ShaderVariantError,
    _atomic_replace_directory,
    _sha256_file,
    recover_directory_backup,
)

RENDER_PLUGIN_INPUT_ID_MARKER = "rbc_render_plugin.input_id"
MANAGED_ARTIFACT_SUFFIXES = frozenset(
    {".dll", ".pyd", ".bytes", ".input_id"}
)


def rbcxx_executable() -> Path:
    """Return the rbcxx.exe used by the shader variant driver."""
    project_root = Path(__file__).resolve().parents[2]
    name = "rbcxx.exe"
    compiler = project_root / "build" / "tool" / "rbcxx" / name
    if not compiler.is_file():
        raise ShaderVariantError(f"Shader compiler not found: {compiler}")
    return compiler


def _rbcxx(*args: str) -> None:
    compiler = rbcxx_executable()
    try:
        subprocess.run([str(compiler), *args], check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or "").strip()
        prefix = "shader-build: error: "
        if detail.startswith(prefix):
            detail = detail[len(prefix):]
        if not detail:
            detail = f"rbcxx {args[0] if args else ''} failed with exit code {error.returncode}"
        raise ShaderVariantError(detail) from error


def _kv(key: str, value: object) -> str:
    return f"{key}={value}"


def verify_shader_root(shader_root: Path, backend: str | None = None) -> None:
    """Strictly verify one shader root via rbcxx --variant=verify."""
    args = ["--variant=verify", _kv("--shader-root", shader_root)]
    if backend is not None:
        args.append(_kv("--backend", backend))
    _rbcxx(*args)


def verify_shader_backends(build_root: Path, backends: Sequence[str]) -> None:
    """Strictly verify every backend root via rbcxx --variant=verify."""
    for backend in backends:
        verify_shader_root(build_root / f"shader_build_{backend}", backend)


def verify_shader_coherence(
    build_root: Path,
    backends: Sequence[str],
    host_output: Path,
    plugin_marker: Path | None = None,
) -> None:
    """Verify backend/hostgen/render-plugin coherence via rbcxx."""
    args = [
        "--variant=verify-coherence",
        _kv("--build-root", build_root),
        _kv("--host-out", host_output),
    ]
    for backend in backends:
        args.append(_kv("--backend", backend))
    if plugin_marker is not None:
        args.append(_kv("--plugin-marker", plugin_marker))
    _rbcxx(*args)


def _shader_manifest(build_root: Path, backend: str) -> dict:
    manifest_path = build_root / f"shader_build_{backend}" / "shader_manifest.json"
    try:
        with manifest_path.open(encoding="utf-8") as stream:
            return json.load(stream)
    except (OSError, json.JSONDecodeError) as error:
        raise ShaderVariantError(
            f"Cannot read runtime shader manifest: {error}"
        ) from error


def install_verified_shader_root(
    source: Path, destination: Path, expected_backend: str
) -> Path:
    """Verify a shader root, stage it next to the destination and publish atomically."""
    source = source.resolve()
    destination = destination.resolve()
    verify_shader_root(source, expected_backend)
    destination.parent.mkdir(parents=True, exist_ok=True)
    staged = Path(
        tempfile.mkdtemp(
            prefix=f".{destination.name}.install.", dir=destination.parent
        )
    )
    published = False
    try:
        shutil.copytree(source, staged, dirs_exist_ok=True)
        verify_shader_root(staged, expected_backend)
        publish_lock = (
            destination.parent
            / ".shader_locks"
            / f"{destination.name}.lock"
        )
        with CrossProcessFileLock(publish_lock):
            recover_directory_backup(destination)
            _atomic_replace_directory(staged, destination)
            published = True
        return destination
    finally:
        if not published and staged.exists():
            shutil.rmtree(staged, ignore_errors=True)


def install_build_artifacts(
    target_dir: Path,
    destination: Path,
    backends: Sequence[str],
    *,
    host_output: Path,
    prepare_staged: Callable[[Path], None] | None = None,
    publisher: Callable[[Path, Path], None] = _atomic_replace_directory,
) -> int:
    """Verify, stage, and atomically publish one complete extension generation."""
    target_dir = target_dir.resolve()
    destination = destination.resolve()
    host_output = host_output.resolve()
    backends = tuple(backends)
    if not backends:
        raise ShaderVariantError("At least one shader backend is required")

    plugin_marker = target_dir / RENDER_PLUGIN_INPUT_ID_MARKER
    verify_shader_coherence(
        target_dir.parent,
        backends,
        host_output,
        plugin_marker,
    )
    source_shader_manifests = {
        backend: _shader_manifest(target_dir.parent, backend) for backend in backends
    }

    def collect_artifacts() -> dict[str, Path]:
        return {
            source.name: source
            for suffix in MANAGED_ARTIFACT_SUFFIXES
            for source in target_dir.glob(f"*{suffix}")
        }

    artifact_sources = collect_artifacts()
    binary_sources = {
        name: source
        for name, source in artifact_sources.items()
        if source.suffix.lower() != ".input_id"
    }
    if not binary_sources:
        raise ShaderVariantError(
            f"No managed binary/resource artifacts were found in {target_dir}"
        )
    source_artifact_hashes = {
        name: _sha256_file(source)
        for name, source in sorted(artifact_sources.items())
    }

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination_key = hashlib.sha256(
        str(destination).casefold().encode("utf-8")
    ).hexdigest()
    publish_lock = (
        Path(tempfile.gettempdir())
        / "robocute-artifact-locks"
        / f"{destination_key}.lock"
    )
    with CrossProcessFileLock(publish_lock):
        recover_directory_backup(destination)
        staged = Path(
            tempfile.mkdtemp(
                prefix=f".{destination.name}.install.",
                dir=destination.parent,
            )
        )
        published = False
        try:
            if destination.is_dir():
                for existing in destination.iterdir():
                    is_managed_file = (
                        existing.is_file()
                        and existing.suffix.lower()
                        in MANAGED_ARTIFACT_SUFFIXES
                    )
                    is_managed_dir = existing.is_dir() and (
                        existing.name.startswith("shader_build_")
                        or existing.name == ".shader_locks"
                    )
                    if is_managed_file or is_managed_dir:
                        continue
                    staged_item = staged / existing.name
                    if existing.is_dir():
                        shutil.copytree(existing, staged_item)
                    else:
                        shutil.copy2(existing, staged_item)

            copied_files = 0
            for name, source in sorted(artifact_sources.items()):
                shutil.copy2(source, staged / name)
                if _sha256_file(staged / name) != source_artifact_hashes[name]:
                    raise ShaderVariantError(
                        f"Build artifact changed while staging: {name}"
                    )
                copied_files += 1

            for backend in backends:
                install_verified_shader_root(
                    target_dir.parent / f"shader_build_{backend}",
                    staged / f"shader_build_{backend}",
                    backend,
                )

            shutil.rmtree(staged / ".shader_locks", ignore_errors=True)
            verify_shader_backends(staged, backends)
            verify_shader_coherence(
                staged,
                backends,
                host_output,
                staged / RENDER_PLUGIN_INPUT_ID_MARKER,
            )
            if prepare_staged is not None:
                prepare_staged(staged)

            current_artifacts = collect_artifacts()
            current_artifact_hashes = {
                name: _sha256_file(source)
                for name, source in sorted(current_artifacts.items())
            }
            if current_artifact_hashes != source_artifact_hashes:
                raise ShaderVariantError(
                    "Build artifacts changed while the extension root was staged"
                )
            verify_shader_backends(target_dir.parent, backends)
            verify_shader_coherence(
                target_dir.parent,
                backends,
                host_output,
                plugin_marker,
            )

            generation_fields = ("input_id", "build_id")
            for backend in backends:
                expected = tuple(
                    source_shader_manifests[backend][field]
                    for field in generation_fields
                )
                staged_generation = tuple(
                    _shader_manifest(staged, backend)[field]
                    for field in generation_fields
                )
                current_generation = tuple(
                    _shader_manifest(target_dir.parent, backend)[field]
                    for field in generation_fields
                )
                if staged_generation != expected or current_generation != expected:
                    raise ShaderVariantError(
                        f"Shader backend changed while staging: {backend}"
                    )

            publisher(staged, destination)
            published = True
            return copied_files
        finally:
            if not published and staged.exists():
                shutil.rmtree(staged, ignore_errors=True)
