#include <rbc_world/project.h>
#include <luisa/core/fiber.h>
#include <rbc_core/utils/parse_string.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_world/resource_base.h>
namespace rbc::world {
static constexpr uint sql_column_count = 4;
Project::Project(
    luisa::filesystem::path assets_path,
    luisa::filesystem::path meta_path,
    luisa::filesystem::path binary_path,
    luisa::filesystem::path const &assets_db_path)
    : _assets_path(std::move(assets_path)),
      _meta_path(std::move(meta_path)),
      _binary_path(std::move(binary_path)) {
    if (!luisa::filesystem::is_directory(_meta_path)) {
        luisa::filesystem::create_directories(_meta_path);
    }
    if (!luisa::filesystem::is_directory(_binary_path)) {
        luisa::filesystem::create_directories(_binary_path);
    }
    if (!luisa::filesystem::is_directory(_assets_path)) {
        luisa::filesystem::create_directories(_assets_path);
    }
}
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
void Project::scan_project() {
    luisa::spin_mutex values_mtx;
    luisa::vector<luisa::filesystem::path> paths;
    for (auto &i : std::filesystem::recursive_directory_iterator(_assets_path)) {
        if (!i.is_regular_file() || i.path().extension() == ".rbcmt") {
            continue;
        }
        // TODO: check file ext valid
        paths.emplace_back(i.path());
    }

    luisa::fiber::parallel(
        paths.size(),
        [&](size_t i) {
            auto &path = paths[i];
            bool file_is_dirty{true};
            bool file_meta_is_dirty{false};
            vstd::Guid guid;
            vstd::Guid md5;
            guid.reset();
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
                if (!deser.read(last_write_time, "last_write_time")) [[unlikely]]
                    return;
                if (!deser._load(guid, "guid")) [[unlikely]]
                    return;
                if (!deser._load(md5, "md5")) [[unlikely]] {
                    md5.reset();
                    return;
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

                    if (file_md5 != reinterpret_cast<vstd::MD5 &>(md5)) {
                        reinterpret_cast<vstd::MD5 &>(md5) = file_md5;
                        return;
                    }
                    file_meta_is_dirty = true;
                }
                file_is_dirty = false;
            }();
            if (!(file_meta_is_dirty || file_is_dirty)) return;// file is clean
            file_meta_is_dirty |= file_is_dirty;
            if (!guid) {
                guid = vstd::Guid{true};
            }
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
                RC<Resource> res{_import_file(path, _binary_path / (guid.to_string() + "rbcb"), guid)};
                if (!res) {
                    return;
                }
                update();
                register_resource(res.get());
            }
            if (file_meta_is_dirty) {
                update();
                auto meta_json = luisa::format(R"({{"guid":{},"md5":{},"last_write_time":{}}})", guid.to_base64(), md5.to_base64(), file_last_write_time);
                BinaryFileWriter file_writer{
                    luisa::to_string(path) + ".rbcmt"};
                file_writer.write({reinterpret_cast<std::byte const *>(meta_json.data()),
                                   meta_json.size()});
            }
        });
}

Resource *Project::_import_file(
    luisa::filesystem::path const &origin_file_path,
    luisa::filesystem::path const &binary_file_path,
    vstd::Guid file_guid) {
    // TODO
    return nullptr;
}

Project::~Project() {}
}// namespace rbc::world