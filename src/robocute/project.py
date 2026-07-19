"""项目级工具：加载 / 解析 `rbc_project.json`。

本模块是「Python 侧项目配置访问」的统一入口，也是用户脚本的可扩展点：

- schema 唯一事实来源（ground-truth）是 `src/rbc_meta/types/project_plugin.py`；
- 生成的 dataclass schema 位于 `robocute/generated/project_schema.py`（`uv run gen` 产物）；
- 用户脚本通过 `load_project_config()` 即可读取全量配置（assets / library /
  datasets / pretrained / docs 等路径与 default_scene 等运行配置），
  无需关心 JSON 细节与版本迁移。
"""

from pathlib import Path, PurePosixPath

from robocute.generated.project_schema import (
    CURRENT_SCHEMA_VERSION,
    SCHEMA_VERSION,
    ProjectConfigSchema,
)

__all__ = [
    "ProjectConfigSchema",
    "CURRENT_SCHEMA_VERSION",
    "SCHEMA_VERSION",
    "PROJECT_CONFIG_FILENAME",
    "load_project_config",
    "asset_rel_path",
]

PROJECT_CONFIG_FILENAME = "rbc_project.json"


def load_project_config(project_root: str | Path) -> ProjectConfigSchema:
    """加载 `<project_root>/rbc_project.json`；不存在时返回全默认 schema（legacy 模式）。

    加载时自动执行版本迁移（v1 -> CURRENT），返回对象的 `schema_version`
    始终为 `CURRENT_SCHEMA_VERSION`。
    """
    root = Path(project_root)
    json_path = root / PROJECT_CONFIG_FILENAME
    if not json_path.is_file():
        cfg = ProjectConfigSchema()
        cfg.schema_version = CURRENT_SCHEMA_VERSION
        return cfg
    return ProjectConfigSchema.load(json_path)


def asset_rel_path(cfg: ProjectConfigSchema, path: str) -> str:
    """将 schema 中的「项目根相对路径」解析为 `import_*` 接口期望的「assets 相对路径」。

    例：`paths.assets == "assets"`，`default_scene == "assets/test_scene.scene"`
    -> `"test_scene.scene"`。
    已经是 assets 相对路径、或不在 assets 前缀下的路径原样返回。
    """
    if not path:
        return path
    p = PurePosixPath(path.replace("\\", "/"))
    if p.is_absolute():
        return path
    assets = cfg.paths.assets.replace("\\", "/").strip("/")
    if not assets:
        return path
    assets_parts = PurePosixPath(assets).parts
    parts = p.parts
    if len(parts) > len(assets_parts) and parts[: len(assets_parts)] == assets_parts:
        return PurePosixPath(*parts[len(assets_parts) :]).as_posix()
    return path
