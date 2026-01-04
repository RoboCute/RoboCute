#include <rbc_world/project.h>
#include <luisa/core/fiber.h>
#include <rbc_core/utils/parse_string.h>
namespace rbc::world {
Project::Project(
    luisa::filesystem::path assets_path,
    luisa::filesystem::path meta_path,
    luisa::filesystem::path binary_path,
    luisa::filesystem::path const &assets_db_path)
    : _db([&] {
          if (assets_db_path.has_parent_path()) {
              auto &&dir = assets_db_path.parent_path();
              if (!dir.empty() && !luisa::filesystem::exists(dir)) {
                  luisa::filesystem::create_directories(dir);
              }
          }
          return luisa::to_string(assets_db_path);
      }()),
      _assets_path(std::move(assets_path)),
      _meta_path(std::move(meta_path)),
      _binary_path(std::move(binary_path)) {
    if (!_db.db()) [[unlikely]] {
        LUISA_ERROR("Create project database failed.");
    }
    if (!luisa::filesystem::is_directory(_meta_path)) {
        luisa::filesystem::create_directories(_meta_path);
    }
    if (!luisa::filesystem::is_directory(_binary_path)) {
        luisa::filesystem::create_directories(_binary_path);
    }
    if (!luisa::filesystem::is_directory(_assets_path)) {
        luisa::filesystem::create_directories(_assets_path);
    }
    if (!_db.check_table_exists("RBC_FILE_META"sv)) {
        luisa::vector<SqliteCpp::ColumnDesc> columns;
        columns.emplace_back(
            SqliteCpp::ColumnDesc{
                .name = "PATH",
                .type = SqliteCpp::DataType::String,
                .primary_key = true,
                .unique = true,
                .not_null = true});
        columns.emplace_back(
            SqliteCpp::ColumnDesc{
                .name = "GUID",
                .type = SqliteCpp::DataType::String,
                .not_null = true});
        columns.emplace_back(
            SqliteCpp::ColumnDesc{
                .name = "LAST_WRITE_TIME",
                .type = SqliteCpp::DataType::Int,
                .not_null = true});
        columns.emplace_back(
            SqliteCpp::ColumnDesc{
                .name = "META_LAST_WRITE_TIME",
                .type = SqliteCpp::DataType::String,
                .not_null = true});
        _db.create_table(
            "RBC_FILE_META"sv,
            columns);
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
        if (!i.is_regular_file()) {
            continue;
        }
        // TODO: check file ext valid
        paths.emplace_back(i.path());
    }
    luisa::vector<SqliteCpp::ValueVariant> insert_values;
    luisa::fiber::parallel(
        paths.size(),
        [&](size_t i) {
            auto &path = paths[i];
            auto path_key = detail::get_path_key(path, _assets_path);
            luisa::fixed_vector<SqliteCpp::ColumnValue, 4> values;
            _db.read_columns_with("RBC_FILE_META"sv, [&](SqliteCpp::ColumnValue &&value) { values.emplace_back(std::move(value)); } /*read all meta*/, "PATH", path_key);
            vstd::Guid file_guid;
            file_guid.reset();
            luisa::filesystem::path file_meta_path;
            bool file_is_dirty = [&] {
                if (values.size() != 4) return true;
                if (values[1].name != "GUID") return true;
                auto &&guid_str = values[1].value;
                if (auto guid = vstd::Guid::TryParseGuid(guid_str)) {
                    file_guid = *guid;
                } else {
                    return true;
                }
                if (values[2].name != "LAST_WRITE_TIME") return true;
                if (values[3].name != "META_LAST_WRITE_TIME") return true;
                auto last_time = luisa::filesystem::last_write_time(path).time_since_epoch().count();
                // original file dirty
                auto parse_last_time = parse_string_to_int(values[2].value);
                if (!parse_last_time || *parse_last_time != last_time) {
                    return true;
                }
                file_meta_path = _meta_path / (file_guid.to_string() + ".rbcmt");
                // meta file illegal
                if (!luisa::filesystem::is_regular_file(file_meta_path)) {
                    return true;
                }
                // meta file dirty
                auto meta_parse_last_time = parse_string_to_int(values[3].value);
                if (!meta_parse_last_time || luisa::filesystem::last_write_time(file_meta_path).time_since_epoch().count() != *meta_parse_last_time) {
                    return true;
                }
                return false;
            }();
            if (!file_guid) {
                file_guid = vstd::Guid{true};
            }
            if (!file_is_dirty) return;
            std::array<SqliteCpp::ValueVariant, 4> local_values;
            if (!values.empty())
                _db.delete_with_key("RBC_FILE_META"sv, "PATH", path_key);
            if (file_meta_path.empty())
                file_meta_path = _meta_path / (file_guid.to_string() + ".rbcmt");
            // TODO: import
            if (!_import_file(
                    path,
                    file_meta_path,
                    _binary_path / (file_guid.to_string(), ".rbcb"),
                    file_guid)) return;
            local_values[0] = path_key;
            local_values[1] = file_guid.to_base64();
            local_values[2] = luisa::filesystem::last_write_time(path).time_since_epoch().count();
            if (luisa::filesystem::is_regular_file(file_meta_path)) {
                local_values[3] = luisa::filesystem::last_write_time(file_meta_path).time_since_epoch().count();
            } else {
                local_values[3] = int64_t(0);
            }
            values_mtx.lock();
            size_t start_idx = insert_values.size();
            insert_values.push_back_uninitialized(local_values.size());
            for (auto &i : local_values) {
                std::construct_at(
                    std::addressof(insert_values[start_idx]),
                    std::move(i));
                ++start_idx;
            }
            values_mtx.unlock();
        },
        128);
    if (!insert_values.empty()) {
        std::initializer_list<luisa::string>
            columns = {
                "PATH",
                "GUID",
                "LAST_WRITE_TIME",
                "META_LAST_WRITE_TIME"};
        _db.insert_values("RBC_FILE_META"sv, columns, insert_values);
    }
}

bool Project::_import_file(
    luisa::filesystem::path const &origin_file_path,
    luisa::filesystem::path const &meta_file_path,
    luisa::filesystem::path const &binary_file_path,
    vstd::Guid file_guid) {
    // TODO
    return true;
}

Project::~Project() {}
}// namespace rbc::world