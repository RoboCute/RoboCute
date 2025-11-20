#include <rbc_core/type_info.h>
#include <luisa/vstl/v_guid.h>
namespace rbc {
RBC_CORE_API luisa::string TypeInfo::md5_to_string(bool upper) const {
    return reinterpret_cast<vstd::Guid const *>(_md5)->to_string(upper);
}
RBC_CORE_API luisa::string TypeInfo::md5_to_base64() const {
    return reinterpret_cast<vstd::Guid const *>(_md5)->to_base64();
}
}// namespace rbc