#include <rbc_world_v2/resource_base.h>

namespace rbc ::world {
void ResourceBaseImpl::_rbc_objser(JsonSerializer &ser) const {
    if (!_path.empty()) {
        ser._store(luisa::to_string(_path), "path");
        ser._store(_file_offset, "file_offset");
    }
}
void ResourceBaseImpl::_rbc_objdeser(JsonDeSerializer &ser) {
    luisa::string path_str;
    if (ser._load(path_str, "path")) {
        _path = path_str;
        ser._load(_file_offset, "file_offset");
    }
}
}// namespace rbc::world