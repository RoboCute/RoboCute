#include <rbc_world_v2/resource_base.h>

namespace rbc ::world {
void Resource::rbc_objser(JsonSerializer &ser) const {
    if (!_path.empty()) {
        ser._store(luisa::to_string(_path), "path");
        ser._store(_file_offset, "file_offset");
    }
}
void Resource::rbc_objdeser(JsonDeSerializer &ser) {
    luisa::string path_str;
    if (ser._load(path_str, "path")) {
        _path = path_str;
        ser._load(_file_offset, "file_offset");
    }
}
void Resource::set_path(
    luisa::filesystem::path const &path,
    uint64_t const &file_offset) {
    if (!_path.empty() || _file_offset != 0) [[unlikely]] {
        LUISA_ERROR("Resource path already setted.");
    }
    _path = path;
    _file_offset = file_offset;
}
}// namespace rbc::world