from __future__ import annotations

import hashlib
import shutil
import tempfile
from collections.abc import Callable, Sequence
from pathlib import Path

from rbc_build.shader_variants import (
    CrossProcessFileLock,
    RENDER_PLUGIN_INPUT_ID_MARKER,
    ShaderVariantError,
    _atomic_replace_directory,
    _sha256_file,
    install_verified_shader_root,
    recover_directory_backup,
    verify_shader_backends,
    verify_shader_coherence,
)


MANAGED_ARTIFACT_SUFFIXES = frozenset(
    {".dll", ".pyd", ".bytes", ".input_id"}
)


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
    source_input_id = verify_shader_coherence(
        target_dir.parent,
        backends,
        host_output,
        plugin_marker,
    )
    source_shader_manifests = verify_shader_backends(
        target_dir.parent, backends
    )

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
            staged_shader_manifests = verify_shader_backends(staged, backends)
            staged_input_id = verify_shader_coherence(
                staged,
                backends,
                host_output,
                staged / RENDER_PLUGIN_INPUT_ID_MARKER,
            )
            if staged_input_id != source_input_id:
                raise ShaderVariantError(
                    "Shader generation changed while staging build artifacts"
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
            current_shader_manifests = verify_shader_backends(
                target_dir.parent, backends
            )
            current_input_id = verify_shader_coherence(
                target_dir.parent,
                backends,
                host_output,
                plugin_marker,
            )
            if current_input_id != source_input_id:
                raise ShaderVariantError(
                    "Shader generation changed while the extension root was staged"
                )

            generation_fields = ("input_id", "build_id")
            for backend in backends:
                expected = tuple(
                    source_shader_manifests[backend][field]
                    for field in generation_fields
                )
                staged_generation = tuple(
                    staged_shader_manifests[backend][field]
                    for field in generation_fields
                )
                current_generation = tuple(
                    current_shader_manifests[backend][field]
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
