from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
from collections.abc import Callable, Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any

if os.name == "nt":
    import msvcrt
else:
    import fcntl


SOURCE_SCHEMA_VERSION = 2
RUNTIME_SCHEMA_VERSION = 1
RUNTIME_MANIFEST_NAME = "shader_manifest.json"
HOST_INPUT_ID_MARKER = ".shader_input_id"
RENDER_PLUGIN_INPUT_ID_MARKER = "rbc_render_plugin.input_id"
CACHE_SCHEMA_VERSION = 2
MAX_SCENE_FEATURES_PER_FAMILY = 16

_IDENTIFIER_RE = re.compile(r"^[a-z][a-z0-9_]*$")
_BACKEND_RE = re.compile(r"^[a-z][a-z0-9_-]*$")
_MACRO_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


class ShaderVariantError(RuntimeError):
    pass


class _ShaderInputsChanged(RuntimeError):
    pass


class CrossProcessFileLock:
    def __init__(
        self,
        path: Path,
        *,
        timeout_seconds: float = 600.0,
        poll_seconds: float = 0.05,
    ) -> None:
        self.path = path
        self.timeout_seconds = timeout_seconds
        self.poll_seconds = poll_seconds
        self._stream: Any = None

    def __enter__(self) -> CrossProcessFileLock:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        deadline = time.monotonic() + self.timeout_seconds
        stream = None
        while stream is None:
            candidate = None
            try:
                candidate = self.path.open("a+b", buffering=0)
                candidate.seek(0, os.SEEK_END)
                if candidate.tell() == 0:
                    candidate.write(b"\0")
                stream = candidate
            except OSError as error:
                if candidate is not None:
                    candidate.close()
                if time.monotonic() >= deadline:
                    raise ShaderVariantError(
                        f"Timed out initializing shader build lock: {self.path}"
                    ) from error
                time.sleep(self.poll_seconds)
        while True:
            try:
                stream.seek(0)
                if os.name == "nt":
                    msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
                else:
                    fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                self._stream = stream
                return self
            except OSError as error:
                if time.monotonic() >= deadline:
                    stream.close()
                    raise ShaderVariantError(
                        f"Timed out waiting for shader build lock: {self.path}"
                    ) from error
                time.sleep(self.poll_seconds)

    def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None:
        stream = self._stream
        self._stream = None
        if stream is None:
            return
        try:
            stream.seek(0)
            if os.name == "nt":
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(stream.fileno(), fcntl.LOCK_UN)
        finally:
            stream.close()


@dataclass(frozen=True)
class SceneFeatures:
    required: tuple[str, ...]
    forbidden: tuple[str, ...]


@dataclass(frozen=True)
class DimensionValue:
    defines: tuple[tuple[str, str | None], ...]
    scene_features: SceneFeatures


@dataclass(frozen=True)
class Dimension:
    default: str
    values: Mapping[str, DimensionValue]


@dataclass(frozen=True)
class Permutation:
    identifier: str
    selection: Mapping[str, str]


@dataclass(frozen=True)
class VariantSet:
    default: str
    permutations: tuple[Permutation, ...]

    def default_permutation(self) -> Permutation:
        for permutation in self.permutations:
            if permutation.identifier == self.default:
                return permutation
        raise ShaderVariantError(f"Variant set default not found: {self.default}")


@dataclass(frozen=True)
class VariantRule:
    selection: Mapping[str, str]
    required_features: tuple[str, ...]
    forbidden_features: tuple[str, ...]

    def matches(self, enabled_features: frozenset[str]) -> bool:
        return all(
            feature in enabled_features for feature in self.required_features
        ) and all(
            feature not in enabled_features for feature in self.forbidden_features
        )


@dataclass(frozen=True)
class Program:
    identifier: str
    source: str
    source_identifier: str
    variant_set: str
    host_abi: str
    defines: tuple[tuple[str, str | None], ...]

    @property
    def uses_bulk_compilation(self) -> bool:
        return self.identifier == self.source_identifier and not self.defines


@dataclass(frozen=True)
class ShaderVariantConfig:
    manifest_path: Path
    shader_root: Path
    source_root: str
    include_dirs: tuple[str, ...]
    optimization: str
    common_defines: tuple[tuple[str, str | None], ...]
    backends: tuple[str, ...]
    scene_features: tuple[str, ...]
    dimensions: Mapping[str, Dimension]
    variant_sets: Mapping[str, VariantSet]
    programs: tuple[Program, ...]

    @property
    def source_dir(self) -> Path:
        return self.shader_root / self.source_root

    def program_map(self) -> dict[str, Program]:
        return {program.identifier: program for program in self.programs}

    def family_programs(self, variant_set: str) -> tuple[Program, ...]:
        return tuple(
            program
            for program in self.programs
            if program.variant_set == variant_set
        )

    def variant_rule(
        self, variant_set: str, permutation: Permutation
    ) -> VariantRule:
        required: set[str] = set()
        forbidden: set[str] = set()
        for dimension_name, value_name in sorted(permutation.selection.items()):
            scene_features = self.dimensions[dimension_name].values[
                value_name
            ].scene_features
            required.update(scene_features.required)
            forbidden.update(scene_features.forbidden)
        conflicts = required & forbidden
        if conflicts:
            raise ShaderVariantError(
                f"Variant set {variant_set} permutation {permutation.identifier} "
                "both requires and forbids scene features: "
                f"{', '.join(sorted(conflicts))}"
            )
        return VariantRule(
            selection=dict(sorted(permutation.selection.items())),
            required_features=tuple(sorted(required)),
            forbidden_features=tuple(sorted(forbidden)),
        )

    def variant_rules(self, variant_set: str) -> tuple[VariantRule, ...]:
        return tuple(
            self.variant_rule(variant_set, permutation)
            for permutation in self.variant_sets[variant_set].permutations
        )

    def source_files(self) -> dict[str, Path]:
        programs: dict[str, Path] = {}
        for source in sorted(self.source_dir.rglob("*.cpp")):
            if not source.is_file():
                continue
            logical = source.relative_to(self.source_dir).with_suffix("").as_posix()
            programs[logical] = source
        return programs

    def default_selection(self) -> dict[str, str]:
        return {
            dimension_name: dimension.default
            for dimension_name, dimension in sorted(self.dimensions.items())
        }

    def effective_selection(self, selection: Mapping[str, str]) -> dict[str, str]:
        selected_values = self.default_selection()
        selected_values.update(selection)
        return dict(sorted(selected_values.items()))

    def effective_defines(
        self, selection: Mapping[str, str]
    ) -> tuple[tuple[str, str | None], ...]:
        selected_values = self.effective_selection(selection)
        merged: dict[str, str | None] = dict(self.common_defines)
        owners: dict[str, str] = {name: "compile.defines" for name in merged}

        for dimension_name in sorted(self.dimensions):
            dimension = self.dimensions[dimension_name]
            value_name = selected_values[dimension_name]
            value = dimension.values[value_name]
            for macro, macro_value in value.defines:
                if macro in merged:
                    raise ShaderVariantError(
                        f"Macro {macro} is defined by both {owners[macro]} and "
                        f"dimension {dimension_name}"
                    )
                merged[macro] = macro_value
                owners[macro] = f"dimension {dimension_name}"
        return tuple(sorted(merged.items()))

    def effective_program_defines(
        self,
        program: Program,
        selection: Mapping[str, str],
    ) -> tuple[tuple[str, str | None], ...]:
        merged = dict(self.effective_defines(selection))
        for macro, macro_value in program.defines:
            if macro in merged:
                raise ShaderVariantError(
                    f"Macro {macro} is defined by compile/dimension defines "
                    f"and program {program.identifier} defines"
                )
            merged[macro] = macro_value
        return tuple(sorted(merged.items()))


CompilerRunner = Callable[[Sequence[str], Path], None]


def _require_object(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ShaderVariantError(f"{context} must be an object")
    return value


def _require_list(value: Any, context: str) -> list[Any]:
    if not isinstance(value, list):
        raise ShaderVariantError(f"{context} must be an array")
    return value


def _require_string(value: Any, context: str) -> str:
    if not isinstance(value, str) or not value:
        raise ShaderVariantError(f"{context} must be a non-empty string")
    return value


def _check_keys(value: Mapping[str, Any], allowed: set[str], context: str) -> None:
    unknown = sorted(set(value) - allowed)
    if unknown:
        raise ShaderVariantError(f"Unknown {context} keys: {', '.join(unknown)}")


def _safe_relative(value: str, context: str) -> str:
    if "\\" in value or ":" in value:
        raise ShaderVariantError(f"{context} must use a project-relative POSIX path")
    path = PurePosixPath(value)
    if path.is_absolute() or not path.parts or any(part in ("", ".", "..") for part in path.parts):
        raise ShaderVariantError(f"Unsafe {context}: {value}")
    return path.as_posix()


def _identifier(value: Any, context: str) -> str:
    identifier = _require_string(value, context)
    if not _IDENTIFIER_RE.fullmatch(identifier):
        raise ShaderVariantError(f"Invalid {context}: {identifier}")
    return identifier


def _parse_defines(value: Any, context: str) -> tuple[tuple[str, str | None], ...]:
    raw = _require_object(value, context)
    defines: list[tuple[str, str | None]] = []
    for macro, macro_value in sorted(raw.items()):
        if not _MACRO_RE.fullmatch(macro):
            raise ShaderVariantError(f"Invalid macro name in {context}: {macro}")
        if macro_value is not None and not isinstance(macro_value, str):
            raise ShaderVariantError(f"Macro value for {macro} must be a string or null")
        defines.append((macro, macro_value))
    return tuple(defines)


def _parse_feature_list(value: Any, context: str) -> tuple[str, ...]:
    features: list[str] = []
    for feature_value in _require_list(value, context):
        feature = _identifier(feature_value, context)
        if feature in features:
            raise ShaderVariantError(f"Duplicate scene feature in {context}: {feature}")
        features.append(feature)
    return tuple(sorted(features))


def _parse_scene_features(value: Any, context: str) -> SceneFeatures:
    raw = _require_object(value, context)
    _check_keys(raw, {"required", "forbidden"}, context)
    required = _parse_feature_list(raw.get("required", []), f"{context} required")
    forbidden = _parse_feature_list(
        raw.get("forbidden", []), f"{context} forbidden"
    )
    conflicts = set(required) & set(forbidden)
    if conflicts:
        raise ShaderVariantError(
            f"{context} both requires and forbids: {', '.join(sorted(conflicts))}"
        )
    return SceneFeatures(required=required, forbidden=forbidden)


def _validate_variant_rules(
    variant_set: str, rules: Sequence[VariantRule]
) -> None:
    involved_features = sorted(
        {
            feature
            for rule in rules
            for feature in (*rule.required_features, *rule.forbidden_features)
        }
    )
    if len(involved_features) > MAX_SCENE_FEATURES_PER_FAMILY:
        raise ShaderVariantError(
            f"Variant set {variant_set} uses {len(involved_features)} scene "
            f"features; exhaustive validation supports at most "
            f"{MAX_SCENE_FEATURES_PER_FAMILY}"
        )
    for enabled_values in itertools.product(
        (False, True), repeat=len(involved_features)
    ):
        enabled = frozenset(
            feature
            for feature, is_enabled in zip(involved_features, enabled_values)
            if is_enabled
        )
        matches = [rule for rule in rules if rule.matches(enabled)]
        if len(matches) == 1:
            continue
        feature_description = ", ".join(sorted(enabled)) or "<none>"
        if matches:
            selections = ", ".join(
                canonical_selection(rule.selection) for rule in matches
            )
            raise ShaderVariantError(
                f"Variant set {variant_set} has ambiguous scene feature rules for "
                f"[{feature_description}]: {selections}"
            )
        raise ShaderVariantError(
            f"Variant set {variant_set} has no scene feature rule for "
            f"[{feature_description}]"
        )


def canonical_selection(selection: Mapping[str, str]) -> str:
    if not selection:
        return "default"
    return "+".join(f"{name}={selection[name]}" for name in sorted(selection))


def _canonical_json(value: Any) -> bytes:
    return json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("ascii")


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
    temporary.write_text(
        json.dumps(value, indent=2, ensure_ascii=True) + "\n", encoding="utf-8"
    )
    os.replace(temporary, path)


def load_config(manifest_path: Path) -> ShaderVariantConfig:
    manifest_path = manifest_path.resolve()
    try:
        raw = _require_object(
            json.loads(manifest_path.read_text(encoding="utf-8")), "manifest"
        )
    except (OSError, json.JSONDecodeError) as error:
        raise ShaderVariantError(f"Cannot read shader variant manifest: {error}") from error

    _check_keys(
        raw,
        {
            "schema_version",
            "backends",
            "compile",
            "scene_features",
            "dimensions",
            "variant_sets",
            "programs",
        },
        "manifest",
    )
    if raw.get("schema_version") != SOURCE_SCHEMA_VERSION:
        raise ShaderVariantError(
            f"Unsupported shader variant schema: {raw.get('schema_version')}"
        )

    backends: list[str] = []
    for backend_value in _require_list(raw.get("backends"), "backends"):
        backend = _require_string(backend_value, "backend")
        if not _BACKEND_RE.fullmatch(backend):
            raise ShaderVariantError(f"Invalid backend: {backend}")
        if backend in backends:
            raise ShaderVariantError(f"Duplicate backend: {backend}")
        backends.append(backend)
    if not backends:
        raise ShaderVariantError("At least one backend is required")

    compile_raw = _require_object(raw.get("compile"), "compile")
    _check_keys(
        compile_raw,
        {"source_root", "include_dirs", "optimization", "defines"},
        "compile",
    )
    source_root = _safe_relative(
        _require_string(compile_raw.get("source_root"), "compile.source_root"),
        "source root",
    )
    include_dirs = tuple(
        _safe_relative(_require_string(value, "include directory"), "include directory")
        for value in _require_list(compile_raw.get("include_dirs"), "compile.include_dirs")
    )
    if not include_dirs:
        raise ShaderVariantError("At least one shader include directory is required")
    optimization = _require_string(
        compile_raw.get("optimization"), "compile.optimization"
    ).lower()
    if optimization not in ("on", "off"):
        raise ShaderVariantError("compile.optimization must be 'on' or 'off'")
    common_defines = _parse_defines(compile_raw.get("defines", {}), "compile.defines")
    scene_features = _parse_feature_list(
        raw.get("scene_features"), "scene_features"
    )
    declared_scene_features = set(scene_features)

    dimensions: dict[str, Dimension] = {}
    for name_value, dimension_value in sorted(
        _require_object(raw.get("dimensions"), "dimensions").items()
    ):
        name = _identifier(name_value, "dimension name")
        dimension_raw = _require_object(dimension_value, f"dimension {name}")
        _check_keys(dimension_raw, {"default", "values"}, f"dimension {name}")
        default = _identifier(dimension_raw.get("default"), f"dimension {name} default")
        values: dict[str, DimensionValue] = {}
        for value_name_raw, value_raw_value in sorted(
            _require_object(
                dimension_raw.get("values"), f"dimension {name} values"
            ).items()
        ):
            value_name = _identifier(value_name_raw, f"dimension {name} value")
            value_raw = _require_object(
                value_raw_value, f"dimension {name} value {value_name}"
            )
            _check_keys(
                value_raw,
                {"defines", "scene_features"},
                f"dimension {name} value {value_name}",
            )
            dimension_value_parsed = DimensionValue(
                defines=_parse_defines(
                    value_raw.get("defines", {}),
                    f"dimension {name} value {value_name} defines",
                ),
                scene_features=_parse_scene_features(
                    value_raw.get("scene_features", {}),
                    f"dimension {name} value {value_name} scene_features",
                ),
            )
            unknown_features = (
                set(dimension_value_parsed.scene_features.required)
                | set(dimension_value_parsed.scene_features.forbidden)
            ) - declared_scene_features
            if unknown_features:
                raise ShaderVariantError(
                    f"Dimension {name} value {value_name} references undeclared "
                    f"scene features: {', '.join(sorted(unknown_features))}"
                )
            values[value_name] = dimension_value_parsed
        if default not in values:
            raise ShaderVariantError(
                f"Default value {default} is missing from dimension {name}"
            )
        dimensions[name] = Dimension(default=default, values=values)

    variant_sets: dict[str, VariantSet] = {}
    for set_name_raw, set_value in sorted(
        _require_object(raw.get("variant_sets"), "variant_sets").items()
    ):
        set_name = _identifier(set_name_raw, "variant set name")
        set_raw = _require_object(set_value, f"variant set {set_name}")
        _check_keys(set_raw, {"default", "permutations"}, f"variant set {set_name}")
        default = _identifier(set_raw.get("default"), f"variant set {set_name} default")
        permutations: list[Permutation] = []
        permutation_ids: set[str] = set()
        selection_keys: set[str] | None = None
        canonical_keys: set[str] = set()
        for index, permutation_value in enumerate(
            _require_list(
                set_raw.get("permutations"), f"variant set {set_name} permutations"
            )
        ):
            permutation_raw = _require_object(
                permutation_value, f"variant set {set_name} permutation {index}"
            )
            _check_keys(
                permutation_raw,
                {"id", "select"},
                f"variant set {set_name} permutation {index}",
            )
            permutation_id = _identifier(
                permutation_raw.get("id"), f"variant set {set_name} permutation id"
            )
            if permutation_id in permutation_ids:
                raise ShaderVariantError(
                    f"Duplicate permutation id {permutation_id} in variant set {set_name}"
                )
            permutation_ids.add(permutation_id)
            selection_raw = _require_object(
                permutation_raw.get("select"),
                f"variant set {set_name} permutation {permutation_id} select",
            )
            selection: dict[str, str] = {}
            for dimension_name_raw, value_name_raw in sorted(selection_raw.items()):
                dimension_name = _identifier(dimension_name_raw, "selected dimension")
                value_name = _identifier(value_name_raw, "selected dimension value")
                if dimension_name not in dimensions:
                    raise ShaderVariantError(
                        f"Unknown dimension {dimension_name} in variant set {set_name}"
                    )
                if value_name not in dimensions[dimension_name].values:
                    raise ShaderVariantError(
                        f"Unknown value {value_name} for dimension {dimension_name}"
                    )
                selection[dimension_name] = value_name
            if not selection:
                raise ShaderVariantError(
                    f"Variant set {set_name} permutations must select a dimension"
                )
            current_keys = set(selection)
            if selection_keys is None:
                selection_keys = current_keys
            elif current_keys != selection_keys:
                raise ShaderVariantError(
                    f"All permutations in variant set {set_name} must select the same dimensions"
                )
            canonical_key = canonical_selection(selection)
            if canonical_key in canonical_keys:
                raise ShaderVariantError(
                    f"Duplicate selection {canonical_key} in variant set {set_name}"
                )
            canonical_keys.add(canonical_key)
            permutations.append(Permutation(permutation_id, selection))
        if default not in permutation_ids:
            raise ShaderVariantError(
                f"Default permutation {default} is missing from variant set {set_name}"
            )
        variant_set = VariantSet(default=default, permutations=tuple(permutations))
        default_permutation = variant_set.default_permutation()
        for dimension_name, value_name in default_permutation.selection.items():
            if dimensions[dimension_name].default != value_name:
                raise ShaderVariantError(
                    f"Default permutation of {set_name} must use the global default "
                    f"for dimension {dimension_name}"
                )
        variant_sets[set_name] = variant_set

    shader_root = manifest_path.parent
    programs: list[Program] = []
    program_ids: set[str] = set()
    for index, program_value in enumerate(
        _require_list(raw.get("programs"), "programs")
    ):
        program_raw = _require_object(program_value, f"program {index}")
        _check_keys(
            program_raw,
            {"id", "source", "variant_set", "host_abi", "defines"},
            f"program {index}",
        )
        identifier = _safe_relative(
            _require_string(program_raw.get("id"), f"program {index} id"),
            f"program {index} id",
        )
        source = _safe_relative(
            _require_string(program_raw.get("source"), f"program {identifier} source"),
            f"program {identifier} source",
        )
        variant_set_name = _identifier(
            program_raw.get("variant_set"), f"program {identifier} variant_set"
        )
        host_abi = _require_string(
            program_raw.get("host_abi"), f"program {identifier} host_abi"
        )
        if host_abi not in ("stable", "per_variant"):
            raise ShaderVariantError(
                f"Program {identifier} host_abi must be 'stable' or 'per_variant'"
            )
        if variant_set_name not in variant_sets:
            raise ShaderVariantError(
                f"Program {identifier} references unknown variant set {variant_set_name}"
            )
        if identifier in program_ids:
            raise ShaderVariantError(f"Duplicate program id: {identifier}")
        program_ids.add(identifier)

        source_path = (shader_root / source).resolve()
        source_dir = (shader_root / source_root).resolve()
        try:
            source_relative = source_path.relative_to(source_dir)
        except ValueError as error:
            raise ShaderVariantError(
                f"Program {identifier} source is outside compile.source_root"
            ) from error
        if source_relative.suffix != ".cpp":
            raise ShaderVariantError(f"Program {identifier} source must be a .cpp file")
        source_identifier = source_relative.with_suffix("").as_posix()
        if not source_path.is_file():
            raise ShaderVariantError(f"Program source does not exist: {source}")
        defines = _parse_defines(
            program_raw.get("defines", {}),
            f"program {identifier} defines",
        )
        programs.append(
            Program(
                identifier,
                source,
                source_identifier,
                variant_set_name,
                host_abi,
                defines,
            )
        )

    config = ShaderVariantConfig(
        manifest_path=manifest_path,
        shader_root=shader_root,
        source_root=source_root,
        include_dirs=include_dirs,
        optimization=optimization,
        common_defines=common_defines,
        backends=tuple(backends),
        scene_features=scene_features,
        dimensions=dimensions,
        variant_sets=variant_sets,
        programs=tuple(programs),
    )
    if not config.source_dir.is_dir():
        raise ShaderVariantError(f"Shader source root does not exist: {config.source_dir}")
    for include_dir in config.include_dirs:
        include_path = config.shader_root / include_dir
        if not include_path.is_dir():
            raise ShaderVariantError(
                f"Shader include directory does not exist: {include_path}"
            )
    source_programs = config.source_files()
    if not source_programs:
        raise ShaderVariantError("No shader .cpp files were found")
    for program in config.programs:
        if program.source_identifier not in source_programs:
            raise ShaderVariantError(
                f"Program source was not discovered below source_root: {program.source}"
            )
        if (
            program.identifier in source_programs
            and program.identifier != program.source_identifier
        ):
            raise ShaderVariantError(
                f"Program id {program.identifier} collides with a different physical "
                "shader source"
            )
    config.effective_defines({})
    for variant_set in config.variant_sets.values():
        for permutation in variant_set.permutations:
            config.effective_defines(permutation.selection)
    for program in config.programs:
        variant_set = config.variant_sets[program.variant_set]
        for permutation in variant_set.permutations:
            config.effective_program_defines(program, permutation.selection)
    used_scene_features: set[str] = set()
    for variant_set_name in config.variant_sets:
        if not config.family_programs(variant_set_name):
            raise ShaderVariantError(
                f"Variant set {variant_set_name} is not referenced by any program"
            )
        rules = config.variant_rules(variant_set_name)
        _validate_variant_rules(variant_set_name, rules)
        for rule in rules:
            used_scene_features.update(rule.required_features)
            used_scene_features.update(rule.forbidden_features)
    unused_scene_features = declared_scene_features - used_scene_features
    if unused_scene_features:
        raise ShaderVariantError(
            "Declared scene features are not used by any variant set: "
            f"{', '.join(sorted(unused_scene_features))}"
        )
    return config


def _tree_digest(config: ShaderVariantConfig) -> str:
    roots = [config.source_dir]
    roots.extend(config.shader_root / include_dir for include_dir in config.include_dirs)
    files: dict[str, Path] = {
        config.manifest_path.relative_to(config.shader_root).as_posix(): config.manifest_path
    }
    for root in roots:
        for path in root.rglob("*"):
            if path.is_file():
                files[path.relative_to(config.shader_root).as_posix()] = path
    digest = hashlib.sha256()
    for relative, path in sorted(files.items()):
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def _snapshot_config(
    config: ShaderVariantConfig, snapshot_root: Path
) -> ShaderVariantConfig:
    """Copy exactly the inputs covered by _tree_digest into an immutable build root."""
    snapshot_root.mkdir(parents=True, exist_ok=True)
    relative_roots = {config.source_root, *config.include_dirs}
    for relative in sorted(relative_roots):
        shutil.copytree(
            config.shader_root / relative,
            snapshot_root / relative,
            dirs_exist_ok=True,
        )
    manifest_relative = config.manifest_path.relative_to(config.shader_root)
    snapshot_manifest = snapshot_root / manifest_relative
    snapshot_manifest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(config.manifest_path, snapshot_manifest)
    return load_config(snapshot_manifest)


def _build_input_id(
    dependency_digest: str, compiler_info: Mapping[str, Any]
) -> str:
    payload = {
        "compiler": compiler_info["sha256"],
        "dependency_digest": dependency_digest,
        "source_schema": SOURCE_SCHEMA_VERSION,
    }
    return hashlib.sha256(_canonical_json(payload)).hexdigest()


def compiler_fingerprint(compiler_path: Path) -> dict[str, Any]:
    compiler_path = compiler_path.resolve()
    if not compiler_path.is_file():
        raise ShaderVariantError(f"Shader compiler not found: {compiler_path}")
    related = [compiler_path]
    for pattern_value in ("*.dll", "*.so", "*.dylib", "template.txt"):
        related.extend(compiler_path.parent.glob(pattern_value))
    unique = sorted({path.resolve() for path in related}, key=lambda path: path.name)
    digest = hashlib.sha256()
    files: dict[str, str] = {}
    for path in unique:
        file_hash = _sha256_file(path)
        files[path.name] = file_hash
        digest.update(path.name.encode("utf-8"))
        digest.update(b"\0")
        digest.update(file_hash.encode("ascii"))
        digest.update(b"\0")
    fingerprint = digest.hexdigest()
    return {"id": fingerprint[:16], "sha256": fingerprint, "files": files}


def _run_compiler(command: Sequence[str], cwd: Path) -> None:
    subprocess.run(list(command), cwd=cwd, check=True)


def _compiler_command(
    config: ShaderVariantConfig,
    compiler_path: Path,
    input_path: Path,
    output_path: Path,
    defines: Sequence[tuple[str, str | None]],
    *,
    backend: str | None = None,
    cache_dir: Path | None = None,
    hostgen_path: Path | None = None,
    rebuild: bool = False,
    lsp: bool = False,
) -> list[str]:
    command = [
        str(compiler_path),
        f"--in={input_path}",
        f"--out={output_path}",
    ]
    for include_dir in config.include_dirs:
        command.append(f"--include={config.shader_root / include_dir}")
    for macro, value in defines:
        command.append(f"--D={macro}" if value is None else f"--D={macro}={value}")
    if backend is not None:
        command.append(f"--backend={backend}")
        command.append(f"--opt={config.optimization}")
    if cache_dir is not None:
        command.append(f"--cache_dir={cache_dir}")
    if hostgen_path is not None:
        command.append(f"--hostgen={hostgen_path}")
    if rebuild:
        command.append("--rebuild")
    if lsp:
        command.append("--lsp")
    return command


def _compile_program_host_interface(
    config: ShaderVariantConfig,
    program: Program,
    selection: Mapping[str, str],
    *,
    compiler_path: Path,
    output_path: Path,
    work_dir: Path,
    cache_dir: Path,
    rebuild: bool,
    runner: CompilerRunner,
) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.unlink(missing_ok=True)
    binary_path = work_dir / "program-out" / f"{program.identifier}.bin"
    binary_path.parent.mkdir(parents=True, exist_ok=True)
    command = _compiler_command(
        config,
        compiler_path,
        config.shader_root / program.source,
        binary_path,
        config.effective_program_defines(program, selection),
        cache_dir=cache_dir,
        hostgen_path=output_path,
        rebuild=rebuild,
    )
    runner(command, work_dir)


def _normalize_host_interface(content: str, shader_root: Path) -> bytes:
    normalized = content.replace("\r\n", "\n").replace("\r", "\n")
    root_values = {
        str(shader_root.resolve()),
        str(shader_root.resolve()).replace("\\", "/"),
        str(shader_root.resolve()).replace("/", "\\"),
    }
    for root_value in sorted(root_values, key=len, reverse=True):
        normalized = normalized.replace(root_value, "<shader-root>")
    lines = [line.rstrip() for line in normalized.split("\n")]
    while lines and not lines[-1]:
        lines.pop()
    return ("\n".join(lines) + "\n").encode("utf-8")


def _stable_abi_selections(
    config: ShaderVariantConfig,
) -> list[tuple[str, Mapping[str, str], tuple[tuple[str, str | None], ...]]]:
    selections: list[
        tuple[str, Mapping[str, str], tuple[tuple[str, str | None], ...]]
    ] = []
    seen_defines: set[tuple[tuple[str, str | None], ...]] = set()

    default_defines = config.effective_defines({})
    seen_defines.add(default_defines)
    selections.append(("default", {}, default_defines))
    for set_name, variant_set in sorted(config.variant_sets.items()):
        for permutation in sorted(
            variant_set.permutations,
            key=lambda value: canonical_selection(value.selection),
        ):
            defines = config.effective_defines(permutation.selection)
            if defines in seen_defines:
                continue
            seen_defines.add(defines)
            selections.append(
                (
                    f"{set_name}:{canonical_selection(permutation.selection)}",
                    permutation.selection,
                    defines,
                )
            )
    return selections


def validate_stable_host_abi(
    config: ShaderVariantConfig,
    *,
    cache_root: Path,
    compiler_path: Path,
    compiler_info: Mapping[str, Any],
    dependency_digest: str,
    rebuild: bool = False,
    runner: CompilerRunner = _run_compiler,
) -> None:
    stable_programs = tuple(
        program for program in config.programs if program.host_abi == "stable"
    )
    if not stable_programs:
        return
    selections = _stable_abi_selections(config)
    validation_payload = {
        "cache_schema": CACHE_SCHEMA_VERSION,
        "compiler": compiler_info["sha256"],
        "dependency_digest": dependency_digest,
        "programs": [program.identifier for program in stable_programs],
        "define_sets": [list(defines) for _, _, defines in selections],
        "program_define_sets": [
            {
                program.identifier: list(
                    config.effective_program_defines(program, selection)
                )
                for program in stable_programs
            }
            for _, selection, _ in selections
        ],
    }
    validation_key = hashlib.sha256(_canonical_json(validation_payload)).hexdigest()
    validation_path = cache_root / "host_abi" / f"{validation_key}.json"
    expected_programs = {program.identifier for program in stable_programs}
    expected_define_sets = [label for label, _, _ in selections]

    def cache_valid() -> bool:
        if rebuild or not validation_path.is_file():
            return False
        try:
            cached = json.loads(validation_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return False
        return (
            isinstance(cached, dict)
            and cached.get("validation_key") == validation_key
            and isinstance(cached.get("programs"), dict)
            and set(cached["programs"]) == expected_programs
            and cached.get("define_sets") == expected_define_sets
        )

    lock_path = cache_root / "locks" / f"host-abi-{validation_key}.lock"
    with CrossProcessFileLock(lock_path):
        if cache_valid():
            return
        work_parent = cache_root / "host_abi_work"
        work_parent.mkdir(parents=True, exist_ok=True)
        work_dir = Path(
            tempfile.mkdtemp(prefix=f"{validation_key[:12]}.", dir=work_parent)
        )
        reference: dict[str, bytes] | None = None
        reference_label = ""
        interface_hashes: dict[str, str] = {}
        try:
            for index, (label, selection, defines) in enumerate(selections):
                selection_root = work_dir / str(index)
                host_dir = selection_root / "host"
                host_dir.mkdir(parents=True)
                define_key = hashlib.sha256(
                    _canonical_json(list(defines))
                ).hexdigest()[:16]
                compiler_cache = selection_root / f"compiler-cache-{define_key}"
                compiler_cache.mkdir(parents=True, exist_ok=True)
                command = _compiler_command(
                    config,
                    compiler_path,
                    config.source_dir,
                    selection_root / "out",
                    defines,
                    cache_dir=compiler_cache,
                    hostgen_path=host_dir,
                    rebuild=rebuild,
                )
                runner(command, selection_root)
                for program in stable_programs:
                    if program.uses_bulk_compilation:
                        continue
                    _compile_program_host_interface(
                        config,
                        program,
                        selection,
                        compiler_path=compiler_path,
                        output_path=host_dir / f"{program.identifier}.inl",
                        work_dir=selection_root,
                        cache_dir=compiler_cache,
                        rebuild=rebuild,
                        runner=runner,
                    )
                current: dict[str, bytes] = {}
                for program in stable_programs:
                    generated = host_dir / f"{program.identifier}.inl"
                    if not generated.is_file():
                        raise ShaderVariantError(
                            f"Hostgen did not produce {program.identifier}.inl "
                            f"for {label}"
                        )
                    current[program.identifier] = _normalize_host_interface(
                        generated.read_text(encoding="utf-8"), config.shader_root
                    )
                if reference is None:
                    reference = current
                    reference_label = label
                    interface_hashes = {
                        program: hashlib.sha256(content).hexdigest()
                        for program, content in current.items()
                    }
                    continue
                for program in stable_programs:
                    identifier = program.identifier
                    if current[identifier] != reference[identifier]:
                        raise ShaderVariantError(
                            f"Stable host ABI changed for {identifier}: "
                            f"{reference_label} != {label}"
                        )
            _write_json(
                validation_path,
                {
                    "validation_key": validation_key,
                    "programs": interface_hashes,
                    "define_sets": expected_define_sets,
                },
            )
        finally:
            shutil.rmtree(work_dir, ignore_errors=True)


def _remove_path(path: Path) -> None:
    if path.is_dir():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()


def _directory_backup_path(destination: Path) -> Path:
    return destination.with_name(f".{destination.name}.backup")


def recover_directory_backup(destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    backup = _directory_backup_path(destination)
    if not backup.exists():
        return
    if destination.exists():
        _remove_path(backup)
    else:
        os.replace(backup, destination)


def _atomic_replace_directory(staged: Path, destination: Path) -> None:
    recover_directory_backup(destination)
    backup = _directory_backup_path(destination)
    had_destination = destination.exists()
    if had_destination:
        os.replace(destination, backup)
    try:
        os.replace(staged, destination)
    except BaseException:
        if had_destination and backup.exists() and not destination.exists():
            os.replace(backup, destination)
        raise
    else:
        if backup.exists():
            try:
                _remove_path(backup)
            except OSError:
                # The new directory is already committed. Keep the backup as a
                # recovery journal and retry cleanup on the next publication.
                pass


def _cache_record_valid(object_dir: Path, compile_key: str) -> bool:
    artifact = object_dir / "shader.bin"
    metadata_path = object_dir / "metadata.json"
    if not artifact.is_file() or not metadata_path.is_file():
        return False
    try:
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    return (
        metadata.get("compile_key") == compile_key
        and metadata.get("size") == artifact.stat().st_size
        and metadata.get("sha256") == _sha256_file(artifact)
    )


def _default_tree_cache_valid(
    object_dir: Path, compile_key: str, expected_artifacts: Sequence[str]
) -> bool:
    tree = object_dir / "tree"
    metadata_path = object_dir / "metadata.json"
    if not tree.is_dir() or not metadata_path.is_file():
        return False
    try:
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    artifacts = metadata.get("artifacts")
    if (
        metadata.get("compile_key") != compile_key
        or not isinstance(artifacts, dict)
        or set(artifacts) != set(expected_artifacts)
    ):
        return False
    for relative in expected_artifacts:
        artifact = tree / relative
        record = artifacts.get(relative)
        if not artifact.is_file() or not isinstance(record, dict):
            return False
        if (
            record.get("size") != artifact.stat().st_size
            or record.get("sha256") != _sha256_file(artifact)
        ):
            return False
    return True


def _materialize_default_tree(
    config: ShaderVariantConfig,
    *,
    backend: str,
    compiler_path: Path,
    compiler_info: Mapping[str, Any],
    default_compile_key: str,
    default_defines: Sequence[tuple[str, str | None]],
    expected_artifacts: Sequence[str],
    cache_root: Path,
    destination: Path,
    rebuild: bool,
    runner: CompilerRunner,
) -> None:
    object_dir = cache_root / "default_objects" / default_compile_key
    lock_path = cache_root / "locks" / f"default-{default_compile_key}.lock"
    with CrossProcessFileLock(lock_path):
        if rebuild or not _default_tree_cache_valid(
            object_dir, default_compile_key, expected_artifacts
        ):
            work_parent = cache_root / "default_work"
            work_parent.mkdir(parents=True, exist_ok=True)
            work_dir = Path(
                tempfile.mkdtemp(
                    prefix=f"{default_compile_key[:12]}.", dir=work_parent
                )
            )
            compiled_tree = work_dir / "tree"
            compiler_cache = (
                cache_root
                / "compiler"
                / compiler_info["id"]
                / backend
                / "default"
                / default_compile_key
            )
            compiler_cache.mkdir(parents=True, exist_ok=True)
            try:
                command = _compiler_command(
                    config,
                    compiler_path,
                    config.source_dir,
                    compiled_tree,
                    default_defines,
                    backend=backend,
                    cache_dir=compiler_cache,
                    rebuild=rebuild,
                )
                runner(command, work_dir)
                artifact_records: dict[str, dict[str, Any]] = {}
                for relative in expected_artifacts:
                    artifact = compiled_tree / relative
                    if not artifact.is_file():
                        raise ShaderVariantError(
                            f"Compiler did not produce default shader {relative}"
                        )
                    artifact_records[relative] = {
                        "sha256": _sha256_file(artifact),
                        "size": artifact.stat().st_size,
                    }
                publish_parent = cache_root / "default_publish"
                publish_parent.mkdir(parents=True, exist_ok=True)
                publish_dir = Path(
                    tempfile.mkdtemp(
                        prefix=f"{default_compile_key[:12]}.", dir=publish_parent
                    )
                )
                try:
                    shutil.copytree(compiled_tree, publish_dir / "tree")
                    _write_json(
                        publish_dir / "metadata.json",
                        {
                            "compile_key": default_compile_key,
                            "artifacts": artifact_records,
                        },
                    )
                    object_dir.parent.mkdir(parents=True, exist_ok=True)
                    _atomic_replace_directory(publish_dir, object_dir)
                finally:
                    if publish_dir.exists():
                        shutil.rmtree(publish_dir, ignore_errors=True)
            finally:
                shutil.rmtree(work_dir, ignore_errors=True)
        destination.mkdir(parents=True, exist_ok=True)
        shutil.copytree(object_dir / "tree", destination, dirs_exist_ok=True)


def _host_tree_cache_valid(
    object_dir: Path, hostgen_key: str, required_interfaces: Sequence[str]
) -> bool:
    tree = object_dir / "tree"
    metadata_path = object_dir / "metadata.json"
    if not tree.is_dir() or not metadata_path.is_file():
        return False
    try:
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return False
    files = metadata.get("files")
    if metadata.get("hostgen_key") != hostgen_key or not isinstance(files, dict):
        return False
    if not set(required_interfaces).issubset(files):
        return False
    for relative, record in files.items():
        if not isinstance(relative, str) or not isinstance(record, dict):
            return False
        try:
            safe_relative = _safe_relative(relative, "cached host interface")
        except ShaderVariantError:
            return False
        generated = tree / safe_relative
        if (
            not generated.is_file()
            or record.get("size") != generated.stat().st_size
            or record.get("sha256") != _sha256_file(generated)
        ):
            return False
    return True


def _materialize_hostgen_tree(
    config: ShaderVariantConfig,
    *,
    compiler_path: Path,
    hostgen_key: str,
    default_defines: Sequence[tuple[str, str | None]],
    selection: Mapping[str, str],
    required_programs: Sequence[Program],
    cache_root: Path,
    destination: Path,
    rebuild: bool,
    runner: CompilerRunner,
) -> None:
    required_interfaces = tuple(
        f"{program.identifier}.inl" for program in required_programs
    )
    object_dir = cache_root / "host_objects" / hostgen_key
    lock_path = cache_root / "locks" / f"host-object-{hostgen_key}.lock"
    with CrossProcessFileLock(lock_path):
        if rebuild or not _host_tree_cache_valid(
            object_dir, hostgen_key, required_interfaces
        ):
            compiler_cache = (
                destination.parent
                / f"compiler-cache-{hostgen_key[:16]}"
            )
            compiler_cache.mkdir(parents=True, exist_ok=True)
            command = _compiler_command(
                config,
                compiler_path,
                config.source_dir,
                destination.parent / "out",
                default_defines,
                cache_dir=compiler_cache,
                hostgen_path=destination,
                rebuild=rebuild,
            )
            runner(command, destination.parent)
            for program in required_programs:
                if program.uses_bulk_compilation:
                    continue
                _compile_program_host_interface(
                    config,
                    program,
                    selection,
                    compiler_path=compiler_path,
                    output_path=destination / f"{program.identifier}.inl",
                    work_dir=destination.parent,
                    cache_dir=compiler_cache,
                    rebuild=rebuild,
                    runner=runner,
                )
            for relative in required_interfaces:
                if not (destination / relative).is_file():
                    raise ShaderVariantError(
                        f"Hostgen did not produce required interface "
                        f"{relative} in this invocation"
                    )
            generated_files = sorted(
                path for path in destination.rglob("*.inl") if path.is_file()
            )
            if not generated_files:
                raise ShaderVariantError("Hostgen did not produce any host interfaces")
            file_records = {
                path.relative_to(destination).as_posix(): {
                    "sha256": _sha256_file(path),
                    "size": path.stat().st_size,
                }
                for path in generated_files
            }
            publish_parent = cache_root / "host_publish"
            publish_parent.mkdir(parents=True, exist_ok=True)
            publish_dir = Path(
                tempfile.mkdtemp(prefix=f"{hostgen_key[:12]}.", dir=publish_parent)
            )
            try:
                shutil.copytree(destination, publish_dir / "tree")
                _write_json(
                    publish_dir / "metadata.json",
                    {
                        "hostgen_key": hostgen_key,
                        "files": file_records,
                    },
                )
                object_dir.parent.mkdir(parents=True, exist_ok=True)
                _atomic_replace_directory(publish_dir, object_dir)
            finally:
                if publish_dir.exists():
                    shutil.rmtree(publish_dir, ignore_errors=True)
        else:
            shutil.copytree(object_dir / "tree", destination, dirs_exist_ok=True)


def _compile_variant(
    config: ShaderVariantConfig,
    program: Program,
    permutation: Permutation,
    *,
    backend: str,
    compiler_path: Path,
    compiler_info: Mapping[str, Any],
    dependency_digest: str,
    cache_root: Path,
    destination: Path,
    rebuild: bool,
    runner: CompilerRunner,
) -> str:
    defines = config.effective_program_defines(
        program, permutation.selection
    )
    compile_payload = {
        "cache_schema": CACHE_SCHEMA_VERSION,
        "compiler": compiler_info["sha256"],
        "backend": backend,
        "optimization": config.optimization,
        "source": program.source,
        "include_dirs": list(config.include_dirs),
        "defines": list(defines),
        "dependency_digest": dependency_digest,
    }
    compile_key = hashlib.sha256(_canonical_json(compile_payload)).hexdigest()
    object_dir = cache_root / "objects" / compile_key
    lock_path = cache_root / "locks" / f"variant-{compile_key}.lock"
    with CrossProcessFileLock(lock_path):
        if rebuild or not _cache_record_valid(object_dir, compile_key):
            work_parent = cache_root / "work"
            work_parent.mkdir(parents=True, exist_ok=True)
            work_dir = Path(
                tempfile.mkdtemp(prefix=f"{compile_key[:12]}.", dir=work_parent)
            )
            try:
                compiled = work_dir / "shader.bin"
                command = _compiler_command(
                    config,
                    compiler_path,
                    config.shader_root / program.source,
                    compiled,
                    defines,
                    backend=backend,
                )
                runner(command, work_dir)
                if not compiled.is_file():
                    raise ShaderVariantError(
                        f"Compiler did not produce variant {program.identifier} "
                        f"({canonical_selection(permutation.selection)})"
                    )
                publish_parent = cache_root / "publish"
                publish_parent.mkdir(parents=True, exist_ok=True)
                publish_dir = Path(
                    tempfile.mkdtemp(
                        prefix=f"{compile_key[:12]}.", dir=publish_parent
                    )
                )
                try:
                    artifact = publish_dir / "shader.bin"
                    shutil.copy2(compiled, artifact)
                    _write_json(
                        publish_dir / "metadata.json",
                        {
                            "compile_key": compile_key,
                            "sha256": _sha256_file(artifact),
                            "size": artifact.stat().st_size,
                        },
                    )
                    object_dir.parent.mkdir(parents=True, exist_ok=True)
                    _atomic_replace_directory(publish_dir, object_dir)
                finally:
                    if publish_dir.exists():
                        shutil.rmtree(publish_dir, ignore_errors=True)
            finally:
                shutil.rmtree(work_dir, ignore_errors=True)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(object_dir / "shader.bin", destination)
    return compile_key


def _artifact_record(
    root: Path,
    selection: Mapping[str, str],
    artifact: str,
    compile_key: str | None = None,
) -> dict[str, Any]:
    artifact_path = root / artifact
    if not artifact_path.is_file():
        raise ShaderVariantError(f"Missing shader artifact: {artifact}")
    record: dict[str, Any] = {
        "selection": dict(sorted(selection.items())),
        "artifact": artifact,
        "sha256": _sha256_file(artifact_path),
        "size": artifact_path.stat().st_size,
    }
    if selection:
        record["canonical_key"] = canonical_selection(selection)
    if compile_key is not None:
        record["compile_key"] = compile_key
    return record


def _runtime_families(config: ShaderVariantConfig) -> dict[str, Any]:
    families: dict[str, Any] = {}
    for family_name in sorted(config.variant_sets):
        families[family_name] = {
            "programs": [
                program.identifier
                for program in config.family_programs(family_name)
            ],
            "rules": [
                {
                    "selection": dict(rule.selection),
                    "required_features": list(rule.required_features),
                    "forbidden_features": list(rule.forbidden_features),
                }
                for rule in config.variant_rules(family_name)
            ],
        }
    return families


def _runtime_selection(value: Any, context: str) -> dict[str, str]:
    raw = _require_object(value, context)
    selection: dict[str, str] = {}
    for dimension_raw, value_raw in raw.items():
        dimension = _identifier(dimension_raw, f"{context} dimension")
        selected_value = _identifier(value_raw, f"{context} value")
        selection[dimension] = selected_value
    return dict(sorted(selection.items()))


def _runtime_feature_list(value: Any, context: str) -> tuple[str, ...]:
    raw = _require_list(value, context)
    features = _parse_feature_list(raw, context)
    if raw != list(features):
        raise ShaderVariantError(f"{context} must be sorted")
    return features


def verify_shader_root(shader_root: Path, expected_backend: str | None = None) -> dict[str, Any]:
    shader_root = shader_root.resolve()
    manifest_path = shader_root / RUNTIME_MANIFEST_NAME
    try:
        manifest = _require_object(
            json.loads(manifest_path.read_text(encoding="utf-8")), "runtime manifest"
        )
    except (OSError, json.JSONDecodeError) as error:
        raise ShaderVariantError(f"Cannot read runtime shader manifest: {error}") from error
    for required in (
        "schema_version",
        "backend",
        "build_id",
        "input_id",
        "programs",
        "families",
    ):
        if required not in manifest:
            raise ShaderVariantError(f"Runtime shader manifest is missing {required}")
    if manifest["schema_version"] != RUNTIME_SCHEMA_VERSION:
        raise ShaderVariantError(
            f"Unsupported runtime shader schema: {manifest['schema_version']}"
        )
    backend = _require_string(manifest["backend"], "runtime backend")
    if expected_backend is not None and backend != expected_backend:
        raise ShaderVariantError(
            f"Runtime manifest backend is {backend}, expected {expected_backend}"
        )
    _require_string(manifest["build_id"], "runtime build_id")
    input_id = _require_string(manifest["input_id"], "runtime input_id")
    if not re.fullmatch(r"[0-9a-f]{64}", input_id):
        raise ShaderVariantError("Runtime input_id must be a lowercase SHA-256 digest")
    programs = _require_object(manifest["programs"], "runtime programs")
    if not programs:
        raise ShaderVariantError("Runtime manifest has no programs")

    artifacts: set[str] = set()
    program_selections: dict[str, set[str]] = {}
    program_defaults: dict[str, dict[str, str]] = {}
    for logical_raw, program_value in programs.items():
        logical = _safe_relative(logical_raw, "runtime program id")
        program = _require_object(program_value, f"runtime program {logical}")
        _check_keys(
            program,
            {"default_selection", "variants"},
            f"runtime program {logical}",
        )
        default_selection = _runtime_selection(
            program.get("default_selection"),
            f"runtime program {logical} default_selection",
        )
        variants = _require_list(
            program.get("variants"), f"runtime program {logical} variants"
        )
        if not variants:
            raise ShaderVariantError(f"Runtime program {logical} has no variants")
        found_default = False
        selection_keys: set[str] = set()
        for variant_value in variants:
            variant = _require_object(
                variant_value, f"runtime program {logical} variant"
            )
            selection = _runtime_selection(
                variant.get("selection"),
                f"runtime program {logical} variant selection",
            )
            allowed_variant_keys = {
                "selection",
                "artifact",
                "sha256",
                "size",
                "compile_key",
            }
            if selection:
                allowed_variant_keys.add("canonical_key")
            _check_keys(
                variant,
                allowed_variant_keys,
                f"runtime program {logical} variant",
            )
            canonical = canonical_selection(selection)
            if canonical in selection_keys:
                raise ShaderVariantError(
                    f"Runtime program {logical} has duplicate selection {canonical}"
                )
            selection_keys.add(canonical)
            if selection == default_selection:
                found_default = True
            if selection:
                if variant.get("canonical_key") != canonical:
                    raise ShaderVariantError(
                        f"Runtime program {logical} has invalid canonical_key for "
                        f"{canonical}"
                    )
            compile_key = _require_string(
                variant.get("compile_key"),
                f"runtime program {logical} variant compile_key",
            )
            if not re.fullmatch(r"[0-9a-f]{64}", compile_key):
                raise ShaderVariantError(
                    f"Invalid runtime compile_key for {logical} ({canonical})"
                )
            artifact = _safe_relative(
                _require_string(
                    variant.get("artifact"),
                    f"runtime program {logical} variant artifact",
                ),
                f"runtime program {logical} artifact",
            )
            if artifact in artifacts:
                raise ShaderVariantError(f"Duplicate runtime artifact: {artifact}")
            artifacts.add(artifact)
            artifact_path = (shader_root / artifact).resolve()
            try:
                artifact_path.relative_to(shader_root)
            except ValueError as error:
                raise ShaderVariantError(f"Artifact escapes shader root: {artifact}") from error
            if not artifact_path.is_file():
                raise ShaderVariantError(f"Missing runtime shader artifact: {artifact}")
            expected_size = variant.get("size")
            expected_hash = variant.get("sha256")
            if (
                not isinstance(expected_size, int)
                or isinstance(expected_size, bool)
                or expected_size < 0
            ):
                raise ShaderVariantError(f"Invalid artifact size for {artifact}")
            if artifact_path.stat().st_size != expected_size:
                raise ShaderVariantError(f"Artifact size mismatch: {artifact}")
            if (
                not isinstance(expected_hash, str)
                or not re.fullmatch(r"[0-9a-f]{64}", expected_hash)
                or _sha256_file(artifact_path) != expected_hash
            ):
                raise ShaderVariantError(f"Artifact hash mismatch: {artifact}")
        if not found_default:
            raise ShaderVariantError(
                f"Runtime program {logical} default_selection has no matching variant"
            )
        program_selections[logical] = selection_keys
        program_defaults[logical] = default_selection

    families = _require_object(manifest["families"], "runtime families")
    family_programs: set[str] = set()
    for family_name_raw, family_value in families.items():
        family_name = _identifier(family_name_raw, "runtime family name")
        family = _require_object(family_value, f"runtime family {family_name}")
        _check_keys(
            family, {"programs", "rules"}, f"runtime family {family_name}"
        )
        members: list[str] = []
        for member_value in _require_list(
            family.get("programs"), f"runtime family {family_name} programs"
        ):
            member = _safe_relative(
                _require_string(
                    member_value, f"runtime family {family_name} program"
                ),
                f"runtime family {family_name} program",
            )
            if member in members:
                raise ShaderVariantError(
                    f"Runtime family {family_name} repeats program {member}"
                )
            if member not in programs:
                raise ShaderVariantError(
                    f"Runtime family {family_name} references unknown program {member}"
                )
            if member in family_programs:
                raise ShaderVariantError(
                    f"Runtime program {member} belongs to multiple families"
                )
            members.append(member)
            family_programs.add(member)
        if not members:
            raise ShaderVariantError(
                f"Runtime family {family_name} contains no programs"
            )

        rules: list[VariantRule] = []
        selection_dimensions: set[str] | None = None
        rule_selections: set[str] = set()
        for rule_value in _require_list(
            family.get("rules"), f"runtime family {family_name} rules"
        ):
            rule_raw = _require_object(
                rule_value, f"runtime family {family_name} rule"
            )
            _check_keys(
                rule_raw,
                {"selection", "required_features", "forbidden_features"},
                f"runtime family {family_name} rule",
            )
            selection = _runtime_selection(
                rule_raw.get("selection"),
                f"runtime family {family_name} rule selection",
            )
            if not selection:
                raise ShaderVariantError(
                    f"Runtime family {family_name} rules must select a dimension"
                )
            current_dimensions = set(selection)
            if selection_dimensions is None:
                selection_dimensions = current_dimensions
            elif current_dimensions != selection_dimensions:
                raise ShaderVariantError(
                    f"Runtime family {family_name} rules select different dimensions"
                )
            canonical = canonical_selection(selection)
            if canonical in rule_selections:
                raise ShaderVariantError(
                    f"Runtime family {family_name} repeats selection {canonical}"
                )
            rule_selections.add(canonical)
            required_features = _runtime_feature_list(
                rule_raw.get("required_features"),
                f"runtime family {family_name} required_features",
            )
            forbidden_features = _runtime_feature_list(
                rule_raw.get("forbidden_features"),
                f"runtime family {family_name} forbidden_features",
            )
            conflicts = set(required_features) & set(forbidden_features)
            if conflicts:
                raise ShaderVariantError(
                    f"Runtime family {family_name} both requires and forbids: "
                    f"{', '.join(sorted(conflicts))}"
                )
            rules.append(
                VariantRule(selection, required_features, forbidden_features)
            )
        if not rules:
            raise ShaderVariantError(f"Runtime family {family_name} has no rules")
        _validate_variant_rules(family_name, rules)
        for member in members:
            if program_selections[member] != rule_selections:
                raise ShaderVariantError(
                    f"Runtime family {family_name} rules do not match artifacts for "
                    f"program {member}"
                )
            if canonical_selection(program_defaults[member]) not in rule_selections:
                raise ShaderVariantError(
                    f"Runtime family {family_name} default does not match program "
                    f"{member} artifacts"
                )

    for logical in programs:
        if logical in family_programs:
            continue
        if program_defaults[logical] or program_selections[logical] != {"default"}:
            raise ShaderVariantError(
                f"Runtime program {logical} has variants but belongs to no family"
            )
    return manifest


def verify_shader_backends(
    build_root: Path, backends: Sequence[str]
) -> dict[str, dict[str, Any]]:
    build_root = build_root.resolve()
    verified: dict[str, dict[str, Any]] = {}
    for backend in backends:
        verified[backend] = verify_shader_root(
            build_root / f"shader_build_{backend}", backend
        )
    input_ids = {manifest["input_id"] for manifest in verified.values()}
    if len(input_ids) > 1:
        generations = ", ".join(
            f"{backend}={manifest['input_id']}"
            for backend, manifest in sorted(verified.items())
        )
        raise ShaderVariantError(
            f"Shader backends were built from different inputs: {generations}"
        )
    return verified


def _read_input_id_marker(path: Path, context: str) -> str:
    try:
        input_id = path.read_text(encoding="ascii").strip()
    except OSError as error:
        raise ShaderVariantError(
            f"Cannot read {context} input marker {path}: {error}"
        ) from error
    if not re.fullmatch(r"[0-9a-f]{64}", input_id):
        raise ShaderVariantError(
            f"{context} input marker must contain a lowercase SHA-256 digest: {path}"
        )
    return input_id


def verify_shader_coherence(
    build_root: Path,
    backends: Sequence[str],
    host_output: Path,
    plugin_marker: Path | None = None,
) -> str:
    """Require backend, hostgen, and optionally render-plugin generations to match."""
    backends = tuple(backends)
    if not backends:
        raise ShaderVariantError("At least one shader backend is required")
    manifests = verify_shader_backends(build_root, backends)
    expected = manifests[backends[0]]["input_id"]
    components = {
        **{
            f"backend {backend}": manifest["input_id"]
            for backend, manifest in manifests.items()
        },
        "hostgen": _read_input_id_marker(
            host_output.resolve() / HOST_INPUT_ID_MARKER, "hostgen"
        ),
    }
    if plugin_marker is not None:
        components["render plugin"] = _read_input_id_marker(
            plugin_marker.resolve(), "render plugin"
        )
    mismatches = {
        component: input_id
        for component, input_id in components.items()
        if input_id != expected
    }
    if mismatches:
        generations = ", ".join(
            f"{component}={input_id}"
            for component, input_id in sorted(components.items())
        )
        raise ShaderVariantError(
            f"Shader build generations do not match: {generations}"
        )
    return expected


def install_verified_shader_root(
    source: Path, destination: Path, expected_backend: str
) -> Path:
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


def _build_backend_locked(
    config: ShaderVariantConfig,
    *,
    backend: str,
    build_root: Path,
    cache_root: Path,
    compiler_path: Path,
    compiler_info: Mapping[str, Any],
    dependency_digest: str,
    destination: Path,
    validate_before_publish: Callable[[], None] | None = None,
    rebuild: bool = False,
    runner: CompilerRunner = _run_compiler,
) -> Path:
    recover_directory_backup(destination)
    staged = Path(
        tempfile.mkdtemp(prefix=f".shader_build_{backend}.", dir=build_root)
    )
    published = False
    try:
        default_defines = config.effective_defines({})
        default_compile_payload = {
            "cache_schema": CACHE_SCHEMA_VERSION,
            "compiler": compiler_info["sha256"],
            "backend": backend,
            "optimization": config.optimization,
            "source_root": config.source_root,
            "include_dirs": list(config.include_dirs),
            "defines": list(default_defines),
            "dependency_digest": dependency_digest,
        }
        default_compile_key = hashlib.sha256(
            _canonical_json(default_compile_payload)
        ).hexdigest()
        source_programs = config.source_files()
        expected_default_artifacts = [
            f"{logical}.bin" for logical in sorted(source_programs)
        ]
        _materialize_default_tree(
            config,
            backend=backend,
            compiler_path=compiler_path,
            compiler_info=compiler_info,
            default_compile_key=default_compile_key,
            default_defines=default_defines,
            expected_artifacts=expected_default_artifacts,
            cache_root=cache_root,
            destination=staged,
            rebuild=rebuild,
            runner=runner,
        )

        declared_programs = config.program_map()
        logical_sources = dict(source_programs)
        for program in config.programs:
            logical_sources[program.identifier] = (
                config.shader_root / program.source
            )
        runtime_programs: dict[str, Any] = {}
        build_inputs: list[Any] = []
        for logical, source in sorted(logical_sources.items()):
            default_artifact = f"{logical}.bin"
            declared = declared_programs.get(logical)
            if declared is None:
                default_selection: dict[str, str] = {}
                variants = [
                    _artifact_record(
                        staged,
                        default_selection,
                        default_artifact,
                        default_compile_key,
                    )
                ]
            else:
                variant_set = config.variant_sets[declared.variant_set]
                default_permutation = variant_set.default_permutation()
                default_selection = dict(
                    sorted(default_permutation.selection.items())
                )
                if declared.uses_bulk_compilation:
                    default_compile_key_for_program = default_compile_key
                else:
                    default_compile_key_for_program = _compile_variant(
                        config,
                        declared,
                        default_permutation,
                        backend=backend,
                        compiler_path=compiler_path,
                        compiler_info=compiler_info,
                        dependency_digest=dependency_digest,
                        cache_root=cache_root,
                        destination=staged / default_artifact,
                        rebuild=rebuild,
                        runner=runner,
                    )
                variants = [
                    _artifact_record(
                        staged,
                        default_selection,
                        default_artifact,
                        default_compile_key_for_program,
                    )
                ]
                non_default = sorted(
                    (
                        permutation
                        for permutation in variant_set.permutations
                        if permutation.identifier != variant_set.default
                    ),
                    key=lambda permutation: canonical_selection(permutation.selection),
                )
                for permutation in non_default:
                    canonical_key = canonical_selection(permutation.selection)
                    artifact = f"variants/{canonical_key}/{logical}.bin"
                    runtime_selection = dict(
                        sorted(permutation.selection.items())
                    )
                    compile_key = _compile_variant(
                        config,
                        declared,
                        permutation,
                        backend=backend,
                        compiler_path=compiler_path,
                        compiler_info=compiler_info,
                        dependency_digest=dependency_digest,
                        cache_root=cache_root,
                        destination=staged / artifact,
                        rebuild=rebuild,
                        runner=runner,
                    )
                    variants.append(
                        _artifact_record(
                            staged,
                            runtime_selection,
                            artifact,
                            compile_key,
                        )
                    )
            runtime_programs[logical] = {
                "default_selection": default_selection,
                "variants": variants,
            }
            build_inputs.append(
                {
                    "program": logical,
                    "source": source.relative_to(config.shader_root).as_posix(),
                    "variants": [
                        {
                            "selection": variant["selection"],
                            "artifact": variant["artifact"],
                            "compile_key": variant["compile_key"],
                            "sha256": variant["sha256"],
                            "size": variant["size"],
                        }
                        for variant in variants
                    ],
                }
            )

        build_id_payload = {
            "backend": backend,
            "compiler": compiler_info["sha256"],
            "dependency_digest": dependency_digest,
            "programs": build_inputs,
            "source_schema": SOURCE_SCHEMA_VERSION,
        }
        runtime_manifest = {
            "schema_version": RUNTIME_SCHEMA_VERSION,
            "backend": backend,
            "build_id": hashlib.sha256(_canonical_json(build_id_payload)).hexdigest(),
            "input_id": _build_input_id(dependency_digest, compiler_info),
            "compiler": compiler_info,
            "programs": runtime_programs,
            "families": _runtime_families(config),
        }
        _write_json(staged / RUNTIME_MANIFEST_NAME, runtime_manifest)
        verify_shader_root(staged, backend)
        if validate_before_publish is not None:
            validate_before_publish()
        _atomic_replace_directory(staged, destination)
        published = True
        return destination
    finally:
        if not published and staged.exists():
            shutil.rmtree(staged, ignore_errors=True)


def build_backend(
    config: ShaderVariantConfig,
    *,
    backend: str,
    build_root: Path,
    cache_root: Path,
    compiler_path: Path,
    rebuild: bool = False,
    runner: CompilerRunner = _run_compiler,
) -> Path:
    build_root = build_root.resolve()
    cache_root = cache_root.resolve()
    compiler_path = compiler_path.resolve()
    build_root.mkdir(parents=True, exist_ok=True)
    cache_root.mkdir(parents=True, exist_ok=True)
    compiler_info = compiler_fingerprint(compiler_path)
    destination = build_root / f"shader_build_{backend}"
    publish_lock = build_root / ".shader_locks" / f"shader_build_{backend}.lock"
    with CrossProcessFileLock(publish_lock):
        snapshot_parent = cache_root / "input_snapshots"
        snapshot_parent.mkdir(parents=True, exist_ok=True)
        for attempt in range(2):
            live_config = load_config(config.manifest_path)
            if backend not in live_config.backends:
                raise ShaderVariantError(
                    f"Backend {backend} is not declared by the manifest"
                )
            snapshot_root = Path(
                tempfile.mkdtemp(prefix=f"{backend}.", dir=snapshot_parent)
            )
            try:
                snapshot_config = _snapshot_config(live_config, snapshot_root)
                dependency_digest = _tree_digest(snapshot_config)
                validate_stable_host_abi(
                    snapshot_config,
                    cache_root=cache_root,
                    compiler_path=compiler_path,
                    compiler_info=compiler_info,
                    dependency_digest=dependency_digest,
                    rebuild=rebuild,
                    runner=runner,
                )

                def validate_inputs() -> None:
                    current_config = load_config(config.manifest_path)
                    if _tree_digest(current_config) != dependency_digest:
                        raise _ShaderInputsChanged

                try:
                    return _build_backend_locked(
                        snapshot_config,
                        backend=backend,
                        build_root=build_root,
                        cache_root=cache_root,
                        compiler_path=compiler_path,
                        compiler_info=compiler_info,
                        dependency_digest=dependency_digest,
                        destination=destination,
                        validate_before_publish=validate_inputs,
                        rebuild=rebuild,
                        runner=runner,
                    )
                except _ShaderInputsChanged:
                    if attempt == 1:
                        raise ShaderVariantError(
                            "Shader inputs changed repeatedly during compilation; "
                            "the previous shader build was preserved"
                        ) from None
            finally:
                shutil.rmtree(snapshot_root, ignore_errors=True)
    raise AssertionError("unreachable")


def build_hostgen(
    config: ShaderVariantConfig,
    *,
    host_output: Path,
    cache_root: Path,
    compiler_path: Path,
    rebuild: bool = False,
    runner: CompilerRunner = _run_compiler,
) -> Path:
    host_output = host_output.resolve()
    cache_root = cache_root.resolve()
    compiler_path = compiler_path.resolve()
    host_output.parent.mkdir(parents=True, exist_ok=True)
    cache_root.mkdir(parents=True, exist_ok=True)
    compiler_info = compiler_fingerprint(compiler_path)
    output_key = hashlib.sha256(
        str(host_output).casefold().encode("utf-8")
    ).hexdigest()
    host_lock = cache_root / "locks" / f"host-publish-{output_key}.lock"
    with CrossProcessFileLock(host_lock):
        recover_directory_backup(host_output)
        snapshot_parent = cache_root / "input_snapshots"
        snapshot_parent.mkdir(parents=True, exist_ok=True)
        for attempt in range(2):
            live_config = load_config(config.manifest_path)
            snapshot_root = Path(
                tempfile.mkdtemp(prefix="host.", dir=snapshot_parent)
            )
            try:
                snapshot_config = _snapshot_config(live_config, snapshot_root)
                dependency_digest = _tree_digest(snapshot_config)
                validate_stable_host_abi(
                    snapshot_config,
                    cache_root=cache_root,
                    compiler_path=compiler_path,
                    compiler_info=compiler_info,
                    dependency_digest=dependency_digest,
                    rebuild=rebuild,
                    runner=runner,
                )
                default_defines = snapshot_config.effective_defines({})
                stable_programs = tuple(
                    program
                    for program in snapshot_config.programs
                    if program.host_abi == "stable"
                )
                per_variant_families = {
                    variant_set_name: tuple(
                        program
                        for program in snapshot_config.family_programs(
                            variant_set_name
                        )
                        if program.host_abi == "per_variant"
                    )
                    for variant_set_name in sorted(snapshot_config.variant_sets)
                }
                per_variant_families = {
                    family: programs
                    for family, programs in per_variant_families.items()
                    if programs
                }
                default_required_programs = list(stable_programs)
                for family_name, programs in per_variant_families.items():
                    variant_set = snapshot_config.variant_sets[family_name]
                    for permutation in variant_set.permutations:
                        defines = snapshot_config.effective_defines(
                            permutation.selection
                        )
                        if defines == default_defines:
                            default_required_programs.extend(programs)
                default_required_programs = sorted(
                    {
                        program.identifier: program
                        for program in default_required_programs
                    }.values(),
                    key=lambda program: program.identifier,
                )
                hostgen_payload = {
                    "cache_schema": CACHE_SCHEMA_VERSION,
                    "compiler": compiler_info["sha256"],
                    "dependency_digest": dependency_digest,
                    "defines": list(default_defines),
                    "programs": [
                        {
                            "id": program.identifier,
                            "source": program.source,
                            "defines": list(
                                snapshot_config.effective_program_defines(
                                    program, {}
                                )
                            ),
                        }
                        for program in default_required_programs
                    ],
                }
                hostgen_key = hashlib.sha256(
                    _canonical_json(hostgen_payload)
                ).hexdigest()
                work_dir = Path(
                    tempfile.mkdtemp(
                        prefix=".shader_hostgen.", dir=host_output.parent
                    )
                )
                generated_host = work_dir / "generated"
                generated_host.mkdir()
                try:
                    _materialize_hostgen_tree(
                        snapshot_config,
                        compiler_path=compiler_path,
                        hostgen_key=hostgen_key,
                        default_defines=default_defines,
                        selection={},
                        required_programs=default_required_programs,
                        cache_root=cache_root,
                        destination=generated_host,
                        rebuild=rebuild,
                        runner=runner,
                    )
                    for family_name, programs in per_variant_families.items():
                        variant_set = snapshot_config.variant_sets[family_name]
                        for permutation in variant_set.permutations:
                            defines = snapshot_config.effective_defines(
                                permutation.selection
                            )
                            variant_payload = {
                                "cache_schema": CACHE_SCHEMA_VERSION,
                                "compiler": compiler_info["sha256"],
                                "dependency_digest": dependency_digest,
                                "host_abi": "per_variant",
                                "variant_set": family_name,
                                "permutation": permutation.identifier,
                                "selection": dict(permutation.selection),
                                "programs": [
                                    program.identifier for program in programs
                                ],
                                "defines": list(defines),
                                "program_defines": {
                                    program.identifier: list(
                                        snapshot_config.effective_program_defines(
                                            program, permutation.selection
                                        )
                                    )
                                    for program in programs
                                },
                            }
                            variant_hostgen_key = hashlib.sha256(
                                _canonical_json(variant_payload)
                            ).hexdigest()
                            variant_interfaces = [
                                f"{program.identifier}.inl"
                                for program in programs
                            ]
                            if defines == default_defines:
                                raw_host = generated_host
                            else:
                                raw_host = (
                                    work_dir
                                    / "per_variant"
                                    / family_name
                                    / permutation.identifier
                                )
                                raw_host.mkdir(parents=True)
                                _materialize_hostgen_tree(
                                    snapshot_config,
                                    compiler_path=compiler_path,
                                    hostgen_key=variant_hostgen_key,
                                    default_defines=defines,
                                    selection=permutation.selection,
                                    required_programs=programs,
                                    cache_root=cache_root,
                                    destination=raw_host,
                                    rebuild=rebuild,
                                    runner=runner,
                                )
                            variant_destination = (
                                generated_host
                                / "variants"
                                / family_name
                                / permutation.identifier
                            )
                            for relative in variant_interfaces:
                                source = raw_host / relative
                                destination = variant_destination / relative
                                destination.parent.mkdir(parents=True, exist_ok=True)
                                shutil.copy2(source, destination)

                    for programs in per_variant_families.values():
                        for program in programs:
                            _remove_path(
                                generated_host / f"{program.identifier}.inl"
                            )

                    merged_host = work_dir / "merged"
                    merged_host.mkdir()
                    if host_output.is_dir():
                        shutil.copytree(
                            host_output, merged_host, dirs_exist_ok=True
                        )
                    shutil.rmtree(
                        merged_host / "families", ignore_errors=True
                    )
                    shutil.rmtree(
                        merged_host / "variants", ignore_errors=True
                    )
                    for programs in per_variant_families.values():
                        for program in programs:
                            _remove_path(
                                merged_host / f"{program.identifier}.inl"
                            )
                    shutil.copytree(
                        generated_host, merged_host, dirs_exist_ok=True
                    )
                    (merged_host / ".shader_input_id").write_text(
                        _build_input_id(dependency_digest, compiler_info) + "\n",
                        encoding="ascii",
                    )
                    current_config = load_config(config.manifest_path)
                    if _tree_digest(current_config) != dependency_digest:
                        raise _ShaderInputsChanged
                    _atomic_replace_directory(merged_host, host_output)
                    return host_output
                finally:
                    shutil.rmtree(work_dir, ignore_errors=True)
            except _ShaderInputsChanged:
                if attempt == 1:
                    raise ShaderVariantError(
                        "Shader inputs changed repeatedly during host generation; "
                        "the previous host interfaces were preserved"
                    ) from None
            finally:
                shutil.rmtree(snapshot_root, ignore_errors=True)
    raise AssertionError("unreachable")


def generate_lsp(
    config: ShaderVariantConfig,
    *,
    output: Path,
    compiler_path: Path,
    runner: CompilerRunner = _run_compiler,
) -> Path:
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(f".{output.name}.{uuid.uuid4().hex}.tmp")
    command = _compiler_command(
        config,
        compiler_path.resolve(),
        config.shader_root,
        temporary,
        config.effective_defines({}),
        lsp=True,
    )
    try:
        runner(command, output.parent)
        if not temporary.is_file():
            raise ShaderVariantError("Compiler did not produce compile_commands.json")
        os.replace(temporary, output)
        return output
    finally:
        temporary.unlink(missing_ok=True)


def _default_compiler(project_root: Path) -> Path:
    name = "rbcxx.exe" if platform.system() == "Windows" else "rbcxx"
    return project_root / "build" / "tool" / "rbcxx" / name


def _default_build_root(project_root: Path) -> Path:
    system = platform.system().lower()
    platform_name = "macos" if system == "darwin" else system
    machine = platform.machine().lower()
    if machine in ("amd64", "x86_64", "x64"):
        architecture = "x64" if platform_name == "windows" else "x86_x64"
    elif machine in ("aarch64", "arm64"):
        architecture = "arm64"
    elif machine in ("i386", "i686", "x86"):
        architecture = "x86"
    else:
        architecture = machine
    return project_root / "build" / platform_name / architecture


def _resolve_common(args: argparse.Namespace) -> tuple[Path, Path, Path]:
    project_root = Path(args.project_root).resolve()
    manifest = Path(args.manifest)
    if not manifest.is_absolute():
        manifest = project_root / manifest
    compiler = Path(args.compiler) if args.compiler else _default_compiler(project_root)
    if not compiler.is_absolute():
        compiler = project_root / compiler
    return project_root, manifest, compiler


def _add_common_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--project-root", default=".")
    parser.add_argument(
        "--manifest", default="rbc/shader/shader_variants.json"
    )
    parser.add_argument("--compiler")


def _create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate, build, and verify RoboCute shader variants."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    validate_parser = subparsers.add_parser("validate")
    _add_common_arguments(validate_parser)

    build_parser = subparsers.add_parser("build")
    _add_common_arguments(build_parser)
    build_parser.add_argument("--backend", action="append", dest="backends")
    build_parser.add_argument("--build-root")
    build_parser.add_argument("--cache-root")
    build_parser.add_argument("--hostgen", action="store_true")
    build_parser.add_argument("--hostgen-only", action="store_true")
    build_parser.add_argument("--host-out")
    build_parser.add_argument("--rebuild", action="store_true")

    verify_parser = subparsers.add_parser("verify")
    _add_common_arguments(verify_parser)
    verify_parser.add_argument("--shader-root")
    verify_parser.add_argument("--build-root")
    verify_parser.add_argument("--backend")

    coherence_parser = subparsers.add_parser("verify-coherence")
    _add_common_arguments(coherence_parser)
    coherence_parser.add_argument("--build-root")
    coherence_parser.add_argument("--backend", action="append", dest="backends")
    coherence_parser.add_argument("--host-out")
    coherence_parser.add_argument("--plugin-marker")

    lsp_parser = subparsers.add_parser("lsp")
    _add_common_arguments(lsp_parser)
    lsp_parser.add_argument("--out")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = _create_parser()
    args = parser.parse_args(argv)
    try:
        project_root, manifest_path, compiler_path = _resolve_common(args)
        if args.command == "validate":
            load_config(manifest_path)
            print(f"Validated {manifest_path}")
            return 0
        if args.command == "build":
            config = load_config(manifest_path)
            build_root = (
                Path(args.build_root).resolve()
                if args.build_root
                else _default_build_root(project_root)
            )
            cache_root = (
                Path(args.cache_root).resolve()
                if args.cache_root
                else project_root / "build" / ".shader_cache" / "variants-v1"
            )
            if not args.hostgen_only:
                backends = args.backends or list(config.backends)
                for backend in backends:
                    output = build_backend(
                        config,
                        backend=backend,
                        build_root=build_root,
                        cache_root=cache_root,
                        compiler_path=compiler_path,
                        rebuild=args.rebuild,
                    )
                    print(f"Built {backend} shaders: {output}")
            if args.hostgen or args.hostgen_only:
                host_output = (
                    Path(args.host_out).resolve()
                    if args.host_out
                    else config.shader_root / "host"
                )
                output = build_hostgen(
                    config,
                    host_output=host_output,
                    cache_root=cache_root,
                    compiler_path=compiler_path,
                    rebuild=args.rebuild,
                )
                print(f"Generated shader host headers: {output}")
            return 0
        if args.command == "verify":
            if args.shader_root:
                shader_root = Path(args.shader_root)
            else:
                if not args.backend:
                    raise ShaderVariantError(
                        "verify requires --shader-root or --build-root with --backend"
                    )
                build_root = (
                    Path(args.build_root).resolve()
                    if args.build_root
                    else _default_build_root(project_root)
                )
                shader_root = build_root / f"shader_build_{args.backend}"
            manifest = verify_shader_root(shader_root, args.backend)
            print(
                f"Verified {manifest['backend']} shader build {manifest['build_id']}"
            )
            return 0
        if args.command == "verify-coherence":
            config = load_config(manifest_path)
            build_root = (
                Path(args.build_root).resolve()
                if args.build_root
                else _default_build_root(project_root)
            )
            host_output = (
                Path(args.host_out).resolve()
                if args.host_out
                else config.shader_root / "host"
            )
            plugin_marker = (
                Path(args.plugin_marker).resolve()
                if args.plugin_marker
                else None
            )
            input_id = verify_shader_coherence(
                build_root,
                args.backends or config.backends,
                host_output,
                plugin_marker,
            )
            print(f"Verified coherent shader generation {input_id}")
            return 0
        if args.command == "lsp":
            config = load_config(manifest_path)
            output = (
                Path(args.out).resolve()
                if args.out
                else config.shader_root / ".vscode" / "compile_commands.json"
            )
            generate_lsp(
                config, output=output, compiler_path=compiler_path
            )
            print(f"Generated shader compile commands: {output}")
            return 0
        raise ShaderVariantError(f"Unknown command: {args.command}")
    except (ShaderVariantError, subprocess.CalledProcessError) as error:
        print(f"shader-build: error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
