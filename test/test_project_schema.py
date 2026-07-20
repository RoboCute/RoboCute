"""项目 schema（rbc_project.json）代码生成与版本迁移测试。

纯 stdlib 路径：通过 codegen_py_schema 生成 schema 模块到临时目录并以文件路径加载，
不触碰原生扩展（rbc_ext_c）与 robocute 包 __init__。
"""

import importlib.util
import json
from pathlib import Path

import pytest

from rbc_meta.types import project_plugin as gt
from rbc_meta.utils.codegenx import CodeModule
from rbc_meta.utils.codegen_py_schema import gen_py_schema

# 与 rbc-project-default 模板一致的 v1 JSON（无 schema_version、无 paths.library）
V1_JSON = {
    "name": "DefaultRobocuteProject",
    "version": "0.1.0",
    "rbc_version": "0.2.0",
    "author": "sailing-innocent",
    "description": "template",
    "paths": {
        "assets": "assets",
        "docs": "docs",
        "datasets": "datasets",
        "pretrained": "pretrained",
        "intermediate": ".rbc",
    },
    "config": {
        "default_scene": "assets/test_scene.scene",
        "startup_graph": "assets/startup.graph",
        "graphics_backend": "dx",
        "resource_version": "1.0",
    },
    "metadata": {
        "tags": ["aigc", "robotics", "simulation"],
        "license": "Apache-2.0",
        "repository": "https://github.com/RoboCute/rbc-project-default",
    },
}


def _gen_schema_module(tmp_path: Path, classes=None):
    """用 ground-truth 生成 schema 模块到临时目录并加载返回。"""
    out_classes = classes if classes is not None else gt.OUT_CLASSES

    class _Mod(CodeModule):
        name = "project_plugin_test"
        py_schema_file = str(tmp_path / "project_schema.py")
        py_schema_root = "ProjectConfigSchema"
        classes = out_classes

    gen_py_schema(_Mod())
    spec = importlib.util.spec_from_file_location(
        "project_schema_under_test", _Mod.py_schema_file
    )
    m = importlib.util.module_from_spec(spec)
    # dataclass 处理字符串注解时需要从 sys.modules 解析所属模块
    import sys

    sys.modules[spec.name] = m
    spec.loader.exec_module(m)
    return m


def test_constants_match_ground_truth(tmp_path):
    m = _gen_schema_module(tmp_path)
    assert m.SCHEMA_VERSION == gt.SCHEMA_VERSION == 1
    assert m.CURRENT_SCHEMA_VERSION == gt.CURRENT_SCHEMA_VERSION == 2


def test_v1_template_load_and_migrate(tmp_path):
    m = _gen_schema_module(tmp_path)
    json_path = tmp_path / "rbc_project.json"
    json_path.write_text(json.dumps(V1_JSON), encoding="utf-8")

    cfg = m.ProjectConfigSchema.load(json_path)
    # v1 -> v2 迁移生效
    assert cfg.schema_version == gt.CURRENT_SCHEMA_VERSION
    # 新增字段 paths.library 落默认值
    assert cfg.paths.library == "library"
    # 已有字段保持
    assert cfg.paths.assets == "assets"
    assert cfg.paths.intermediate == ".rbc"
    assert cfg.config.default_scene == "assets/test_scene.scene"
    assert cfg.metadata.tags == ["aigc", "robotics", "simulation"]
    assert cfg.name == "DefaultRobocuteProject"


def test_load_accepts_project_root_dir(tmp_path):
    m = _gen_schema_module(tmp_path)
    (tmp_path / "rbc_project.json").write_text(json.dumps(V1_JSON), encoding="utf-8")
    cfg = m.ProjectConfigSchema.load(tmp_path)
    assert cfg.schema_version == gt.CURRENT_SCHEMA_VERSION


def test_forward_and_backward_compat(tmp_path):
    m = _gen_schema_module(tmp_path)
    # 前向兼容：未知 key 被忽略
    cfg = m.ProjectConfigSchema.from_dict(
        {"name": "x", "future_field": 1, "paths": {"assets": "a", "new_path": "z"}}
    )
    assert cfg.name == "x"
    assert cfg.paths.assets == "a"
    assert cfg.paths.library == "library"
    # 后向兼容：全空 dict -> 全默认值
    cfg_default = m.ProjectConfigSchema.from_dict({})
    assert cfg_default.schema_version == gt.SCHEMA_VERSION
    assert cfg_default.paths.assets == "assets"
    assert cfg_default.config.graphics_backend == "dx"


def test_save_load_round_trip(tmp_path):
    m = _gen_schema_module(tmp_path)
    cfg = m.ProjectConfigSchema()
    cfg.name = "round_trip"
    json_path = tmp_path / "rbc_project.json"
    cfg.save(json_path)
    raw = json.loads(json_path.read_text(encoding="utf-8"))
    assert raw["schema_version"] == gt.CURRENT_SCHEMA_VERSION
    cfg2 = m.ProjectConfigSchema.load(json_path)
    assert cfg2 == cfg


def test_migration_self_containment_fail_fast(tmp_path):
    """引用外部符号的 migration 必须在生成期报错（fail-fast）。"""
    import types as _types

    fake_gt = _types.ModuleType("fake_project_plugin_gt")

    def bad_migration(data: dict) -> dict:
        data["schema_version"] = gt.CURRENT_SCHEMA_VERSION  # 引用外部符号 -> 违规
        return data

    fake_gt.SCHEMA_VERSION = 1
    fake_gt.CURRENT_SCHEMA_VERSION = 2
    fake_gt.MIGRATIONS = {1: bad_migration}
    fake_gt.DEPRECATED_FIELDS = {}

    import rbc_meta.utils.codegen_py_schema as gen_mod

    orig_import = gen_mod.importlib.import_module
    gen_mod.importlib.import_module = lambda name: fake_gt
    try:
        with pytest.raises(RuntimeError, match="self-contained"):
            _gen_schema_module(tmp_path)
    finally:
        gen_mod.importlib.import_module = orig_import
