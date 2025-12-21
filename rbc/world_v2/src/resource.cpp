#include <rbc_world_v2/resource.h>

namespace rbc {

ResourceHandle::ResourceHandle() {
    std::memset((void *)this, 0, sizeof(ResourceHandle));
}

void ResourceRegistry::FillRequest(ResourceRequest *request, ResourceHeader header, luisa::string_view base_path, luisa::string_view uri) {
}

}// namespace rbc