#include <rbc_core/sqlite_cpp.h>
#include <luisa/core/logging.h>
using namespace rbc;

int main() {
    SqliteCpp sqlite("db.sqlite3");
    if (sqlite.check_table_exists("STUDENT")) {
        LUISA_INFO("Table exists");
        // update
        auto result = sqlite.update_with_key(
            "STUDENT"sv,
            "EMAIL"sv,
            "updated_peter@griffin.com"sv,
            "ID"sv,
            2);
        if (!result.is_success()) {
            LUISA_WARNING("{}", result.error_message());
        }
    }
    // create
    else {
        luisa::vector<SqliteCpp::ColumnDesc> columns;
        columns.emplace_back(
            SqliteCpp::ColumnDesc{
                .name = "ID",
                .primary_key = true,
                .not_null = true});
        columns.emplace_back(
            SqliteCpp::ColumnDesc{
                .name = "FIRST_NAME",
                .type = SqliteCpp::DataType::String,
                .not_null = true});
        columns.emplace_back(
            SqliteCpp::ColumnDesc{
                .name = "LAST_NAME",
                .type = SqliteCpp::DataType::String,
                .not_null = true});
        columns.emplace_back(
            SqliteCpp::ColumnDesc{
                .name = "EMAIL",
                .type = SqliteCpp::DataType::String,
                .not_null = true});
        auto result = sqlite.create_table(
            "STUDENT",
            columns);
        if (!result.is_success()) {
            LUISA_WARNING("{}", result.error_message());
        }
        // insert
        luisa::vector<luisa::string> column_names{
            "ID",
            "FIRST_NAME",
            "LAST_NAME",
            "EMAIL"};
        luisa::vector<SqliteCpp::ValueVariant> values;
        values.push_back(1);
        values.push_back("John"sv);
        values.push_back("Doe"sv);
        values.push_back("john@doe.com"sv);
        values.push_back(2);
        values.push_back("Peter"sv);
        values.push_back("Griffin"sv);
        values.push_back("peter@griffin.com"sv);
        result = sqlite.insert_values(
            "STUDENT",
            column_names,
            values);
        if (!result.is_success()) {
            LUISA_WARNING("{}", result.error_message());
        }

        // delete
        // result = sqlite.delete_with_key(
        //     "STUDENT"sv,
        //     "ID",
        //     1);
        // if (!result.is_success()) {
        //     LUISA_WARNING("{}", result.error_message());
        // }
    }
    luisa::vector<SqliteCpp::ColumnValue> values;

    // read seperate
    {
        auto r = sqlite.read_columns_with("STUDENT", values, "EMAIL", "ID", 2);
        if (!r.is_success()) {
            LUISA_WARNING("{}", r.error_message());
        } else {
            for (auto &i : values) {
                luisa::string value = i.value.visit_or<luisa::string>("NULL", [&](auto &&t) {
                    return luisa::format("{}", t);
                });
                LUISA_INFO("Name: {} Value: {}", i.name, value);
            }
        }
    }
    // read ALL
    {
        auto r = sqlite.read_columns_with("STUDENT", values);
        if (!r.is_success()) {
            LUISA_WARNING("{}", r.error_message());
        } else {
            for (auto &i : values) {
                luisa::string value = i.value.visit_or<luisa::string>("NULL", [&](auto &&t) {
                    return luisa::format("{}", t);
                });
                LUISA_INFO("Name: {} Value: {}", i.name, value);
            }
        }
    }
}