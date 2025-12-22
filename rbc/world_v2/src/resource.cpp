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

void ResourceRequest::Okey() {
    // TODO: 标记请求成功完成
    // 设置 resource_record 的状态为 Loaded/Installed
    // 通知等待的 fiber
    if (resource_record) {
        resource_record->SetStatus(EResourceLoadingStatus::Loaded);
        if (requestInstall) {
            resource_record->SetStatus(EResourceLoadingStatus::Installed);
        }
    }
    serde_event.signal();
}

void ResourceRequest::Failed() {
    // TODO: 标记请求失败
    // 设置 resource_record 的状态为 Error
    // 通知等待的 fiber
    if (resource_record) {
        resource_record->SetStatus(EResourceLoadingStatus::Error);
    }
    serde_event.signal();
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
    // TODO: 清理所有资源
    // 卸载所有资源，清理所有 record，等待所有请求完成
    Shutdown();
}

void ResourceSystem::Initialize(ResourceRegistry *provider) {
    // TODO: 初始化资源系统
    // 设置 resource_registry
    // 初始化各种数据结构
    resource_registry = provider;
}

bool ResourceSystem::IsInitialized() {
    // TODO: 检查系统是否已初始化
    return resource_registry != nullptr;
}

void ResourceSystem::Shutdown() {
    // TODO: 关闭资源系统
    // 设置 quit 标志
    // 等待所有请求完成
    // 清理所有资源记录
    quit = true;
    WaitRequest();
    // 清理所有 records
}

void ResourceSystem::Update() {
    // TODO: 更新资源系统
    // 处理请求队列中的新请求
    // 更新进行中的请求
    // 处理序列化批次
    // 清理失败的请求
    std::lock_guard<std::mutex> lock(queue_mtx);

    // 处理新请求
    ResourceRequest *request = nullptr;
    while (auto opt = request_queue.pop()) {
        request = *opt;
        to_update_requests.push_back(request);
    }

    // 更新进行中的请求
    for (auto *req : to_update_requests) {
        req->Update();
    }

    // 处理序列化批次
    // 根据 factory 的 AsyncSerdeLoadFactor 决定批次大小
    // 执行序列化操作

    // 清理失败的请求
    ClearFinishRequestsInternal();
}

bool ResourceSystem::WaitRequest() {
    // TODO: 等待所有请求完成
    // 使用 counter 等待所有异步操作完成
    counter.wait();
    return true;
}

void ResourceSystem::Quit() {
    // TODO: 设置退出标志
    quit = true;
}

ResourceRecord *ResourceSystem::RegisterResource(const vstd::Guid &guid) {
    // TODO: 注册资源记录
    // 检查是否已存在，如果不存在则创建新的 ResourceRecord
    // 添加到 resource_records map
    resource_records_mtx.lock();
    auto iter = resource_records.find(guid);
    if (iter) {
        auto *result = iter.value();
        resource_records_mtx.unlock();
        return result;
    }

    auto *record = new ResourceRecord{};
    record->header.guid = guid;
    resource_records.emplace(guid, record);
    resource_records_mtx.unlock();
    return record;
}

ResourceRecord *ResourceSystem::LoadResource(const ResourceHandle handle, bool requireInstall) {
    // TODO: 加载资源
    // 如果 handle 是 guid 模式，先注册 record
    // 创建 ResourceRequest，加入请求队列
    // 如果 requireInstall，等待安装完成
    // 返回 ResourceRecord 指针
    auto guid = handle.get_guid();
    auto *record = RegisterResource(guid);

    // 创建请求并加入队列
    // 这里需要创建 ResourceRequest 的具体实现类
    // request_queue.push(request);

    return record;
}

ResourceHandle ResourceSystem::EnqueueResource(vstd::Guid guid, rbc::TypeInfo type, rbc::IResource *resource, bool requireInstall, luisa::vector<ResourceHandle> dependencies, EResourceLoadingStatus status) {
    // TODO: 将资源加入队列
    // 注册或获取 ResourceRecord
    // 设置 header、resource、状态等
    // 创建 ResourceHandle 并返回
    auto *record = RegisterResource(guid);
    record->header.type = type;
    record->header.dependencies = std::move(dependencies);
    record->resource = resource;
    record->SetStatus(status);

    if (resource) {
        resource_to_update_mtx.lock();
        // resource_to_record 的 key 是 vstd::Guid，需要从 record->header.guid 获取
        resource_to_record.emplace(record->header.guid, record);
        resource_to_update_mtx.unlock();
    }

    // TODO: 创建 ResourceHandle 并关联 record
    // 需要通过 ResourceHandle 的构造函数或特殊方法来设置 record
    // 当前实现：创建一个 guid 模式的 handle，后续在 load 时会解析为 record
    ResourceHandle handle(guid);
    // 注意：这里需要一种方式将 record 关联到 handle
    // 可能需要修改 ResourceHandle 的设计，或者通过其他机制实现
    return handle;
}

void ResourceSystem::UnloadResource(const ResourceHandle handle) {
    // TODO: 卸载资源
    // 通过 guid 查找 ResourceRecord，调用 UnloadResourceInternal
    auto guid = handle.get_guid();
    auto *record = FindResourceRecord(guid);
    if (record) {
        UnloadResourceInternal(record);
    }
}

void ResourceSystem::FlushResource(const ResourceHandle handle) {
    // TODO: 刷新资源
    // 等待资源加载完成，确保资源已安装
    auto guid = handle.get_guid();
    auto *record = FindResourceRecord(guid);
    if (!record) return;

    // 等待加载完成
    while (record->loading_status.load() < EResourceLoadingStatus::Installed) {
        Update();
        // 可能需要等待 fiber event
    }
}

ResourceFactory *ResourceSystem::FindFactory(rbc::TypeInfo type) const {

    const_cast<luisa::spin_mutex &>(resource_factories_mtx).lock();
    auto iter = resource_factories.find(type);
    auto *result = iter ? iter.value() : nullptr;
    const_cast<luisa::spin_mutex &>(resource_factories_mtx).unlock();
    return result;
}

void ResourceSystem::RegisterFactory(ResourceFactory *factory) {
    // TODO: 注册资源工厂
    // 需要从 factory 获取类型 GUID（可能需要虚函数）
    // 添加到 resource_factories map
    // 这里需要知道如何获取 factory 对应的类型 GUID
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

ResourceRecord *ResourceSystem::GetRecordInternal(void *resource) {
    // TODO: 从 resource 指针查找 ResourceRecord
    // resource_to_record 的 key 是 vstd::Guid，但这里需要从 void* 查找
    // 可能需要通过其他方式获取 resource 的 guid，或者需要修改 map 的设计
    // 当前实现：返回 nullptr，需要后续实现
    return nullptr;
}

void ResourceSystem::UnloadResourceInternal(ResourceRecord *record) {
    // TODO: 卸载资源（内部实现）
    // 设置状态为 Unloading
    // 调用 resource->OnUninstall()
    // 卸载依赖资源
    // 清理资源数据
    // 设置状态为 Unloaded
    if (!record) return;

    record->SetStatus(EResourceLoadingStatus::Unloading);

    if (record->resource) {
        record->resource->OnUninstall();
        resource_to_update_mtx.lock();
        resource_to_record.remove(record->header.guid);
        resource_to_update_mtx.unlock();
    }

    // 卸载依赖
    for (auto &dep : record->header.dependencies) {
        UnloadResource(dep);
    }

    record->SetStatus(EResourceLoadingStatus::Unloaded);
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

void ResourceSystem::ClearFinishRequestsInternal() {
    // TODO: 清理完成的请求（内部方法）
    // 移除已完成的请求（成功或失败）
    // 清理 failed_requests 和 to_update_requests
    faield_requests.clear();
    to_update_requests.clear();
}

ResourceSystem *GetResourceSystem() {
    static ResourceSystem system;
    return &system;
}

// =====================================================

}// namespace rbc