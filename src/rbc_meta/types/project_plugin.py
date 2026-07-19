"""Project schema ground-truth.

本文件是 `rbc_project.json` 的唯一 schema 定义来源（single ground-truth）：

- 通过 `uv run gen` 生成 C++ struct：
  - `rbc/project_plugin/include/rbc_project/generated/project.h`（struct 定义 + serde 声明）
  - `rbc/project_plugin/src/generated/project.cpp`（serde 实现）
- 通过 `uv run gen` 生成 Python dataclass：
  - `src/robocute/generated/project_schema.py`

演进规则（详见 docs/design/project_schema.md）：

- 新增字段：直接添加带默认值的字段即可。旧 JSON 缺失该 key 时，
  C++/Python 双侧均回落到此处定义的默认值（serde 缺 key 容忍），无需改版本号。
- 破坏性变更（重命名 / 删除 / 语义变化）：`CURRENT_SCHEMA_VERSION` +1，
  并在 `MIGRATIONS` 中注册 `from_version -> callable(dict) -> dict`，
  C++ 侧同步在 `project_schema_migration.cpp` 注册等价 fixup。
- 弃用字段：保留字段与默认值至少一个大版本，在 `DEPRECATED_FIELDS` 登记并注明替代字段。

注意：受 `JsonSerializer`/`JsonDeSerializer` 能力约束（不支持 optional），
本 schema 所有字段一律使用「普通字段 + 默认值」，禁止 `Optional[...]` 注解。
"""

from rbc_meta.utils.reflect import reflect
from rbc_meta.utils.builtin import uint, Vector

# ===== 版本常量（生成器会读取并注入 C++/Python 两侧产物）=====
SCHEMA_VERSION = 1  # 缺省 schema_version 的 JSON 视为 v1（legacy，即当前模板）
CURRENT_SCHEMA_VERSION = 2  # 当前写入 / 迁移目标版本


# ===== Schema 类 =====
@reflect(cpp_namespace="rbc", serde=True)
class ProjectPathsConfig:
    """项目目录布局。所有路径均为「项目根目录」相对路径。"""

    assets: str = "assets"
    library: str = "library"  # v2 新增：world meta/binary 缓存目录（替代 app.py 中的 "library" 硬编码）
    docs: str = "docs"
    datasets: str = "datasets"
    pretrained: str = "pretrained"
    intermediate: str = ".rbc"  # meta_db 等中间产物目录（替代 ".temp_db" 硬编码）


@reflect(cpp_namespace="rbc", serde=True)
class ProjectDefaultConfig:
    """项目默认运行配置。"""

    default_scene: str = "assets/test_scene.scene"  # 项目根相对路径
    startup_graph: str = ""
    graphics_backend: str = "dx"
    resource_version: str = "1.0"


@reflect(cpp_namespace="rbc", serde=True)
class ProjectMetaData:
    """项目元信息（展示 / 索引用途）。"""

    tags: Vector[str]
    license: str = ""
    repository: str = ""


@reflect(cpp_namespace="rbc", serde=True)
class ProjectConfigSchema:
    """`rbc_project.json` 的顶层 schema。

    兼容性约定：
    - 缺 key 时字段落默认值（旧 JSON 前向兼容）。
    - 多余 key 被忽略（新 JSON 后向兼容）。
    - `schema_version` 缺省视为 v1（SCHEMA_VERSION），加载后由迁移层升级到
      CURRENT_SCHEMA_VERSION。
    """

    schema_version: uint = SCHEMA_VERSION
    name: str = "unnamed"
    version: str = "0.1.0"
    rbc_version: str = ""
    author: str = ""
    description: str = ""
    paths: ProjectPathsConfig
    config: ProjectDefaultConfig
    metadata: ProjectMetaData


OUT_CLASSES = [
    ProjectPathsConfig,
    ProjectDefaultConfig,
    ProjectMetaData,
    ProjectConfigSchema,
]

__all__ = [
    "OUT_CLASSES",
    "SCHEMA_VERSION",
    "CURRENT_SCHEMA_VERSION",
    "MIGRATIONS",
    "DEPRECATED_FIELDS",
]

# ===== 弃用登记（文档用途；生成器在产物中输出 deprecation 注释）=====
# 格式: "ClassName.field": "自 vX 起弃用，将在 vY 移除；替代: some_other_field"
DEPRECATED_FIELDS = {}


# ===== Migration =====
# 约束：migration 函数必须自包含（仅可使用 dict/list/str/int 等内建操作，
# 禁止引用外部符号），因为函数体会经 inspect.getsource 内嵌进生成的 Python schema。
def _migrate_v1_to_v2(data: dict) -> dict:
    """v1 -> v2：补充 paths.library（默认 'library'）；其余新增字段由默认值兜底。"""
    paths = data.setdefault("paths", {})
    paths.setdefault("library", "library")
    data["schema_version"] = 2
    return data


MIGRATIONS = {
    1: _migrate_v1_to_v2,
}
