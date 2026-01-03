#pragma once
#include <luisa/vstl/common.h>
typedef struct sqlite3 sqlite3;

namespace rbc {
// TODO
struct RBC_CORE_API SqliteCpp {
private:
    sqlite3 *_db;

public:
    struct Result {
        luisa::string err;
        bool is_success() const { return err.empty(); }
    };
    enum struct DataType : uint8_t {
        Int,
        Float,
        String,
        Bool
    };
    using ValueType = vstd::variant<
        int64_t,
        double,
        luisa::string,
        bool>;
    struct ColumnDesc {
        luisa::string name;
        DataType type{DataType::Int};
        bool primary_key : 1 {false};
        bool unique : 1 {true};
        bool not_null : 1 {true};
    };
    auto db() const { return _db; }
    SqliteCpp();
    SqliteCpp(char const *filename);
    ~SqliteCpp();
    SqliteCpp(SqliteCpp &&rhs);
    SqliteCpp(SqliteCpp const &) = delete;
    Result create_table(
        luisa::string_view table_name,
        luisa::span<ColumnDesc const> column_descs);
    
};
}// namespace rbc