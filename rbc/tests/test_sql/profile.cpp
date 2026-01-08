#include <rbc_core/sqlite_cpp.h>
#include <rbc_core/sqlite_cpp.h>

#include <luisa/core/clock.h>
#include <luisa/core/logging.h>
#include <luisa/vstl/lmdb.hpp>
#include <luisa/vstl/v_guid.h>
void profile() {
    using namespace rbc;
    luisa::vector<
        std::pair<luisa::vector<std::byte>,
                  luisa::vector<std::byte>>>
        key_values;
    auto push = [&](luisa::vector<std::byte> &vec, luisa::string_view s) {
        auto sz = vec.size();
        vec.push_back_uninitialized(s.size());
        std::memcpy(vec.data() + sz, s.data(), s.size());
    };
    constexpr uint test_count = 10000;
    key_values.resize(test_count);
    // make data
    LUISA_INFO("Generating random key-value...");
    for (auto &i : key_values) {
        for (auto k : vstd::range(10)) {
            vstd::Guid guid{true};
            vstd::Guid guid1{true};
            push(i.first, guid.to_string());
            push(i.second, guid1.to_string());
        }
    }
    // insert
    luisa::Clock clk;
    vstd::LMDB lmdb{"db.lmdb"};
    rbc::SqliteCpp sqlite("db.lmdb/db.sqlite");
    luisa::fixed_vector<SqliteCpp::ColumnDesc, 2> columns;
    columns.emplace_back(
        SqliteCpp::ColumnDesc{
            .name = "K",
            .primary_key = true,
            .unique = true,
            .not_null = true});
    columns.emplace_back(
        SqliteCpp::ColumnDesc{
            .name = "V",
            .not_null = true});
    LUISA_ASSERT(sqlite.create_table("TEST", columns).is_success());
    std::array<SqliteCpp::ValueVariant, 2> column_values;
    std::array<luisa::string, 2> column_names;
    column_values[0] = luisa::string{};
    column_values[1] = luisa::string{};
    column_names[0] = "K";
    column_names[1] = "V";
    clk.tic();
    for (auto &i : key_values) {
        column_values[0].force_get<luisa::string>() = luisa::string_view{
            (char *)i.first.data(), i.first.size()};
        column_values[1].force_get<luisa::string>() = luisa::string_view{
            (char *)i.second.data(), i.second.size()};
        sqlite.insert_values("TEST"sv, column_names, column_values, true);
    }
    auto time = clk.toc();
    LUISA_INFO("Sqlite insert time {} ms", time);
    luisa::vector<vstd::LMDBWriteCommand> commands;
    commands.reserve(key_values.size());
    for (auto &i : key_values) {
        commands.emplace_back(i.first, i.second);
    }
    clk.tic();
    lmdb.write_all(std::move(commands));
    time = clk.toc();
    LUISA_INFO("LMDB insert time {} ms", time);
    clk.tic();
    for (auto i : vstd::range(test_count * 10)) {
        auto idx = rand() % test_count;
        sqlite.read_columns_with("TEST"sv, [](auto &&) {}, "V", "K", luisa::string{
                                                                         (char *)key_values[idx].first.data(),
                                                                         key_values[idx].first.size(),
                                                                     });
    }
    time = clk.toc();
    LUISA_INFO("Sqlite read time {} ms", time);
    clk.tic();
    for (auto i : vstd::range(test_count * 10)) {
        auto idx = rand() % test_count;
        lmdb.read(key_values[idx].first);
    }
    time = clk.toc();
    LUISA_INFO("LMDB read time {} ms", time);
}