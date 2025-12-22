#include <rbc_world_v2/resource.h>
#include <rbc_core/type_info.h>
#include <rbc_core/memory.h>
#include <tracy_wrapper.h>

namespace rbc {

// =====================================================
// IResource
// =====================================================
IResource::~IResource() {}

// =====================================================
// ResourceHandle
// =====================================================
ResourceHandle::ResourceHandle() {
    std::memset((void *)this, 0, sizeof(ResourceHandle));
}

ResourceHandle::ResourceHandle(const ResourceHandle &other) : padding(0), pointer(0) {
    if (get_guid() != other.get_guid())
        reset();

    if (other.is_null()) {
        std::memset((void *)this, 0, sizeof(ResourceHandle));
    } else if (other.is_guid()) {
        guid = other.guid;
    } else if (auto record = other.get_record()) {
        acquire_record(record);
    }
}

ResourceHandle &ResourceHandle::operator=(const ResourceHandle &other) {
    if (get_guid() != other.get_guid())
        reset();

    if (other.is_null()) {
        std::memset((void *)this, 0, sizeof(ResourceHandle));
    } else if (other.is_guid()) {
        guid = other.guid;
    } else if (auto record = other.get_record()) {
        acquire_record(record);
    }
    return *this;
}

ResourceHandle::ResourceHandle(const vstd::Guid &other) : padding(0), pointer(0) {
    if (get_guid() != other)
        reset();

    guid = other;
    LUISA_ASSERT(is_guid() || is_null());
}

ResourceHandle &ResourceHandle::operator=(const vstd::Guid &other) {
    if (get_guid() != other)
        reset();

    guid = other;
    LUISA_ASSERT(is_guid() || is_null());
    return *this;
}

ResourceHandle::ResourceHandle(ResourceHandle &&other) : padding(0), pointer(0) {
    if (get_guid() != other.get_guid())
        reset();

    memcpy((void *)this, &other, sizeof(ResourceHandle));
    memset((void *)&other, 0, sizeof(ResourceHandle));
}

ResourceHandle &ResourceHandle::operator=(ResourceHandle &&other) {
    if (get_guid() != other.get_guid())
        reset();

    memcpy((void *)this, &other, sizeof(ResourceHandle));
    memset((void *)&other, 0, sizeof(ResourceHandle));
    return *this;
}

ResourceHandle::~ResourceHandle() {
    reset();
}

ResourceHandle::operator bool() const {
    return !is_null();
}

bool ResourceHandle::operator==(const ResourceHandle &other) const {
    return get_guid() == other.get_guid();
}

void *ResourceHandle::load() {
    return load(false);
}

void *ResourceHandle::install() {
    return load(true);
}

void ResourceHandle::reset() {
    if (padding == 0 && pointer != 0) {
        if (auto record = get_record()) {
            record->RemoveReference();
        }
    }
    std::memset((void *)this, 0, sizeof(ResourceHandle));
}

void *ResourceHandle::get_loaded() const {
    if (auto record = get_record()) {
        if (record->loading_status >= EResourceLoadingStatus::Loaded)
            return record->resource;
    }
    return nullptr;
}

void *ResourceHandle::get_installed() const {
    if (auto record = get_record()) {
        if (record->loading_status >= EResourceLoadingStatus::Installed)
            return record->resource;
    }
    return nullptr;
}

bool ResourceHandle::is_null() const {
    return padding == 0 && pointer == 0;
}

bool ResourceHandle::is_guid() const {
    return padding != 0;
}

bool ResourceHandle::is_loaded() const {
    return get_status() == EResourceLoadingStatus::Loaded;
}

bool ResourceHandle::is_installed() const {
    return get_status() == EResourceLoadingStatus::Installed;
}

vstd::Guid ResourceHandle::get_guid() const {
    if (is_guid()) {
        return guid;
    } else if (const auto record = get_record()) {
        return record->header.guid;
    }
    return {};
}

rbc::TypeInfo ResourceHandle::get_type() const {
    LUISA_ASSERT(padding == 0);
    if (const auto record = get_record()) {
        return record != nullptr ? record->header.type : rbc::TypeInfo::invalid();
    }
    return rbc::TypeInfo::invalid();
}

EResourceLoadingStatus ResourceHandle::get_status() const {
    if (is_null()) {
        return EResourceLoadingStatus::Unloaded;
    } else if (auto record = get_record()) {
        return record->loading_status;
    }
    return EResourceLoadingStatus::Unloaded;
}

ResourceHandle ResourceHandle::clone() {
    return *this;
}

void *ResourceHandle::load(bool requireInstalled) {
    auto system = GetResourceSystem();

    if (is_null()) {
        return nullptr;
    } else if (is_guid())// new acquire
    {
        auto record = system->FindResourceRecord(guid);
        if (record == nullptr) {
            record = system->LoadResource(*this, requireInstalled);
        }
        if (record != nullptr) {
            acquire_record(record);
            if (record->loading_status >= EResourceLoadingStatus::Loaded) {
                return record->resource;
            }
        }
    } else if (auto record = get_record()) {
        if (record->loading_status >= EResourceLoadingStatus::Loaded)
            return record->resource;
    }
    return nullptr;
}

ResourceRecord *ResourceHandle::get_record() const {
    if (is_null()) {
        return nullptr;
    } else if (is_guid()) {
        auto system = GetResourceSystem();
        return system->FindResourceRecord(guid);
    }
    return pointer;
}

void ResourceHandle::acquire_record(ResourceRecord *record) const {
    if (pointer != record) {
        if (padding == 0 && pointer != 0) {
            get_record()->RemoveReference();
            pointer = 0;
        }
        record->AddReference();
        padding = 0;
        pointer = record;
    }
}

// =====================================================
// ResourceHeader
// =====================================================
void ResourceHeader::serialize(rbc::JsonSerializer &obj) const {
    obj._store(guid, "guid");
    auto md5_array = type.md5();
    vstd::Guid type_guid = *reinterpret_cast<vstd::Guid const *>(md5_array.data());
    obj._store(type_guid, "type");
    obj._store(version, "version");
    luisa::vector<vstd::Guid> dep_guids;
    dep_guids.reserve(dependencies.size());
    for (auto const &dep : dependencies) {
        dep_guids.push_back(dep.get_guid());
    }
    obj._store(dep_guids, "dependencies");
}
void ResourceHeader::deserialize(rbc::JsonDeSerializer &obj) {
    obj._load(guid, "guid");
    vstd::Guid type_guid{};
    obj._load(type_guid, "type");
    auto md5_data = reinterpret_cast<std::array<uint64_t, 2> const &>(type_guid);
    type = rbc::TypeInfo{"", md5_data[0], md5_data[1]};
    obj._load(version, "version");
    luisa::vector<vstd::Guid> dep_guids;
    obj._load(dep_guids, "dependencies");
    dependencies.clear();
    dependencies.reserve(dep_guids.size());
    for (auto const &dep_guid : dep_guids) {
        dependencies.emplace_back(dep_guid);
    }
}
// =====================================================

// =====================================================
// ResourceRegistry
// =====================================================
void ResourceRegistry::FillRequest(ResourceRequest *request, ResourceHeader header, luisa::string_view base_path, luisa::string_view uri) {

    if (request) {
        // fill meta and dependencies with header
        request->resource_record->header.type = header.type;
        request->resource_record->header.version = header.version;
        request->resource_record->header.dependencies = header.dependencies;

        // fill local registry
        // TODO: concat base path
        request->resource_url = uri;
        request->system = request->system;

        // find factory
        request->factory = request->system->FindFactory(request->resource_record->header.type);
    }
}

// =====================================================
// ResourceRequest
// =====================================================
vstd::Guid ResourceRequest::GetGuid() const {
    return resource_record ? resource_record->header.guid : vstd::Guid{};
}

luisa::span<const uint8_t> ResourceRequest::GetData() const {
    if (!blob) {
        return {};
    }
    return {blob->get_data(), blob->get_size()};
}

luisa::span<const ResourceHandle> ResourceRequest::GetDependencies() const {
    auto &dependencies = resource_record->header.dependencies;
    return {dependencies.data(), dependencies.size()};
}

void ResourceRequest::Update() {
    RBCZoneScopedN("ResourceRequest::Update");
}

bool ResourceRequest::Okay() {
    return true;
}

bool ResourceRequest::Failed() {
    return true;
}

void ResourceRequest::UnloadDependenciesInternal() {
    auto &dependencies = resource_record->header.dependencies;
    for (auto &dep : dependencies) {
        dep.reset();
    }
}

// =====================================================
// LocalResourceRegistry
// =====================================================
LocalResourceRegistry::LocalResourceRegistry(luisa::string_view base_path) : _base_path(base_path) {
    // TODO: 初始化本地资源注册表
}

bool LocalResourceRegistry::RequestResourceFile(ResourceRequest *request) {
    // TODO: 请求资源文件
    // 根据 base_path 和 request 的 guid/uri 构建文件路径
    // 启动异步文件读取，填充 request->blob
    return false;// 占位符
}

bool LocalResourceRegistry::CancelRequestFile(ResourceRequest *request) {
    // TODO: 取消资源文件请求
    // 停止正在进行的文件读取操作
    return false;// 占位符
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
        auto system = GetResourceSystem();// get global system
        system->UnloadResource(header.guid);
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
ResourceSystem::ResourceSystem() : counter(true) {}

ResourceSystem::~ResourceSystem() {
    Shutdown();
}

void ResourceSystem::Initialize(ResourceRegistry *provider) {
    LUISA_ASSERT(provider);
    resource_registry = provider;
}

bool ResourceSystem::IsInitialized() {
    return resource_registry != nullptr;
}

void ResourceSystem::Shutdown() {
    // unload already unloaded and error records
    for (auto &pair : resource_records) {
        auto record = pair.second;
        if (record->loading_status == EResourceLoadingStatus::Unloaded || record->loading_status == EResourceLoadingStatus::Error) {
            continue;
        }
        UnloadResourceInternal(record);
    }
    // clear all finished requests
    ClearFinishedRequestsInternal();
    // mark quit and last update for ongoing requests
    quit = true;
    Update();
    // spin until all requests uninstalled
    while (!to_update_requests.empty()) {
        Update();
    }
    // now delete all records
    for (auto &pair : resource_records) {
        auto record = pair.second;
        RBCDelete(record);
    }
    resource_records.clear();

    resource_registry = nullptr;
}

void ResourceSystem::Update() {
    {
        ResourceRequest *request = nullptr;
        while (request_queue.try_dequeue(request)) {
            to_update_requests.emplace_back(request);
        }
        ClearFinishedRequestsInternal();
    }
    // TODO: time limit
    {
        for (auto req : to_update_requests) {
            auto record = req->resource_record;
            uint32_t spinCounter = 0;
            EResourceLoadingStatus last_phase;
            while (!req->Okay() && spinCounter < 16) {
                last_phase = record->loading_status;
                req->Update();
                if (last_phase == record->loading_status)
                    spinCounter++;
                else
                    spinCounter = 0;
            };
        }
    }
}

bool ResourceSystem::WaitRequest() {
    if (quit)
        return false;
    counter.wait();
    return !quit;
}

void ResourceSystem::Quit() {
    quit = true;
    counter.add(1);
}

ResourceRecord *ResourceSystem::RegisterResource(const vstd::Guid &guid) {
    auto record = FindResourceRecord(guid);
    if (!record) {
        record = RBCNew<ResourceRecord>();
        record->header.guid = guid;
        record->loading_status = EResourceLoadingStatus::Unloaded;
        {
            resource_records_mtx.lock();
            resource_records.emplace(guid, record);
            resource_records_mtx.unlock();
        }
    }
    return record;
}

ResourceRecord *ResourceSystem::LoadResource(const ResourceHandle handle, bool requireInstall) {
    LUISA_ASSERT(!quit);
    LUISA_ASSERT(handle.is_guid());
    auto guid = handle.get_guid();
    auto *record = RegisterResource(guid);
    if (record->loading_status >= EResourceLoadingStatus::Loading) {
        // already loaded
        return record;
    }
    if (record->loading_status == EResourceLoadingStatus::Unloaded) {
        record->loading_status = EResourceLoadingStatus::Loading;
    }

    if (auto request = record->active_request) {
        request->requireInstall = requireInstall;
    } else {
        // new request
        request = RBCNew<ResourceRequest>();
        request->requireInstall = requireInstall;
        request->resource_record = record;
        request->system = this;
        request->factory = nullptr;

        // append to record
        record->active_request = request;
        record->loading_status = EResourceLoadingStatus::Loading;

        // enqueue
        counter.add(1);
        request_queue.enqueue(request);
    }
    return record;
}

ResourceHandle ResourceSystem::EnqueueResource(vstd::Guid guid, rbc::TypeInfo type, rbc::IResource *resource, bool requireInstall, luisa::vector<ResourceHandle> dependencies, EResourceLoadingStatus status) {
    // 运行时手动注册Resource，常用于PCG或者测试Resource
    ResourceHandle handle = guid;
    auto *record = FindResourceRecord(guid);
    if (record == nullptr) {
        record = RegisterResource(guid);
    }
    handle.acquire_record(record);

    // check dependencies
    for (auto dependency : dependencies) {
        LUISA_ASSERT(!dependency.is_null());
    }

    if (auto request = record->active_request) {
        request->requireInstall = requireInstall;
    } else {
        request = RBCNew<ResourceRequest>();
        request->requireInstall = requireInstall;
        request->resource_record = record;
        request->system = this;
        request->factory = FindFactory(type);
        counter.add(1);
        request_queue.enqueue(request);

        record->active_request = request;
        record->loading_status = status;
        record->resource = resource;
        record->header.guid = guid;
        record->header.type = type;
        record->header.dependencies = dependencies;
    }

    return handle;
}

void ResourceSystem::UnloadResource(const ResourceHandle handle) {
    if (quit) return;
    LUISA_ASSERT(!handle.is_null());
    if (auto record = handle.get_record()) {
        LUISA_ASSERT(record->loading_status != EResourceLoadingStatus::Unloaded);
        record->RemoveReference();
        memset((void *)&handle, 0, sizeof(ResourceHandle));
        UnloadResourceInternal(record);
    }
}

void ResourceSystem::FlushResource(const ResourceHandle handle) {
    LUISA_ERROR("NOT IMPLEMENTED");
}

ResourceFactory *ResourceSystem::FindFactory(rbc::TypeInfo type) const {

    const_cast<luisa::spin_mutex &>(resource_factories_mtx).lock();
    auto iter = resource_factories.find(type);
    auto *result = iter ? iter.value() : nullptr;
    const_cast<luisa::spin_mutex &>(resource_factories_mtx).unlock();
    return result;
}

void ResourceSystem::RegisterFactory(ResourceFactory *factory) {
    auto type = factory->GetResourceType();
    resource_factories_mtx.lock();
    auto iter = resource_factories.find(type);
    LUISA_ASSERT(!iter, "Duplicate Registration for ResourceFactory");
    resource_factories.emplace(type, factory);
    resource_factories_mtx.unlock();
}

void ResourceSystem::UnregisterFactory(rbc::TypeInfo type) {
    resource_factories_mtx.lock();
    resource_factories.remove(type);
    resource_factories_mtx.unlock();
}

ResourceRegistry *ResourceSystem::GetRegistry() const {
    return resource_registry;
}

ResourceRecord *ResourceSystem::FindResourceRecord(const vstd::Guid &guid) {
    resource_records_mtx.lock();
    auto iter = resource_records.find(guid);
    auto *result = iter ? iter.value() : nullptr;
    resource_records_mtx.unlock();
    return result;
}

void ResourceSystem::UnloadResourceInternal(ResourceRecord *record) {
    LUISA_ASSERT(!quit);

    if (record->loading_status == EResourceLoadingStatus::Error || record->loading_status == EResourceLoadingStatus::Unloading) {
        DestroyRecordInternal(record);
        return;
    }
    if (auto request = record->active_request) {
    } else {
        request = RBCNew<ResourceRequest>();
        request->requireInstall = false;
        request->resource_record = record;
        request->system = this;

        if (record->loading_status == EResourceLoadingStatus::Installed) {
            record->loading_status = EResourceLoadingStatus::Uninstalling;
        } else if (record->loading_status == EResourceLoadingStatus::Loaded) {
            record->loading_status = EResourceLoadingStatus::Unloading;
        } else [[unlikely]] {
            LUISA_ERROR("Unreachable!");
        }

        request->factory = this->FindFactory(record->header.type);
        record->active_request = request;
        counter.add(1);
        request_queue.enqueue(request);
    }
}

void ResourceSystem::DestroyRecordInternal(ResourceRecord *record) {
    resource_records_mtx.lock();
    auto request = record->active_request;
    if (request) {
        request->resource_record = nullptr;
    }
    resource_records.remove(record->header.guid);
    resource_records_mtx.unlock();

    if (record->resource) {
        resource_to_update_mtx.lock();
        resource_to_record.remove(record->header.guid);
        resource_to_update_mtx.unlock();
    }
    rbc_free(record);
}

void ResourceSystem::ClearFinishedRequestsInternal() {
    to_update_requests.erase(
        std::remove_if(to_update_requests.begin(), to_update_requests.end(), [&](ResourceRequest *req) {
            if (req->Okay()) {
                if (req->resource_record) {
                    req->resource_record->active_request = nullptr;
                }
                RBCDelete(req);
                counter.done();
                return true;
            }
            if (req->Failed()) {
                failed_requests.push_back(req);
                counter.done();
                return true;
            }
            return false;
        }),
        to_update_requests.end());

    failed_requests.erase(
        std::remove_if(failed_requests.begin(), failed_requests.end(), [&](ResourceRequest *req) {
            if (!req->resource_record) {
                RBCDelete(req);
                return true;
            }
            return false;
        }),
        failed_requests.end());
}

ResourceSystem *GetResourceSystem() {
    static ResourceSystem system;
    return &system;
}

// =====================================================

}// namespace rbc