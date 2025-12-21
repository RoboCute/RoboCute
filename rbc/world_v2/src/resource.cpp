#include <rbc_world_v2/resource.h>

namespace rbc {

IResource::~IResource() {}

ResourceHandle::ResourceHandle() {
    std::memset((void *)this, 0, sizeof(ResourceHandle));
}

void ResourceRegistry::FillRequest(ResourceRequest *request, ResourceHeader header, luisa::string_view base_path, luisa::string_view uri) {
}

// =====================================================
// Resource Record
// =====================================================
uint32_t ResourceRecord::AddReference() {
    ++reference_count;
    return reference_count;
}
void ResourceRecord::RemoveReference() {
    --reference_count;
    if (reference_count == 0) {
        // auto system = GetResourceSystemInstance(); // get global system
        // system->UnloadResource(header.guid);
    }
}
bool ResourceRecord::isReferenced() const {
    return reference_count > 0;
}
void ResourceRecord::SetStatus(EResourceLoadingStatus InStatus) {
    if (InStatus != loading_status) {
        loading_status = InStatus;
    }
}
// =====================================================

// =====================================================
// Resource System
// =====================================================
ResourceSystem::~ResourceSystem() {}
// =====================================================

}// namespace rbc