#include <rbc_world_v2/resource_base.h>

namespace rbc ::world {
void Resource::serialize(ObjSerialize const &obj) const {
    auto type_id = this->type_id();
    obj.ser._store(
        reinterpret_cast<vstd::Guid &>(type_id),
        "__typeid__");
    if (!_path.empty()) {
        obj.ser._store(luisa::to_string(_path), "path");
        obj.ser._store(_file_offset, "file_offset");
    }
}
void Resource::deserialize(ObjDeSerialize const &obj) {
    luisa::string path_str;
    if (obj.ser._load(path_str, "path")) {
        _path = path_str;
        obj.ser._load(_file_offset, "file_offset");
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
bool Resource::save_to_path() {
    if (_path.empty()) return false;
    return unsafe_save_to_path();
}
Resource::Resource() = default;
Resource::~Resource() = default;
}// namespace rbc::world