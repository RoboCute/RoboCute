#include <rbc_world/project.h>
#include <luisa/core/fiber.h>
#include <rbc_core/utils/parse_string.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_world/resource_base.h>
#include <rbc_world/resource_importer.h>
namespace rbc::world {
static constexpr uint sql_column_count = 4;
Project::Project(
    luisa::filesystem::path assets_path,
    luisa::filesystem::path meta_path,
    luisa::filesystem::path const &assets_db_path)
    : _assets_path(std::move(assets_path)),
      _meta_path(std::move(meta_path)) {
    if (!luisa::filesystem::is_directory(_meta_path)) {
        luisa::filesystem::create_directories(_meta_path);
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
            luisa::vector<vstd::Guid> guids;
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
                uint64_t guid_count;
                if (deser.start_array(guid_count, "guids")) {
                    guids.reserve(guid_count);
                    for (auto i : vstd::range(guid_count)) {
                        vstd::Guid guid;
                        if (!deser._load(guid)) break;
                        guids.emplace_back(guid);
                    }
                    deser.end_scope();
                }
                if (!deser.read(last_write_time, "last_time")) [[unlikely]]
                    return;
                if (!deser._load(md5, "md5")) [[unlikely]] {
                    md5.reset();
                    return;
                }
                if (!deser._load(file_is_dirty, "dirty") || file_is_dirty) {
                    file_is_dirty = true;
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
                // TODO: may need to process dirty file
            }
            if (file_meta_is_dirty) {
                update();
                luisa::fixed_vector<char, 64> guid_str;
                for (auto i : vstd::range(guids.size())) {
                    if (i > 0) {
                        guid_str.push_back(',');
                    }
                    auto s = guids[i].to_base64();
                    auto start_idx = guid_str.size();
                    guid_str.push_back_uninitialized(s.size());
                    std::memcpy(guid_str.data() + start_idx, s.data(), s.size());
                }
                auto meta_json = luisa::format(R"({{"guids":[{}],"md5":{},"dirty":{},"last_time":{}}})", luisa::string_view{guid_str.data(), guid_str.size()}, md5.to_base64(), file_is_dirty ? "true"sv : "false"sv, file_last_write_time);
                BinaryFileWriter file_writer{
                    luisa::to_string(path) + ".rbcmt"};
                file_writer.write({reinterpret_cast<std::byte const *>(meta_json.data()),
                                   meta_json.size()});
            }
        });
}

Project::~Project() {}
}// namespace rbc::world