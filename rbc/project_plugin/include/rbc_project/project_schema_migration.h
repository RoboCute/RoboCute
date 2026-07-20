#pragma once
#include <rbc_project/generated/project.h>// ProjectConfigSchema（由 uv run gen 生成）
#include <luisa/core/stl/filesystem.h>
#include <luisa/core/stl/string.h>

namespace rbc {

/**
 * 项目 schema 的加载与版本迁移。
 *
 * 版本演进策略（详见 docs/design/project_schema.md）：
 * - 兼容新增字段由「默认值 + 缺 key 容忍」自动覆盖，无需迁移；
 * - 破坏性变更（重命名 / 删除 / 语义变化）需要：
 *     1. ground-truth（src/rbc_meta/types/project_plugin.py）中
 *        CURRENT_SCHEMA_VERSION +1 并注册 Python dict 级 migration；
 *     2. 本文件对应的 .cpp 中注册等价的「结构级 fixup」；
 *   两侧必须同步演进。
 *
 * 为什么 C++ 侧采用结构级 fixup 而非 JSON 文本改写：
 * 现有 JsonReader 是流式只读接口，改写任意 JSON 文本成本高；而缺 key 容忍
 * 机制保证了旧 JSON 加载后字段已定（落默认值），结构级 fixup 与 Python 侧
 * dict 级迁移语义等价。若未来出现必须 JSON 级的迁移（如 key 重命名且需保留
 * 旧 key 数据），策略为：先按旧 schema（保留的 deprecated 字段）读入 →
 * fixup 中拷贝到新字段 → 保存时只写新字段。
 */

// 从 rbc_project.json 加载项目配置：
//   读取文件 -> 反序列化（缺 key 容忍，多余 key 忽略）-> 结构级迁移 ->
//   schema_version = RBC_PROJECT_CURRENT_SCHEMA_VERSION。
// 文件不存在 / 解析失败时返回 false，out 被重置为默认构造（全默认值，
// schema_version = CURRENT），调用方可安全继续使用。
bool load_project_config(luisa::filesystem::path const &json_path, ProjectConfigSchema &out);

// 结构级迁移：按 schema.schema_version 逐级应用 fixup，直到
// RBC_PROJECT_CURRENT_SCHEMA_VERSION。与 Python 侧 MIGRATIONS 一一对应。
void fixup_project_schema(ProjectConfigSchema &schema);

}// namespace rbc
