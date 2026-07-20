#include <rbc_project/project_schema_migration.h>
#include <rbc_core/serde.h>
#include <luisa/core/binary_file_stream.h>
#include <luisa/core/logging.h>

namespace rbc {
namespace detail {

// ===== 结构级迁移函数表 =====
// 与 src/rbc_meta/types/project_plugin.py 中的 MIGRATIONS 一一对应（from_version -> fixup），
// 做任何破坏性 schema 变更时必须两侧同步注册。

// v1 -> v2：补充 paths.library（默认 "library"）；其余新增字段由默认值兜底。
static void _migrate_v1_to_v2(ProjectConfigSchema &s) {
    if (s.paths.library.empty()) s.paths.library = "library";
    s.schema_version = 2;
}

using ProjectSchemaMigrationFn = void (*)(ProjectConfigSchema &);
// 下标即 from_version；nullptr 为占位/缺口
static ProjectSchemaMigrationFn const PROJECT_SCHEMA_MIGRATIONS[] = {
    nullptr,          // v0 占位
    _migrate_v1_to_v2,// v1 -> v2
};
constexpr size_t PROJECT_SCHEMA_MIGRATION_COUNT =
    sizeof(PROJECT_SCHEMA_MIGRATIONS) / sizeof(PROJECT_SCHEMA_MIGRATIONS[0]);

}// namespace detail

void fixup_project_schema(ProjectConfigSchema &schema) {
    // 缺省 schema_version 视为 v1（legacy 模板）
    if (schema.schema_version == 0) schema.schema_version = RBC_PROJECT_SCHEMA_VERSION;
    while (schema.schema_version < RBC_PROJECT_CURRENT_SCHEMA_VERSION) {
        auto v = schema.schema_version;
        if (v >= detail::PROJECT_SCHEMA_MIGRATION_COUNT ||
            detail::PROJECT_SCHEMA_MIGRATIONS[v] == nullptr) [[unlikely]] {
            LUISA_WARNING("No migration from project schema v{}, keep current fields as-is.", v);
            break;
        }
        detail::PROJECT_SCHEMA_MIGRATIONS[v](schema);
    }
    schema.schema_version = RBC_PROJECT_CURRENT_SCHEMA_VERSION;
}

bool load_project_config(luisa::filesystem::path const &json_path, ProjectConfigSchema &out) {
    luisa::vector<std::byte> data;
    {
        luisa::BinaryFileStream fs{luisa::to_string(json_path)};
        if (!fs.valid()) {
            out = ProjectConfigSchema{};
            out.schema_version = RBC_PROJECT_CURRENT_SCHEMA_VERSION;
            return false;
        }
        data.push_back_uninitialized(fs.length());
        fs.read(data);
    }
    JsonDeSerializer deser{luisa::string_view{reinterpret_cast<char const *>(data.data()), data.size()}};
    if (!deser.valid()) [[unlikely]] {
        LUISA_WARNING("Project config json is broken: {}", luisa::to_string(json_path));
        out = ProjectConfigSchema{};
        out.schema_version = RBC_PROJECT_CURRENT_SCHEMA_VERSION;
        return false;
    }
    // 根对象 scope 已由 JsonReader 构造时打开；rbc_objdeser 逐字段 _load 且忽略返回值，
    // 因此缺 key 落默认值、多余 key 被忽略（与 Python 侧 from_dict 行为对称）。
    out.rbc_objdeser(deser);
    fixup_project_schema(out);
    return true;
}

}// namespace rbc
