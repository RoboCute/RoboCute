#include <rbc_project/project.h>
#include <luisa/core/fiber.h>
#include <rbc_core/utils/parse_string.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_world/resource_base.h>
#include <rbc_world/resource_importer.h>
#include <luisa/core/binary_file_stream.h>
#include <rbc_project/project_plugin.h>
#include <rbc_graphics/texture/texture_loader.h>
#include <rbc_graphics/graphics_utils.h>
namespace rbc {
struct Project : IProject {
private:

    luisa::filesystem::path _assets_path;
    vstd::HashMap<luisa::filesystem::path, std::pair<luisa::spin_mutex, std::atomic_uint64_t>> _file_mtx;
    luisa::spin_mutex _glb_mtx;
    static void _reimport(
        vstd::Guid binary_guid,
        luisa::string &meta_data,
        vstd::MD5 type_id,
        luisa::filesystem::path const &origin_path);

public:
    Project(luisa::filesystem::path const &assets_db_path)
        : _assets_path(assets_db_path) {
        if (_assets_path.empty()) {
            LUISA_ERROR("Assets path must not be empty.");
        }
    }
    void scan_project() override;
    ~Project() {
    }
    Project(Project const &) = delete;
    Project(Project &&) = delete;
    static void read_file_metas(
        luisa::vector<FileMeta> &result,
        luisa::span<std::byte const> file_data,
        uint64_t &last_write_time,
        vstd::MD5 &md5);
    void read_file_metas(
        luisa::filesystem::path origin_path,
        luisa::vector<FileMeta> &result) const override {
        if (origin_path.is_relative()) {
            origin_path = _assets_path / origin_path;
        }
        luisa::vector<std::byte> data;
        luisa::BinaryFileStream fs(luisa::to_string(origin_path) + ".rbcmt");
        if (fs.valid()) {
            data.push_back_uninitialized(fs.length());
            fs.read(data);
        } else {
            return;
        }
        vstd::MD5 file_last_md5;
        uint64_t file_last_write_time;
        read_file_metas(result, data, file_last_write_time, file_last_md5);
    }
    RC<world::Resource> import_assets(
        luisa::filesystem::path origin_path,
        vstd::MD5 type_id,
        luisa::string const &meta_json) override;
    void write_file_meta(luisa::filesystem::path const &origin_path, uint64_t last_write_time, vstd::MD5 file_md5, luisa::span<FileMeta const> file_metas);
    vstd::MD5 compute_md5(luisa::filesystem::path const &path) {
        luisa::string path_str;
        if (path.is_relative()) {
            path_str = luisa::to_string(_assets_path / path);
        } else {
            path_str = luisa::to_string(path);
        }
        luisa::vector<std::byte> vec;
        luisa::BinaryFileStream fs{path_str};
        if (!fs.valid()) return {};
        vec.clear();
        vec.push_back_uninitialized(fs.length());
        fs.read(vec);
        return vstd::MD5(
            {reinterpret_cast<uint8_t const *>(vec.data()),
             vec.size()});
    }
    luisa::filesystem::path const &root_path() const override {
        return _assets_path;
    }
    void unsafe_write_file_meta(
        luisa::filesystem::path dest_path,
        luisa::span<FileMeta const> metas) override {
        dest_path = dest_path.is_relative() ? _assets_path / dest_path : dest_path;
        write_file_meta(
            dest_path,
            luisa::filesystem::last_write_time(dest_path).time_since_epoch().count(),
            compute_md5(dest_path),
            metas);
    }
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
void Project::write_file_meta(luisa::filesystem::path const &origin_path, uint64_t last_write_time, vstd::MD5 file_md5, luisa::span<FileMeta const> file_metas) {
    JsonSerializer json_ser;
    json_ser.start_array();
    for (auto &i : file_metas) {
        json_ser._store(i.guid);
        json_ser._store(i.meta_info);
        json_ser._store(i.type_id);
    }
    json_ser.add_last_scope_to_object("metas");
    json_ser._store(last_write_time, "last_time");
    json_ser._store(reinterpret_cast<vstd::Guid const &>(file_md5), "md5");
    auto blob = json_ser.write_to();
    luisa::string path_str;
    if (origin_path.is_relative()) {
        path_str = luisa::to_string(_assets_path / origin_path);
    } else {
        path_str = luisa::to_string(origin_path);
    }
    BinaryFileWriter file_writer{
        path_str + ".rbcmt"};
    file_writer.write(blob);
}
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
    LUISA_INFO("Importing {} with meta {}", luisa::to_string(origin_path), meta_data);
    auto res = importer->import(
        binary_guid,
        origin_path,
        meta_data);
    res->save_to_path();
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

                    auto file_md5 = compute_md5(path);
                    if (file_md5 != md5) {
                        md5 = file_md5;
                        return;
                    }
                    file_meta_is_dirty = true;
                }
                for (auto &i : metas) {
                    if (!world::resource_exists(i.guid)) return;
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
                write_file_meta(
                    path,
                    file_last_write_time,
                    md5,
                    metas);
            }
        });
    auto graphics = GraphicsUtils::instance();
    if (graphics && graphics->tex_loader())
        graphics->tex_loader()->finish_task();
}
RC<world::Resource> Project::import_assets(
    luisa::filesystem::path origin_path,
    vstd::MD5 type_id,
    luisa::string const &meta_json) {
    if (origin_path.is_absolute()) {
        origin_path = luisa::filesystem::relative(origin_path, _assets_path);
    }
    origin_path = origin_path.lexically_normal();
    if (!luisa::filesystem::is_regular_file(luisa::to_string(_assets_path / origin_path))) {
        return {};
    }
    luisa::spin_mutex *mtx{};
    std::atomic_uint64_t *rc{};
    auto origin_path_str = luisa::to_string(origin_path);
    origin_path = _assets_path / origin_path;
    decltype(_file_mtx)::Index iter;
    {
        std::lock_guard lck{_glb_mtx};
        iter = _file_mtx.emplace(origin_path_str);
        mtx = &iter.value().first;
        rc = &iter.value().second;
        ++(*rc);
    }
    auto dsp = vstd::scope_exit([&] {
        if (--(*rc) == 0) {
            std::lock_guard lck{_glb_mtx};
            _file_mtx.remove(iter);
        }
    });
    std::lock_guard lck{*mtx};
    luisa::vector<std::byte> data;
    bool file_is_new{false};
    {
        luisa::BinaryFileStream fs(luisa::to_string(origin_path) + ".rbcmt");
        if (fs.valid()) {
            data.push_back_uninitialized(fs.length());
            fs.read(data);
        } else {
            file_is_new = true;
        }
    }
    vstd::MD5 file_last_md5;
    uint64_t file_last_write_time;
    luisa::vector<FileMeta> metas;
    read_file_metas(metas, data, file_last_write_time, file_last_md5);
    for (auto &i : metas) {
        if (i.type_id == vstd::Guid{type_id}) {
            auto res = world::load_resource(i.guid, false);
            if (res) return res;
        }
    }
    auto &importers = world::ResourceImporterRegistry::instance();
    auto importer = importers.find_importer(origin_path, type_id);
    if (!importer) {
        return {};
    }
    LUISA_VERBOSE("Importing {}", origin_path_str);
    RC<world::BaseObject> new_res_base{world::create_object(type_id)};
    if (!new_res_base) return {};
    if (new_res_base->base_type() != world::BaseObjectType::Resource) {
        return {};
    }
    auto new_res = std::move(new_res_base).cast_static<world::Resource>();
    if (!meta_json.empty()) {
        JsonDeSerializer ser(meta_json);
        rbc::ArchiveReadJson reader{ser};
        new_res->deserialize_meta(rbc::world::ObjDeSerialize{.ar = reader});
    }
    if (!importer->import(
            new_res.get(),
            origin_path)) {
        return {};
    }
    world::register_resource_meta(new_res.get());
    new_res->save_to_path();
    // write meta
    vstd::string meta_info;
    {
        JsonSerializer ser{false};
        ArchiveWriteJson adapter(ser);
        new_res->serialize_meta(world::ObjSerialize{adapter});
        ser.write_to(meta_info);
    }
    FileMeta file_meta{
        .guid = new_res->guid(),
        .meta_info = std::move(meta_info),
        .type_id = type_id};
    if (file_is_new || file_last_write_time == 0) {
        file_last_md5 = compute_md5(origin_path);
        file_last_write_time = luisa::filesystem::last_write_time(_assets_path / origin_path).time_since_epoch().count();
    }
    write_file_meta(
        origin_path,
        file_last_write_time, file_last_md5,
        {&file_meta, 1});
    return new_res;
}
void Project::read_file_metas(
    luisa::vector<FileMeta> &result,
    luisa::span<std::byte const> file_data,
    uint64_t &last_write_time,
    vstd::MD5 &md5) {
    auto str = luisa::string_view{(char const *)file_data.data(), file_data.size()};
    JsonDeSerializer deser{str};
    last_write_time = 0;
    md5 = vstd::MD5::MD5Data{0, 0};
    if (!deser.valid()) [[unlikely]] {
        return;
    }
    deser._load(last_write_time, "last_time");
    deser._load(reinterpret_cast<vstd::Guid &>(md5), "md5");
    uint64_t meta_count;
    if (deser.start_array(meta_count, "metas")) {
        auto d = vstd::scope_exit([&] {
            deser.end_scope();
        });
        // TODO: deal with error
        if (meta_count % 3 != 0) [[unlikely]] {
            LUISA_ERROR("Project json is broken.");
        }
        result.reserve(result.size() + meta_count / 3);
        for (auto i : vstd::range(meta_count / 3)) {
            auto &v = result.emplace_back();
            if (!(deser._load(v.guid) && deser._load(v.meta_info) && deser._load(v.type_id))) [[unlikely]] {
                LUISA_ERROR("Project json is broken.");
            }
        }
    }
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