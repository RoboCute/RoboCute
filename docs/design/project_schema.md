# RoboCute 项目配置 Schema（`rbc_project.json`）

本文档是 `rbc_project.json` 的权威参考：字段定义、版本演进政策、migration 编写约束与
代码生成工作流。

## 1. 设计原则：单一 Ground-Truth

Schema 的**唯一事实来源**是 Python 文件：

```
src/rbc_meta/types/project_plugin.py
```

通过 `uv run gen` 从该文件同时生成 C++ 与 Python 两侧产物：

| 产物 | 路径 | 生成器 |
|------|------|--------|
| C++ struct 定义（含默认值、serde 声明、版本常量） | `rbc/project_plugin/include/rbc_project/generated/project.h` | `codegen_cpp.gen_cpp_interface_header` |
| C++ serde 实现 | `rbc/project_plugin/src/generated/project.cpp` | `codegen_cpp.gen_cpp_impl` |
| 自包含 Python dataclass schema | `src/robocute/generated/project_schema.py` | `codegen_py_schema.gen_py_schema`（本重构新增） |

**禁止手改 `generated/` 产物**；一切 schema 变更都改 ground-truth 后重新 `uv run gen`。

### 关键设计约束

1. **不使用 `Optional` 字段**。`JsonSerializer`/`JsonDeSerializer` 不支持
   `vstd::optional<T>`（`serde.h` 的 `_store`/`_load` 无 optional 分支）。
   兼容性完全依靠「**普通字段 + 双侧默认值**」：
   - 旧 JSON 缺新字段的 key → 反序列化 `_load` 返回 false 且被忽略 → 字段保持默认值；
   - 新 JSON 多出未知 key → 旧代码读取时直接忽略。
   Python 侧 `from_dict` 行为严格对称（缺 key 落默认值，未知 key 忽略）。
2. **pybind 边界只传原始类型**。C++ 侧 schema struct 不跨 DLL 传递；
   跨边界统一用 `IProject::config_json()` 返回的 JSON 字符串，
   Python 侧用生成的 `ProjectConfigSchema.from_dict(json.loads(...))` 解析。
3. **默认值只定义一次**（ground-truth 的类属性），C++/Python 产物共享同一份字面量。

## 2. Schema 参考（v2）

```jsonc
{
    "schema_version": 2,              // uint，缺省视为 1（legacy 模板）
    "name": "DefaultRobocuteProject",
    "version": "0.1.0",
    "rbc_version": "0.2.0",
    "author": "sailing-innocent",
    "description": "...",
    "paths": {                        // ProjectPathsConfig：全部为「项目根」相对路径
        "assets": "assets",           // 资源目录（模型/材质/纹理/场景）
        "library": "library",         // world meta/binary 缓存目录（v2 新增）
        "docs": "docs",
        "datasets": "datasets",
        "pretrained": "pretrained",
        "intermediate": ".rbc"        // 中间产物目录（meta_db 等；替代旧 .temp_db）
    },
    "config": {                       // ProjectDefaultConfig
        "default_scene": "assets/test_scene.scene",  // 项目根相对路径
        "startup_graph": "",
        "graphics_backend": "dx",
        "resource_version": "1.0"
    },
    "metadata": {                     // ProjectMetaData
        "tags": ["aigc", "robotics", "simulation"],
        "license": "Apache-2.0",
        "repository": "https://github.com/RoboCute/rbc-project-default"
    }
}
```

对应 ground-truth 类：`ProjectConfigSchema`（顶层）、`ProjectPathsConfig`、
`ProjectDefaultConfig`、`ProjectMetaData`。

### 路径解析规则

- `paths.*` 均为**项目根目录**相对路径；C++ 侧经 `IProject::assets_dir()` 等访问器
  拼出绝对路径，Python 侧经 `robocute.project.load_project_config()` 读取后自行拼接。
- `config.default_scene` 是**项目根相对路径**；而 `Project.import_scene()` 等
  `import_*` 接口期望 **assets 相对路径**。Python 侧用
  `robocute.project.asset_rel_path(cfg, cfg.config.default_scene)` 完成转换
  （`"assets/test_scene.scene"` → `"test_scene.scene"`；不在 assets 前缀下的路径原样返回）。

## 3. 版本演进政策

版本常量定义在 ground-truth 中，生成器注入两侧产物：

- `SCHEMA_VERSION = 1`：缺省 `schema_version` 的 JSON 视为 v1（即当前模板）。
- `CURRENT_SCHEMA_VERSION = 2`：当前写入/迁移目标版本。
- C++ 侧常量：`RBC_PROJECT_SCHEMA_VERSION` / `RBC_PROJECT_CURRENT_SCHEMA_VERSION`
  （在 `namespace rbc` 中，见生成的 `project.h`）。

### 3.1 兼容新增（不改版本号）

新增**带默认值的可选字段**（如新路径项、新配置项）：

1. 在 ground-truth 对应类中加一行 `field: str = "default"`；
2. `uv run gen`；
3. 完成。旧 JSON 自动兼容（缺 key 落默认值），新 JSON 被旧代码读取时多余 key 被忽略。

### 3.2 破坏性变更（版本号 +1，注册双侧 migration）

重命名 / 删除字段 / 语义变化：

1. ground-truth 中 `CURRENT_SCHEMA_VERSION += 1`；
2. 在 ground-truth 的 `MIGRATIONS` 注册 `from_version -> callable(dict) -> dict`
   （Python dict 级迁移）；
3. 在 `rbc/project_plugin/src/project_schema_migration.cpp` 的
   `PROJECT_SCHEMA_MIGRATIONS` 表中注册**等价的结构级 fixup**；
4. 更新本文档的变更记录。

> **为什么 C++ 侧是结构级 fixup 而非 JSON 文本改写**：现有 `JsonReader` 是流式只读
> 接口，改写任意 JSON 文本成本高；缺 key 容忍保证旧 JSON 加载后字段已定（落默认值），
> 结构级 fixup 与 Python dict 级迁移语义等价。若未来出现必须 JSON 级的迁移
> （如 key 重命名且需保留旧 key 数据），策略为：先按旧 schema（保留的 deprecated
> 字段）读入 → fixup 中拷贝到新字段 → 保存时只写新字段。

**Migration 编写约束（fail-fast，生成器强制）**：

- Python migration 函数必须**自包含**：仅可使用 `dict/list/set/tuple/str/int/float/
  bool/len/isinstance` 等内建名字与 dict/list 方法；引用任何模块级外部符号
  （如 `SCHEMA_VERSION`、其它函数）会在 `uv run gen` 时报错。
  原因：函数体经 `inspect.getsource` 内嵌进生成的 `project_schema.py`。
- 迁移是逐级链式的：`migrate_project_dict` / `fixup_project_schema` 按
  `schema_version` 循环应用 `MIGRATIONS[v]`，直到 `CURRENT`；
  缺失某级迁移时 Python 侧抛 `RuntimeError`，C++ 侧告警并保持现状。
- 加载完成后 `schema_version` 始终被置为 `CURRENT_SCHEMA_VERSION`。

### 3.3 弃用（deprecate）流程

1. 字段保留至少一个大版本，默认值不动；
2. 在 ground-truth 的 `DEPRECATED_FIELDS` 登记：
   `{"ClassName.field": "自 vX 起弃用，将在 vY 移除；替代: new_field"}`，
   生成器会在 Python 产物的对应字段处输出 `# deprecated:` 注释；
3. 读取逻辑先行切换到替代字段，并回退兼容旧字段；
4. 到期后按 3.2 破坏性变更移除（版本号 +1 + migration）。

## 4. 代码生成工作流

```bash
# 1. 编辑 ground-truth
$EDITOR src/rbc_meta/types/project_plugin.py

# 2. 重新生成（只生成代码文本，不编译）
uv run gen

# 3. 检查产物 diff
git diff rbc/project_plugin/include/rbc_project/generated/project.h \
         rbc/project_plugin/src/generated/project.cpp \
         src/robocute/generated/project_schema.py

# 4. 构建（由开发者另行执行）
xmake
```

生成器注册位于 `src/rbc_build/generate.py` 的 `ProjectPluginModule`
（`cpp_interface_header` + `cpp_impl_file` + `py_schema_file` + `extra_constants`），
遵循 codegenx 的「路径即意图」约定。

## 5. 消费方指引

### C++（同模块 / editor）

```cpp
#include <rbc_project/project.h>
// IProject::config() 返回 const ProjectConfigSchema&，直接读字段：
auto scene_rel = proj->config().config.default_scene;
auto assets = proj->assets_dir();        // = project_root() / config().paths.assets
auto library = proj->library_dir();
auto db_dir = proj->intermediate_dir();  // meta_db 位于其下的 meta_db/
```

加载逻辑集中在 `rbc/project_plugin/src/project.cpp` 的构造流程
（`rbc_project.json` 不存在时自动降级为 legacy assets 目录模式并告警）与
`project_schema_migration.cpp`（`load_project_config` / `fixup_project_schema`）。

### Python（脚本 / app）

```python
import robocute.project as rbc_project

cfg = rbc_project.load_project_config(project_root)   # 自动迁移到 CURRENT
scene_rel = rbc_project.asset_rel_path(cfg, cfg.config.default_scene)
datasets = project_root / cfg.paths.datasets

# 运行中的 Project 句柄也可直接取配置（JSON 字符串跨 DLL 边界）：
import json
from robocute.generated.project_schema import ProjectConfigSchema
cfg2 = ProjectConfigSchema.from_dict(json.loads(app._project.config_json()))
root, assets, library = (
    app._project.root_path(),
    app._project.assets_path(),
    app._project.library_path(),
)
```

## 6. 已知后续项

- **editor 编译面**：`rbc/editor/**`（含 `editor/plugins/project_plugin/*`、
  `EditorProject.h`）默认不构建；若其引用 `create_project` 的旧语义，
  启用 editor 构建时需按本文档 §5 的新接口跟进。
- **旧 `.temp_db` 缓存**：v2 起 meta_db 迁至 `<root>/.rbc/meta_db`，旧 `.temp_db`
  不再读取（纯索引缓存，可安全手动删除）。
- **legacy 自动检测**：目录下无 `rbc_project.json` 时按 assets 目录处理
  （`paths.assets = 目录名`、项目根 = 父目录），与旧行为（assets 直接传入、
  `.temp_db` 在父目录）语义一致；该路径为 deprecated，请迁移到 `rbc_project.json` 入口。

## 7. 变更记录

| 版本 | 变更 | 兼容处理 |
|------|------|----------|
| v1 | 初始模板（无 `schema_version`、无 `paths.library`） | — |
| v2 | 新增 `schema_version`、`paths.library`；`.temp_db` → `<intermediate>/meta_db`；`create_project` 参数语义由 assets 目录改为项目根目录（legacy 自动降级） | 新增字段走默认值兜底 + `_migrate_v1_to_v2`（双侧） |
