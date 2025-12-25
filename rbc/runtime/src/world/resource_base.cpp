#include <rbc_world/resource_base.h>
#include <rbc_world/type_register.h>
#include <rbc_core/runtime_static.h>
#include <rbc_core/shared_atomic_mutex.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_core/containers/rbc_concurrent_queue.h>
#include <rbc_world/importers/register_importers.h>

namespace rbc ::world {
void Resource::serialize_meta(ObjSerialize const &obj) const {
    auto type_id = this->type_id();
    obj.ser._store(
        reinterpret_cast<vstd::Guid &>(type_id),
        "__typeid__");
    if (!_path.empty()) {
        obj.ser._store(luisa::to_string(_path), "path");
        obj.ser._store(_file_offset, "file_offset");
    }
}
void Resource::deserialize_meta(ObjDeSerialize const &obj) {
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
struct LoadingResource {
    InstanceID res_inst_id;
    coroutine loading_coro;
};
struct ResourceLoader {
    luisa::filesystem::path _meta_path;

    struct ResourceHandle {
        InstanceID res;
        luisa::BinaryBlob json;
        luisa::spin_mutex mtx;
        ResourceHandle() = default;
        ResourceHandle(ResourceHandle &&rhs) noexcept : res(std::move(rhs.res)) {}
    };

    rbc::shared_atomic_mutex _resmap_mtx;
    vstd::HashMap<vstd::Guid, ResourceHandle> resource_types;
    rbc::ConcurrentQueue<LoadingResource> loading_queue;
    luisa::vector<std::thread> _loading_thds;
    std::atomic_bool _enabled = true;
    std::condition_variable _async_cv;
    std::mutex _async_mtx;
    void register_resource(Resource *res) {
        JsonSerializer js;
        res->serialize_meta(world::ObjSerialize{
            js});
        _resmap_mtx.lock();
        auto &v = resource_types.force_emplace(res->guid()).value();
        _resmap_mtx.unlock();
        v.json = js.write_to();
        LUISA_ASSERT(v.json.size() > 0);

        BinaryFileWriter file_writer(luisa::to_string(_meta_path / (res->guid().to_string() + ".rbcmt")));
        file_writer.write(v.json);
    }
    void _loading_thread() {
        auto execute = [&](LoadingResource &res) {
            auto self_ptr_base = get_object_ref(res.res_inst_id);
            // already disposed
            if (!self_ptr_base || self_ptr_base->base_type() != BaseObjectType::Resource) [[unlikely]] {
                res.loading_coro.destroy();
                return;
            }
            auto ptr = std::move(self_ptr_base).cast_static<Resource>();
            res.loading_coro.resume();
            if (!res.loading_coro.done()) {
                loading_queue.enqueue(std::move(res));
            } else {
                ptr->_status = EResourceLoadingStatus::Loaded;
            }
            auto asset_mng = AssetsManager::instance();
            if (asset_mng) {
                asset_mng->wake_load_thread();
            }
        };
        while (_enabled) {
            while (true) {
                LoadingResource loading_res;
                std::this_thread::yield();// do not make loading thread too busy
                if (!loading_queue.try_dequeue(loading_res))
                    break;
                execute(loading_res);
            }
            std::unique_lock lck{_async_mtx};
            while (_enabled && loading_queue.size_approx() == 0) {
                _async_cv.wait(lck);
            }
        }
        while (true) {
            LoadingResource loading_res;
            std::this_thread::yield();// do not make loading thread too busy
            if (!loading_queue.try_dequeue(loading_res))
                break;
            execute(loading_res);
        }
    }

    void load_all_resources() {
        // iterate all files
        if (std::filesystem::exists(_meta_path)) {
            for (auto &i : std::filesystem::recursive_directory_iterator(_meta_path)) {
                if (!i.is_regular_file()) {
                    continue;
                }
                auto &path = i.path();
                if (path.extension() != ".rbcmt")
                    continue;
                auto file_name = path.filename();
                auto base_path = luisa::to_string(file_name.replace_extension());
                if (auto guid_result = vstd::Guid::TryParseGuid(base_path)) {
                    resource_types.emplace(*guid_result);
                }
            }
        } else {
            std::filesystem::create_directory(_meta_path);
        }
        auto iter = resource_types.begin();
        luisa::spin_mutex _iter_mtx;
        luisa::vector<vstd::Guid> removed_guid;
        luisa::fiber::parallel(resource_types.size(), [&](size_t) {
            ResourceHandle *v;
            vstd::Guid guid;
            _iter_mtx.lock();
            guid = iter->first;
            v = &iter->second;
            iter++;
            _iter_mtx.unlock();
            auto file_name = guid.to_string();
            luisa::BinaryFileStream file_stream(luisa::to_string(_meta_path / (file_name + ".rbcmt")));
            if (!file_stream.valid()) {
                LUISA_WARNING("Read file {} failed.", file_name + ".rbcmt");
                _resmap_mtx.lock();
                removed_guid.emplace_back(guid);
                _resmap_mtx.unlock();
                return;
            }
            v->json = luisa::BinaryBlob{
                (std::byte *)vengine_malloc(file_stream.length()),
                file_stream.length(),
                +[](void *ptr) { vengine_free(ptr); }};
            file_stream.read(
                {reinterpret_cast<std::byte *>(v->json.data()),
                 v->json.size()});
        });
        for (auto &i : removed_guid) {
            resource_types.remove(i);
        }
    }
    ResourceLoader() {
        auto loading_thread_count = std::clamp<uint>(std::thread::hardware_concurrency() / 4, 1, 4);
        _loading_thds.reserve(loading_thread_count);
        for (auto i : vstd::range(loading_thread_count)) {
            _loading_thds.emplace_back([this] { _loading_thread(); });
        }
    }
    void delete_this() {
        if (!_enabled) return;
        _async_mtx.lock();
        _enabled = false;
        _async_mtx.unlock();
        _async_cv.notify_all();
        for (auto &i : _loading_thds) {
            i.join();
        }
    }
    ~ResourceLoader() {
        LUISA_ASSERT(!_enabled, "Resource loader not disabled.");
    }
    void try_load_resource(Resource *res) {
        if (res->_status.exchange(EResourceLoadingStatus::Loading) != EResourceLoadingStatus::Unloaded) {
            return;
        }
        loading_queue.enqueue(LoadingResource{res->instance_id(), res->_async_load()});
        _async_mtx.lock();
        _async_mtx.unlock();
        _async_cv.notify_one();
    }
    void try_unload_resource(Resource *res) {
        coroutine c{[](InstanceID res) -> coroutine {
            while (true) {
                {
                    auto res_ptr = get_object_ref(res);
                    if (!res_ptr || res_ptr->base_type() != BaseObjectType::Resource) {
                        co_return;
                    }
                    static_cast<Resource *>(res_ptr.get())->unload();
                    co_return;
                }
                co_await std::suspend_always{};
            }
        }(res->instance_id())};
        loading_queue.enqueue(LoadingResource{res->instance_id(), std::move(c)});
        _async_mtx.lock();
        _async_mtx.unlock();
        _async_cv.notify_one();
    }
};

RuntimeStatic<ResourceLoader> _res_loader;
void register_resource(Resource *res) {
    _res_loader->register_resource(res);
}
void init_resource_loader(luisa::filesystem::path const &meta_path) {
    // Register all built-in resource importers
    register_builtin_importers();

    LUISA_ASSERT(_res_loader);
    _res_loader->_meta_path = meta_path;
    _res_loader->load_all_resources();
}
RC<Resource> load_resource(vstd::Guid const &guid, bool async_load_from_file) {
    {
        auto obj = get_object_ref(guid);
        if (obj && obj->base_type() == BaseObjectType::Resource) return std::move(obj).cast_static<Resource>();
    }

    LUISA_DEBUG_ASSERT(_res_loader && !_res_loader->_meta_path.empty());
    ResourceLoader::ResourceHandle *v{};
    {
        std::shared_lock lck{_res_loader->_resmap_mtx};
        auto iter = _res_loader->resource_types.find(guid);
        if (!iter) {
            return {};
        }
        v = &iter.value();
    }
    std::unique_lock lck{v->mtx};
    RC<Resource> res;
    auto res_base = get_object_ref(v->res);
    LUISA_DEBUG_ASSERT(!res_base || res_base->base_type() == BaseObjectType::Resource);
    res = std::move(res_base).cast_static<Resource>();
    if (res) {
        lck.unlock();
        if (async_load_from_file) {
            _res_loader->try_load_resource(res.get());
        }
        return res;
    }

    auto guid_str = guid.to_string();
    auto remove_value = [&]() {
        std::lock_guard lck{_res_loader->_resmap_mtx};
        _res_loader->resource_types.remove(guid);
    };
    JsonDeSerializer deser{luisa::string_view{(char const *)v->json.data(), v->json.size()}};
    if (!deser.valid()) [[unlikely]] {
        remove_value();
        return {};
    }
    vstd::Guid type_id;
    if (!deser._load(type_id, "__typeid__")) [[unlikely]] {
        remove_value();
        return {};
    }
    res = static_cast<Resource *>(_zz_create_object_with_guid_test_base(type_id, guid, BaseObjectType::Resource));
    if (!res) [[unlikely]] {
        remove_value();
        return {};
    }
    ObjDeSerialize obj_deser{.ser = deser};
    v->res = res->instance_id();
    res->deserialize_meta(obj_deser);
    lck.unlock();
    if (async_load_from_file)
        _res_loader->try_load_resource(res.get());
    return res;
}
bool ResourceAwait::await_ready() {
    if (!_inst_id) [[unlikely]]
        return true;
    auto obj = get_object_ref(_inst_id);
    if (!obj || obj->base_type() != BaseObjectType::Resource) [[unlikely]]
        return true;
    return static_cast<Resource *>(obj.get())->loading_status() == EResourceLoadingStatus::Loaded;
}
ResourceAwait Resource::await_loading() {
    auto st = _status.load(std::memory_order_relaxed);
    if (st == EResourceLoadingStatus::Loaded) {
        return {InstanceID::invalid_resource_handle()};
    }
    _res_loader->try_load_resource(this);
    return {instance_id()};
}
void Resource::unload() {
    EResourceLoadingStatus old = EResourceLoadingStatus::Loaded;
    if (_status.compare_exchange_strong(
            old,
            EResourceLoadingStatus::Unloaded)) {
        _unload();
        return;
    }
    _res_loader->try_unload_resource(this);
}
void dispose_resource_loader() {
    if (_res_loader)
        _res_loader->delete_this();
}
}// namespace rbc::world