#include <rbc_core/sqlite_cpp.h>
#include <sqlite3.h>

namespace rbc {
SqliteCpp::SqliteCpp() : _db{nullptr} {}
SqliteCpp::~SqliteCpp() {
    if (_db == nullptr) return;
    sqlite3_close(_db);
}
SqliteCpp::SqliteCpp(char const *filename) {
    auto opened = sqlite3_open(filename, &_db);
    if (!opened) _db = nullptr;
}
SqliteCpp::SqliteCpp(SqliteCpp &&rhs)
    : _db(rhs._db) {
    rhs._db = nullptr;
}
}// namespace rbc