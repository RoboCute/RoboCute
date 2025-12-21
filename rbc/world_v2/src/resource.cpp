#include <rbc_world_v2/resource.h>

namespace rbc {

ResourceHandle::ResourceHandle() {
    std::memset((void *)this, 0, sizeof(ResourceHandle));
}

}// namespace rbc