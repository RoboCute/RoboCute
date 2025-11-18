#include <rbc_core/type.h>
#include <luisa/vstl/md5.h>
namespace rbc {
RBC_CORE_API luisa::string Type::md5_to_string(bool upper) const {
    vstd::MD5 md5{vstd::MD5::MD5Data{_md5[0], _md5[1]}};
    return md5.to_string(upper);
}
}// namespace rbc