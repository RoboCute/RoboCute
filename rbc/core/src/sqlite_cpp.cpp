#include <rbc_core/sqlite_cpp.h>
#include <rbc_core/utils/parse_string.h>
#include <luisa/core/stl/format.h>
#include <sqlite3.h>

namespace rbc {
namespace sql_detail {
void push_to(
    luisa::vector<char> &vec,
    luisa::string_view str) {
    auto size = vec.size();
    vec.push_back_uninitialized(str.size());
    std::memcpy(vec.data() + size, str.data(), str.size());
}
luisa::string to_sql_str(luisa::string_view strv) {
    luisa::fixed_vector<size_t, 16> indices;
    for (auto i : vstd::range(strv.size())) {
        if (strv[i] == '\'') {
            indices.emplace_back(i);
        }
    }
    luisa::string result;
    result.resize(strv.size() + indices.size() + 2);
    result[0] = '\'';
    size_t offset = 0;
    auto ptr = result.data() + 1;
    for (auto &i : indices) {
        std::memcpy(
            ptr,
            strv.data() + offset,
            i - offset);
        ptr += i - offset;
        *ptr = '\'';
        ++ptr;
        offset = i;
    }
    std::memcpy(
        ptr,
        strv.data() + offset,
        strv.size() - offset);
    result.back() = '\'';
    return result;
}
char *make_sql_string(luisa::string_view data) {
    auto msg = (char *)sqlite3_malloc(data.size() + 1);
    std::memcpy(msg, data.data(), data.size());
    msg[data.size()] = 0;
    return msg;
}
void push_data_to(
    luisa::vector<char> &vec,
    SqliteCpp::ValueVariant const &value_variant) {
    value_variant.visit([&]<typename T>(T const &t) {
        if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, double>) {
            push_to(vec, luisa::format("{}", t));
        } else {
            push_to(vec, to_sql_str(t));
        }
    });
}
}// namespace sql_detail
SqliteCpp::SqliteCpp() : _db{nullptr} {}
SqliteCpp::~SqliteCpp() {
    if (_db == nullptr) return;
    sqlite3_close(_db);
}
SqliteCpp::SqliteCpp(char const *filename) {
    auto opened = sqlite3_open(filename, &_db);
    if (opened) _db = nullptr;
}
SqliteCpp::SqliteCpp(SqliteCpp &&rhs)
    : _db(rhs._db) {
    rhs._db = nullptr;
}
auto SqliteCpp::create_table(
    luisa::string_view table_name,
    luisa::span<ColumnDesc const> column_descs) -> Result {
    bool is_first = true;
    using namespace sql_detail;
    luisa::vector<char> vec;
    vec.reserve(64);
    push_to(vec, "CREATE TABLE ");
    push_to(vec, table_name);
    push_to(vec, " ("sv);
    for (auto &desc : column_descs) {
        if (!is_first) {
            vec.push_back(',');
        }
        is_first = false;
        push_to(vec, desc.name);
        switch (desc.type) {
            case DataType::Int:
                push_to(vec, " INT"sv);
                break;
            case DataType::Float:
                push_to(vec, " REAL"sv);
                break;
            case DataType::String:
                push_to(vec, " TEXT"sv);
                break;
            case DataType::Null:
                push_to(vec, " NULL"sv);
        }
        if (desc.primary_key) {
            if (desc.type == DataType::Null) [[unlikely]] {
                LUISA_ERROR("Null type not supported.");
            }
            push_to(vec, " PRIMARY KEY"sv);
        }
        if (desc.unique) {
            push_to(vec, " UNIQUE"sv);
        }
        if (desc.not_null) {
            if (desc.type == DataType::Null) [[unlikely]] {
                LUISA_ERROR("Null type not supported.");
            }
            push_to(vec, " NOT NULL"sv);
        }
    };
    push_to(vec, " )"sv);
    vec.push_back(';');
    vec.push_back(0);
    return execute(vec);
}
auto SqliteCpp::insert_values(
    luisa::string_view table_name,
    luisa::span<luisa::string const> column_names,
    luisa::span<ValueVariant const> values) -> Result {
    if (values.size() == 0 || values.size() % column_names.size() != 0) [[unlikely]] {
        LUISA_ERROR("values count not aligned as column_names count.");
    }
    luisa::vector<char> vec;
    vec.reserve(64);
    size_t set_values = values.size() / column_names.size();
    vec.clear();
    using namespace sql_detail;
    push_to(vec, "INSERT INTO "sv);
    push_to(vec, table_name);
    vec.push_back(' ');
    vec.push_back('(');
    bool is_first = true;
    for (auto &i : column_names) {
        if (!is_first) {
            vec.push_back(',');
            vec.push_back(' ');
        }
        is_first = false;
        push_to(vec, i);
    }
    push_to(vec, ") VALUES "sv);
    is_first = true;
    auto value_iter = values.begin();
    for (auto i : vstd::range(set_values)) {
        if (!is_first) {
            vec.push_back(',');
            vec.push_back(' ');
        }
        is_first = false;
        vec.push_back('(');
        for (auto j : vstd::range(column_names.size())) {
            if (j > 0) {
                vec.push_back(',');
                vec.push_back(' ');
            }
            if (value_iter->valid()) {
                push_data_to(vec, *value_iter);
            } else {
                push_to(vec, "NULL"sv);
            }
            ++value_iter;
        }
        vec.push_back(')');
    }
    vec.push_back(';');
    vec.push_back(0);
    return execute(vec);
}
auto SqliteCpp::execute(luisa::span<char const> command) -> Result {
    LUISA_DEBUG_ASSERT(command.back() == 0);
    char *err_msg{};
    int rc = sqlite3_exec(_db, command.data(), nullptr, 0, &err_msg);
    // DEBUG
    Result r;
    if (rc != SQLITE_OK) {
        if (err_msg) {
            r._err = err_msg;
        } else {
            r._err = sql_detail::make_sql_string(luisa::format("Sqlite error {}", sqlite3_errmsg(_db)));
        }
    }
    return r;
}
bool SqliteCpp::check_table_exists(luisa::string_view table_name) {
    auto cmd = luisa::format("SELECT name FROM sqlite_master WHERE type='table' AND name='{}';", table_name);
    bool result{false};
    char *err_msg{};
    int rc = sqlite3_exec(
        _db,
        cmd.c_str(),
        +[](void *usr_data, int value, char **, char **) -> int {
            *static_cast<bool *>(usr_data) = true;
            return 0;
        },
        &result,
        &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            LUISA_WARNING("Sqlite error: {}", err_msg);
            sqlite3_free(err_msg);
        }
    }
    return result;
}
auto SqliteCpp::delete_with_key(
    luisa::string_view table_name,
    luisa::string_view column_name,
    ValueVariant const &column_value) -> Result {
    luisa::vector<char> vec;
    vec.reserve(64);
    using namespace sql_detail;
    push_to(vec, "DELETE FROM "sv);
    push_to(vec, table_name);
    push_to(vec, " WHERE "sv);
    push_to(vec, column_name);
    vec.push_back('=');
    push_data_to(vec, column_value);
    vec.push_back(';');
    vec.push_back(0);
    return execute(vec);
}
auto SqliteCpp::update_with_key(
    luisa::string_view table_name,
    luisa::string_view set_column_name,
    ValueVariant const &set_column_value,
    luisa::string_view compare_column_name,
    ValueVariant const &compare_column_value) -> Result {
    luisa::vector<char> vec;
    vec.reserve(64);
    using namespace sql_detail;
    push_to(vec, "UPDATE "sv);
    push_to(vec, table_name);
    push_to(vec, " SET "sv);
    push_to(vec, set_column_name);
    vec.push_back('=');
    push_data_to(vec, set_column_value);
    push_to(vec, " WHERE "sv);
    push_to(vec, compare_column_name);
    vec.push_back('=');
    push_data_to(vec, compare_column_value);
    vec.push_back(';');
    vec.push_back(0);
    return execute(vec);
}
auto SqliteCpp::select(
    char const *sql_command,
    luisa::vector<ColumnValue> &out_result) -> Result {
    sqlite3_stmt *stmt;
    Result r;
    int rc = sqlite3_prepare_v2(_db, sql_command, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        r._err = sql_detail::make_sql_string(luisa::format("Sqlite error {}", sqlite3_errmsg(_db)));
        return r;
    }
    auto NoOfCols = sqlite3_column_count(stmt);//get the number of columns in the table
    bool done = false;
    while (!done) {//while we are still reading rows from the table
        switch (sqlite3_step(stmt)) {
            case SQLITE_ROW:                                                 //case sqlite3_step has another row ready
                for (int i = 0; i < NoOfCols; i++) {                         //iterate through the columns and get data for each column
                    const char *colName = sqlite3_column_name(stmt, i);      //get the column name
                    const unsigned char *text = sqlite3_column_text(stmt, i);//get the value from at that column as text
                    auto text_strv = luisa::string_view{reinterpret_cast<char const *>(text)};
                    auto value = parse_string_to(text_strv);
                    auto &v = out_result.emplace_back();
                    v.name = colName;
                    if (!value.valid()) {
                        v.value = text_strv;
                    } else {
                        value.visit([&](auto &&t) {
                            v.value = t;
                        });
                    }
                }
                break;
            case SQLITE_DONE:          //case sqlite3_step() has finished executing
                sqlite3_finalize(stmt);//destroy the prepared statement object
                return r;
        }
    }
    return r;
}
auto SqliteCpp::read_columns_with(
    luisa::string_view table_name,
    luisa::vector<ColumnValue> &out_result,
    luisa::string_view target_column_name,
    luisa::string_view compare_column_name,
    ValueVariant const &compare_column_value) -> Result {
    luisa::string where_cmd;
    if (!compare_column_name.empty()) {
        where_cmd = luisa::format(" WHERE {}={}", compare_column_name, compare_column_value.visit_or<luisa::string>("NULL", [&]<typename T>(T const &t) {
            if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, double>) {
                return luisa::format("{}", t);
            } else {
                return sql_detail::to_sql_str(t);
            }
        }));
    }
    auto sql_cmd = luisa::format("SELECT {} FROM {}{};", target_column_name.empty() ? "*"sv : target_column_name, table_name, where_cmd);
    return select(sql_cmd.c_str(), out_result);
}
SqliteCpp::Result::Result(Result &&rhs) {
    _err = rhs._err;
    rhs._err = nullptr;
}
SqliteCpp::Result::~Result() {
    if (_err) {
        sqlite3_free(_err);
    }
}
}// namespace rbc