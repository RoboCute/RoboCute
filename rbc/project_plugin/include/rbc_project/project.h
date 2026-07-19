#pragma once
#include <rbc_config.h>
#include <rbc_core/rc.h>
#include <rbc_project/generated/project.h>// ProjectConfigSchema（由 uv run gen 生成）
#include <luisa/core/stl/filesystem.h>
namespace rbc {
namespace world {
struct Resource;
}// namespace world

/**
 * IProject：项目加载与资源索引接口。
 *
 * 项目统一入口为项目根目录下的 `rbc_project.json`，其 schema 的唯一事实来源是
 * `src/rbc_meta/types/project_plugin.py`（ground-truth），C++ struct 由
 * `uv run gen` 生成（见 `rbc_project/generated/project.h`）。
 *
 * 路径约定：
 * - 所有 paths.* 配置项均为「项目根目录」相对路径；
 * - import_assets / read_file_metas / unsafe_write_file_meta 的相对路径语义
 *   保持相对 assets 目录（与旧行为一致）；
 * - 路径访问器由 project_root() + config().paths.* 组合而成，
 *   不再出现散落的 "assets" / "library" 字面量。
 */
struct IProject : RCBase {
    struct FileMeta {
        vstd::Guid guid;
        vstd::string meta_info;
        vstd::Guid type_id;
    };

    IProject() = default;

    // ===== 项目配置（schema 单一事实来源）=====
    [[nodiscard]] virtual ProjectConfigSchema const &config() const = 0;
    [[nodiscard]] virtual luisa::filesystem::path const &project_root() const = 0;
    // 供 pybind / 脚本边界使用：以 JSON 字符串跨 DLL 传递（避免跨模块符号依赖）
    [[nodiscard]] virtual luisa::string config_json() const = 0;

    // ===== 派生路径访问（非虚 inline，默认值只存在于生成的 schema struct 中）=====
    [[nodiscard]] luisa::filesystem::path assets_dir() const { return project_root() / config().paths.assets; }
    [[nodiscard]] luisa::filesystem::path library_dir() const { return project_root() / config().paths.library; }
    [[nodiscard]] luisa::filesystem::path docs_dir() const { return project_root() / config().paths.docs; }
    [[nodiscard]] luisa::filesystem::path datasets_dir() const { return project_root() / config().paths.datasets; }
    [[nodiscard]] luisa::filesystem::path pretrained_dir() const { return project_root() / config().paths.pretrained; }
    [[nodiscard]] luisa::filesystem::path intermediate_dir() const { return project_root() / config().paths.intermediate; }

    // ===== 兼容接口（deprecated 但保留）=====
    // DEPRECATED: 等价于 assets_dir()；仅为兼容旧代码保留，新代码请使用 assets_dir()。
    [[nodiscard]] virtual luisa::filesystem::path const &root_path() const = 0;

    // ===== 既有资源接口（相对路径语义不变：相对 assets 目录）=====
    virtual RC<world::Resource> import_assets(
        luisa::filesystem::path origin_path,
        vstd::MD5 type_id,
        luisa::string const &meta_json = {}) = 0;
    virtual void scan_project() = 0;
    virtual void read_file_metas(
        luisa::filesystem::path dest_path,
        luisa::vector<FileMeta> &result) const = 0;
    virtual void unsafe_write_file_meta(
        luisa::filesystem::path origin_path,
        luisa::span<FileMeta const> metas) = 0;
    virtual ~IProject() = default;
};
}// namespace rbc
