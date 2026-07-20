"""纯 Python schema 代码生成器。

从 `@reflect` ground-truth（如 `src/rbc_meta/types/project_plugin.py`）生成
**自包含** 的 Python 数据 schema 模块（dataclass + from_dict/to_dict/load/save +
版本常量 + migration）。

产物仅依赖 stdlib（`json` / `dataclasses` / `pathlib`），不得依赖 `rbc_meta`，
因为打包配置 `source-exclude = ["src/rbc_build/*", "src/rbc_meta/*"]` 会在发布时
排除生成器本身。

与 C++ serde 侧行为严格对称：
- `from_dict` 忽略未知 key（前向兼容），缺失 key 落默认值（后向兼容）。
- 不支持 Optional 字段（与 `JsonSerializer` 的约束一致），全部为「普通字段 + 默认值」。
- migration 由版本号驱动，函数体自包含（经 `inspect.getsource` 内嵌）。
"""

import importlib
import inspect
from pathlib import Path
from typing import TYPE_CHECKING, Any, Dict, List, Optional, Tuple

from rbc_meta.utils.reflect import ReflectionRegistry
from rbc_meta.utils.codegen_util import _write_string_to

if TYPE_CHECKING:
    from rbc_meta.utils.codegenx import CodeModule
    from rbc_meta.utils.reflect import ClassInfo, FieldInfo

_IND = " " * 4

# migration 函数允许引用的全局名字（自包含约束，见 docs/design/project_schema.md）
_ALLOWED_MIGRATION_GLOBALS = {
    "dict",
    "list",
    "set",
    "tuple",
    "str",
    "int",
    "float",
    "bool",
    "len",
    "isinstance",
    "sorted",
    "min",
    "max",
    "sum",
    "any",
    "all",
    "enumerate",
    "range",
    "print",
}

_HEADER = """# This File is Generated From Python Def
# Modifying This File will not affect final result, checkout src/rbc_meta/ for real defs
# ================== GENERATED CODE BEGIN ==================
from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

"""

_FOOTER = """
# ================== GENERATED CODE END ==================
"""


def _py_scalar_type(t: Any, registry: ReflectionRegistry) -> str:
    """Map a ground-truth scalar field type to a Python annotation string."""
    if t is str:
        return "str"
    if t is bool:
        return "bool"
    if t is int:
        return "int"
    if t is float:
        return "float"
    py_name = getattr(t, "_py_type_name", None)
    if py_name:
        # builtin aliases like uint -> "int"
        return py_name
    name = getattr(t, "__name__", "") or ""
    if name in ("uint", "ulong", "u8", "u16", "u32", "u64", "size_t", "long", "ubyte"):
        return "int"
    info = registry.get_class_info(name) if name else None
    if info is not None and info.is_enum:
        return "int"
    raise RuntimeError(f"codegen_py_schema: unsupported field type {t!r}")


def _field_py_type(field: "FieldInfo", registry: ReflectionRegistry) -> Tuple[str, str]:
    """Return (python_type_str, kind); kind in {"scalar", "container", "nested"}."""
    gi = field.generic_info
    if gi is not None:
        if gi.is_optional:
            raise RuntimeError(
                f"codegen_py_schema: Optional field '{field.name}' is not allowed "
                "(serde limitation); use a plain field with a default value."
            )
        if gi.is_container or gi.cpp_name:
            if len(gi.args) == 1:
                inner = _py_scalar_type(gi.args[0], registry)
                return f"list[{inner}]", "container"
            raise RuntimeError(
                f"codegen_py_schema: container field '{field.name}' with {len(gi.args)} "
                "type args is not supported (only Vector[T]/List[T])."
            )
    t = field.type
    # 内建别名（uint 等）优先于 registry 查询：它们也可能被注册进 ReflectionRegistry，
    # 但在 schema 中应映射为 Python 标量而非嵌套 dataclass。
    if t is str or t is bool or t is int or t is float:
        return _py_scalar_type(t, registry), "scalar"
    if getattr(t, "_py_type_name", None):
        return _py_scalar_type(t, registry), "scalar"
    info = registry.get_class_info(getattr(t, "__name__", "") or "")
    if info is not None and not info.is_enum:
        return t.__name__, "nested"
    return _py_scalar_type(t, registry), "scalar"


def _field_default_expr(field: "FieldInfo", py_type: str, kind: str) -> str:
    """Return the dataclass default expression for a field declaration."""
    d = field.default
    if kind == "container":
        if d is None:
            return "field(default_factory=list)"
        return f"field(default_factory=lambda: {list(d)!r})"
    if kind == "nested":
        return f"field(default_factory={py_type})"
    # scalar
    if d is None:
        if py_type == "str":
            return '""'
        if py_type == "bool":
            return "False"
        if py_type == "float":
            return "0.0"
        return "0"
    if isinstance(d, bool):
        return str(d)
    if isinstance(d, (int, float)):
        return repr(d)
    if isinstance(d, str):
        return repr(d)
    raise RuntimeError(
        f"codegen_py_schema: unsupported default value {d!r} for field '{field.name}'"
    )


def _gen_dataclass(info: "ClassInfo", registry: ReflectionRegistry, deprecated: Dict[str, str], is_root: bool) -> str:
    """Generate one dataclass definition."""
    lines: List[str] = []
    lines.append("@dataclass")
    lines.append(f"class {info.name}:")
    doc = (info.doc or "").strip()
    if doc:
        lines.append(f'{_IND}"""{doc}"""')
    if not info.fields:
        lines.append(f"{_IND}pass")

    # ---- field declarations ----
    for f in info.fields:
        py_type, kind = _field_py_type(f, registry)
        default_expr = _field_default_expr(f, py_type, kind)
        note = deprecated.get(f"{info.name}.{f.name}")
        comment = f"  # deprecated: {note}" if note else ""
        lines.append(f"{_IND}{f.name}: {py_type} = {default_expr}{comment}")

    # ---- from_dict ----
    lines.append("")
    lines.append(f"{_IND}@classmethod")
    lines.append(f'{_IND}def from_dict(cls, d: dict) -> "{info.name}":')
    lines.append(f'{_IND}{_IND}"""从 dict 构造；忽略未知 key（前向兼容），缺失 key 落默认值（后向兼容）。"""')
    lines.append(f"{_IND}{_IND}if not isinstance(d, dict):")
    lines.append(f"{_IND}{_IND}{_IND}return cls()")
    lines.append(f"{_IND}{_IND}return cls(")
    for f in info.fields:
        py_type, kind = _field_py_type(f, registry)
        if kind == "nested":
            lines.append(
                f'{_IND}{_IND}{_IND}{f.name}={py_type}.from_dict(d.get("{f.name}")) '
                f'if isinstance(d.get("{f.name}"), dict) else {py_type}(),'
            )
        elif kind == "container":
            lines.append(
                f'{_IND}{_IND}{_IND}{f.name}=list(d.get("{f.name}") or []),'
            )
        else:
            default_expr = _field_default_expr(f, py_type, kind)
            lines.append(
                f'{_IND}{_IND}{_IND}{f.name}=d.get("{f.name}", {default_expr}),'
            )
    lines.append(f"{_IND}{_IND})")

    # ---- to_dict ----
    lines.append("")
    lines.append(f"{_IND}def to_dict(self) -> dict:")
    lines.append(f"{_IND}{_IND}return {{")
    for f in info.fields:
        py_type, kind = _field_py_type(f, registry)
        if kind == "nested":
            lines.append(f'{_IND}{_IND}{_IND}"{f.name}": self.{f.name}.to_dict(),')
        elif kind == "container":
            lines.append(f'{_IND}{_IND}{_IND}"{f.name}": list(self.{f.name}),')
        else:
            lines.append(f'{_IND}{_IND}{_IND}"{f.name}": self.{f.name},')
    lines.append(f"{_IND}{_IND}}}")

    # ---- load / save（仅 root schema）----
    if is_root:
        lines.append("")
        lines.append(f"{_IND}@classmethod")
        lines.append(f'{_IND}def load(cls, path) -> "{info.name}":')
        lines.append(f'{_IND}{_IND}"""加载 rbc_project.json（可传文件路径或项目根目录），自动执行版本迁移。"""')
        lines.append(f"{_IND}{_IND}p = Path(path)")
        lines.append(f"{_IND}{_IND}if p.is_dir():")
        lines.append(f'{_IND}{_IND}{_IND}p = p / "rbc_project.json"')
        lines.append(f'{_IND}{_IND}data = json.loads(p.read_text(encoding="utf-8"))')
        lines.append(f"{_IND}{_IND}data = migrate_project_dict(data)")
        lines.append(f"{_IND}{_IND}obj = cls.from_dict(data)")
        lines.append(f"{_IND}{_IND}obj.schema_version = CURRENT_SCHEMA_VERSION")
        lines.append(f"{_IND}{_IND}return obj")
        lines.append("")
        lines.append(f"{_IND}def save(self, path) -> None:")
        lines.append(f'{_IND}{_IND}"""写回 rbc_project.json（可传文件路径或项目根目录），schema_version 记为 CURRENT。"""')
        lines.append(f"{_IND}{_IND}p = Path(path)")
        lines.append(f"{_IND}{_IND}if p.is_dir():")
        lines.append(f'{_IND}{_IND}{_IND}p = p / "rbc_project.json"')
        lines.append(f"{_IND}{_IND}self.schema_version = CURRENT_SCHEMA_VERSION")
        lines.append(f'{_IND}{_IND}p.write_text(json.dumps(self.to_dict(), indent=4, ensure_ascii=False) + "\\n", encoding="utf-8")')

    return "\n".join(lines)


def _gen_migration_section(migrations: Dict[int, Any]) -> str:
    """Inline migration function sources + build the MIGRATIONS table + driver."""
    lines: List[str] = []
    lines.append("")
    lines.append("# ===== migrations（自包含函数，内嵌自 ground-truth）=====")
    names = {}
    for ver, fn in sorted(migrations.items()):
        # co_names 同时包含「全局名」与「属性名」（如 dict.setdefault），
        # 仅当名字能在函数 __globals__ 中解析时才视为外部引用（fail-fast）。
        bad = sorted(
            n
            for n in fn.__code__.co_names
            if n not in _ALLOWED_MIGRATION_GLOBALS and n in fn.__globals__
        )
        if bad:
            raise RuntimeError(
                f"codegen_py_schema: migration '{fn.__name__}' references external "
                f"names {bad}; migrations must be self-contained."
            )
        src = inspect.getsource(fn)
        lines.append(src.rstrip())
        lines.append("")
        names[ver] = fn.__name__
    table = ", ".join(f"{ver}: {name}" for ver, name in sorted(names.items()))
    lines.append(f"MIGRATIONS = {{{table}}}")
    lines.append("")
    lines.append("")
    lines.append("def migrate_project_dict(data: dict) -> dict:")
    lines.append(f'{_IND}"""按 schema_version 逐级迁移到 CURRENT_SCHEMA_VERSION。"""')
    lines.append(f'{_IND}v = int(data.get("schema_version", SCHEMA_VERSION))')
    lines.append(f"{_IND}while v < CURRENT_SCHEMA_VERSION:")
    lines.append(f"{_IND}{_IND}fn = MIGRATIONS.get(v)")
    lines.append(f"{_IND}{_IND}if fn is None:")
    lines.append(f'{_IND}{_IND}{_IND}raise RuntimeError(f"No migration from project schema v{{v}}")')
    lines.append(f"{_IND}{_IND}data = fn(data)")
    lines.append(f'{_IND}{_IND}v = int(data.get("schema_version", v + 1))')
    lines.append(f"{_IND}return data")
    lines.append("")
    return "\n".join(lines)


def gen_py_schema(mod: "CodeModule"):
    """生成自包含 Python schema 模块（由 CodeModule.py_schema_file 触发）。"""
    registry = ReflectionRegistry()
    target = Path(mod.py_schema_file).resolve()
    print("Generating python schema to ", target)

    if not mod.classes:
        raise RuntimeError(f"codegen_py_schema: module '{mod.name}' has no classes.")

    # ground-truth 模块：取第一个类的定义模块，读取版本常量 / migrations / deprecated
    gt_module = importlib.import_module(mod.classes[0].__module__)
    schema_version = int(getattr(gt_module, "SCHEMA_VERSION", 1))
    current_schema_version = int(getattr(gt_module, "CURRENT_SCHEMA_VERSION", schema_version))
    migrations = dict(getattr(gt_module, "MIGRATIONS", {}) or {})
    deprecated = dict(getattr(gt_module, "DEPRECATED_FIELDS", {}) or {})

    # root schema 类：默认 classes 列表最后一个（聚合其它子 schema 的顶层类）
    root_name = getattr(mod, "py_schema_root", None) or mod.classes[-1].__name__

    parts: List[str] = [_HEADER]
    parts.append(f"SCHEMA_VERSION = {schema_version}\n")
    parts.append(f"CURRENT_SCHEMA_VERSION = {current_schema_version}\n")
    parts.append(f"DEPRECATED_FIELDS = {deprecated!r}\n")
    parts.append("\n")

    for cls in mod.classes:
        info = registry.get_class_info(cls.__name__)
        if info is None:
            raise RuntimeError(f"codegen_py_schema: class '{cls.__name__}' is not reflected.")
        if info.is_enum:
            print(f"codegen_py_schema: skip enum '{cls.__name__}' (not supported in py schema)")
            continue
        parts.append(_gen_dataclass(info, registry, deprecated, info.name == root_name))
        parts.append("\n\n")

    parts.append(_gen_migration_section(migrations))
    parts.append(_FOOTER)

    _write_string_to("".join(parts), target)

    # 输出目录的 __init__.py 一井生成（generated/ 目录整体被 gitignore，
    # 新鲜检出后需保证包可导入；已存在且内容一致时 _write_string_to 自动跳过）
    init_file = target.parent / "__init__.py"
    _write_string_to(
        "# This File is Generated From Python Def\n"
        "# robocute generated package (ground-truth: src/rbc_meta/)\n",
        init_file,
    )
