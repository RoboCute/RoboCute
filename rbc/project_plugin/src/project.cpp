#include <rbc_project/project.h>
#include <luisa/core/fiber.h>
#include <rbc_core/utils/parse_string.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_world/resource_base.h>
#include <rbc_world/resource_importer.h>
#include <luisa/core/binary_file_stream.h>
#include <rbc_project/project_plugin.h>
namespace rbc {
struct Project : IProject {
private:
    luisa::filesystem::path _assets_path;
    void _reimport(
        vstd::Guid binary_guid,
        luisa::string &meta_data,
        vstd::MD5 type_id,
        luisa::filesystem::path const &origin_path);

public:
    Project(luisa::filesystem::path const &assets_db_path)
        : _assets_path(assets_db_path) {}
    void scan_project();
    ~Project() {
    }
    Project(Project const &) = delete;
    Project(Project &&) = delete;
};
namespace detail {
static luisa::string get_path_key(luisa::filesystem::path const &path, luisa::filesystem::path const &base) {
    auto path_key = luisa::to_string(luisa::filesystem::relative(path, base).lexically_normal());
    for (auto &i : path_key) {
        if (i == '\\') {
            i = '/';
        }
    }
    return path_key;
}
}// namespace detail
void Project::_reimport(
    vstd::Guid binary_guid,
    luisa::string &meta_data,
    vstd::MD5 type_id,
    luisa::filesystem::path const &origin_path) {
    auto &importers = world::ResourceImporterRegistry::instance();
    auto importer = importers.find_importer(origin_path, type_id);
    if (!importer) {
        return;
    }
    LUISA_VERBOSE("Importing {}", luisa::to_string(origin_path));
    importer->import(
        binary_guid,
        origin_path,
        meta_data);
}
void Project::scan_project() {
    luisa::spin_mutex values_mtx;
    luisa::vector<luisa::filesystem::path> paths;
    for (auto &i : std::filesystem::recursive_directory_iterator(_assets_path)) {
        if (!i.is_regular_file() || i.path().extension() == ".rbcmt") {
            continue;
        }
        paths.emplace_back(i.path());
    }
    struct FileMeta {
        vstd::Guid guid;
        vstd::string meta_info;
        vstd::Guid type_id;
    };

    luisa::fiber::parallel(
        paths.size(),
        [&](size_t i) {
            auto &path = paths[i];
            bool file_is_dirty{true};
            bool file_meta_is_dirty{false};
            luisa::vector<FileMeta> metas;
            vstd::Guid md5;
            md5.reset();
            uint64_t file_last_write_time{};
            luisa::vector<std::byte> vec;
            [&] {
                {
                    luisa::BinaryFileStream fs{luisa::to_string(path) + ".rbcmt"};
                    if (!fs.valid()) return;
                    vec.push_back_uninitialized(fs.length());
                    fs.read(vec);
                }
                JsonDeSerializer deser{luisa::string_view{(char const *)vec.data(), vec.size()}};
                if (!deser.valid()) [[unlikely]] {
                    return;
                }
                uint64_t last_write_time;
                if (!deser.read(last_write_time, "last_time")) [[unlikely]]
                    return;
                if (!deser._load(md5, "md5")) [[unlikely]] {
                    md5.reset();
                    return;
                }
                uint64_t meta_count;
                if (deser.start_array(meta_count, "metas")) {
                    auto d = vstd::scope_exit([&] {
                        deser.end_scope();
                    });
                    // TODO: deal with error
                    if (meta_count % 3 != 0) [[unlikely]] {
                        LUISA_ERROR("Project json is broken.");
                    }
                    metas.reserve(meta_count / 3);
                    for (auto i : vstd::range(meta_count / 3)) {
                        auto &v = metas.emplace_back();
                        if (!(deser._load(v.guid) && deser._load(v.meta_info) && deser._load(v.type_id))) [[unlikely]] {
                            LUISA_ERROR("Project json is broken.");
                        }
                    }
                }
                file_last_write_time = luisa::filesystem::last_write_time(path).time_since_epoch().count();
                if (file_last_write_time != last_write_time) [[unlikely]] {
                    // time not equal, check hash
                    luisa::BinaryFileStream fs{luisa::to_string(path)};
                    if (!fs.valid()) return;
                    vec.clear();
                    vec.push_back_uninitialized(fs.length());
                    vstd::MD5 file_md5(
                        {reinterpret_cast<uint8_t const *>(vec.data()),
                         vec.size()});

                    if (file_md5 != md5) {
                        md5 = file_md5;
                        return;
                    }
                    file_meta_is_dirty = true;
                }
                file_is_dirty = false;
            }();
            if (!(file_meta_is_dirty || file_is_dirty)) return;// file is clean
            file_meta_is_dirty |= file_is_dirty;
            auto update = [&] {
                if (!md5) {
                    luisa::BinaryFileStream fs{luisa::to_string(path)};
                    if (!fs.valid()) return;
                    vec.clear();
                    vec.push_back_uninitialized(fs.length());
                    md5 = vstd::MD5(
                        {reinterpret_cast<uint8_t const *>(vec.data()),
                         vec.size()});
                }
                if (file_last_write_time == 0) {
                    file_last_write_time = luisa::filesystem::last_write_time(path).time_since_epoch().count();
                }
            };
            if (file_is_dirty) {
                auto iter = metas.begin();
                luisa::spin_mutex iter_mtx;
                luisa::fiber::parallel(metas.size(), [&](uint32_t i) {
                    iter_mtx.lock();
                    auto &v = *iter;
                    iter++;
                    iter_mtx.unlock();
                    _reimport(
                        v.guid,
                        v.meta_info,
                        reinterpret_cast<vstd::MD5 const &>(v.type_id), path);
                });
            }
            if (file_meta_is_dirty) {
                LUISA_VERBOSE("Generating {} meta", luisa::to_string(path));
                update();
                JsonSerializer json_ser;
                json_ser.start_array();
                for (auto &i : metas) {
                    json_ser._store(i.guid);
                    json_ser._store(i.meta_info);
                    json_ser._store(i.type_id);
                }
                json_ser.add_last_scope_to_object("metas");
                json_ser._store(file_last_write_time, "last_time");
                json_ser._store(md5, "md5");
                auto blob = json_ser.write_to();
                BinaryFileWriter file_writer{
                    luisa::to_string(path) + ".rbcmt"};
                file_writer.write(blob);
            }
        });
}
struct ProjectPluginImpl : ProjectPlugin {
public:
    ProjectPluginImpl() {}
    IProject *create_project(luisa::filesystem::path const &assets_db_path) override {
        return new Project(assets_db_path);
    }
};

LUISA_EXPORT_API ProjectPlugin *get_project_plugin() {
    static ProjectPluginImpl project_plugin_impl{};
    return &project_plugin_impl;
}

}// namespace rbc