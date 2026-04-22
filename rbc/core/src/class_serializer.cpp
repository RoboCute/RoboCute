#include <rbc_core/class_serializer.h>

namespace rbc {
namespace class_ser_detail {
static vstd::HashMap<vstd::MD5, ClassSerializer::SerdeFuncs> class_map;
}// namespace class_ser_detail

ClassSerializer::ClassSerializer(
    vstd::MD5 type_md5,
    SerdeFuncs serde_func) {
    using namespace class_ser_detail;
    class_map.emplace(type_md5, serde_func);
}
ClassSerializer::~ClassSerializer() = default;
auto ClassSerializer::get_serde_funcs(vstd::MD5 type_md5) -> SerdeFuncs {
    using namespace class_ser_detail;
    auto const iter = class_map.find(type_md5);
    if (!iter) [[unlikely]] {
        return {};
    }
    return iter.value();
}
}// namespace rbc