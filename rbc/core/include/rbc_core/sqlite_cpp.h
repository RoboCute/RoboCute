#pragma once
#include <luisa/vstl/common.h>
typedef struct sqlite3 sqlite3;

namespace rbc {
// TODO
struct RBC_CORE_API SqliteCpp {
private:
    sqlite3 *_db{};

public:
    struct Result {
        friend struct SqliteCpp;
    private:
        char *_err{};
    public:
        bool is_success() const { return _err == nullptr; }
        Result() = default;
        Result(Result const &) = delete;
        Result &operator=(Result const &) = delete;
        Result &operator=(Result &&rhs) {
            vstd::reset(*this, std::move(rhs));
            return *this;
        }
        char const *error_message() const { return _err; }
        RBC_CORE_API Result(Result &&rhs);
        RBC_CORE_API ~Result();
    };
    enum struct DataType : uint8_t {
        Int,
        Float,
        String,
        Null
    };
    using ValueVariant = vstd::variant<
        int64_t,
        double,
        luisa::string>;
    struct ColumnDesc {
        luisa::string name;
        DataType type{DataType::Int};
        bool primary_key : 1 {false};
        bool unique : 1 {false};
        bool not_null : 1 {false};
    };
    struct ColumnValue {
        luisa::string name;
        luisa::string value;
    };
    auto db() const { return _db; }
    SqliteCpp();
    SqliteCpp(char const *filename);
    SqliteCpp(luisa::string const &str) : SqliteCpp{str.c_str()} {}
    ~SqliteCpp();
    SqliteCpp(SqliteCpp &&rhs);
    SqliteCpp(SqliteCpp const &) = delete;
    bool check_table_exists(luisa::string_view table_name) const;
    Result create_table(
        luisa::string_view table_name,
        luisa::span<ColumnDesc const> column_descs);
    Result insert_values(
        luisa::string_view table_name,
        luisa::span<luisa::string const> column_names,
        luisa::span<ValueVariant const> values,
        bool replace);
    Result delete_with_key(
        luisa::string_view table_name,
        luisa::string_view compare_column_name,
        ValueVariant const &compare_column_value);
    Result update_with_key(
        luisa::string_view table_name,
        luisa::string_view set_column_name,
        ValueVariant const &set_column_value,
        luisa::string_view compare_column_name,
        ValueVariant const &compare_column_value);
    Result select(
        char const *sql_command,
        luisa::move_only_function<void(ColumnValue &&)> const &out_callback) const;
    Result read_columns_with(
        luisa::string_view table_name,
        luisa::move_only_function<void(ColumnValue &&)> const &out_callback,
        luisa::string_view target_column_name = {},
        luisa::string_view compare_column_name = {},
        ValueVariant const &compare_column_value = {}) const;
    Result execute(luisa::span<char const> command) const;
};
}// namespace rbc