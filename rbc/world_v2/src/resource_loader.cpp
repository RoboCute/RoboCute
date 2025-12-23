#include <rbc_world_v2/resource_loader.h>
#include <rbc_world_v2/type_register.h>
#include <rbc_core/runtime_static.h>
#include <rbc_core/shared_atomic_mutex.h>
#include <rbc_core/containers/rbc_concurrent_queue.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_core/binary_file_writer.h>

namespace rbc::world {
struct LoadingResource {
    RC<Resource> res;
    coro::coroutine loading_coro;
    luisa::vector<RCWeak<Resource>> depended_res;
};
static thread_local LoadingResource *_current_loading_res{};
struct ResourceLoader {
    luisa::filesystem::path _meta_path;

    struct ResourceHandle {
        RCWeak<Resource> res;
        luisa::BinaryBlob json;
        luisa::spin_mutex mtx;
        ResourceHandle() = default;
        ResourceHandle(ResourceHandle &&rhs) noexcept : res(std::move(rhs.res)) {}
    };

    rbc::shared_atomic_mutex _resmap_mtx;
    vstd::HashMap<vstd::Guid, ResourceHandle> resource_types;
    vstd::LockFreeArrayQueue<LoadingResource> loading_queue;
    std::thread _loading_thd;
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
        while (_enabled) {
            while (auto res = loading_queue.pop()) {
                std::this_thread::yield();// do not make loading thread too busy
                while (!res->depended_res.empty()) {
                    auto dep = res->depended_res.back().lock();
                    if (!dep || dep->_status == EResourceLoadingStatus::Loaded) {
                        res->depended_res.pop_back();
                    } else {
                        break;
                    }
                }
                bool done = false;
                if (res->depended_res.empty()) {
                    _current_loading_res = res.ptr();
                    res->loading_coro.resume();
                    _current_loading_res = nullptr;
                    done = res->loading_coro.done();
                }
                auto asset_mng = AssetsManager::instance();
                if (asset_mng) {
                    asset_mng->wake_load_thread();
                }
                if (!done) {
                    loading_queue.push(std::move(*res));
                } else {
                    res->res->_status = EResourceLoadingStatus::Loaded;
                }
            }
            std::unique_lock lck{_async_mtx};
            auto size = loading_queue.length();
            while (_enabled && loading_queue.length() == 0) {
                size = loading_queue.length();
                _async_cv.wait(lck);
                size = loading_queue.length();
            }
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
    ResourceLoader()
        : _loading_thd([this] { _loading_thread(); }) {
    }
    ~ResourceLoader() {
        _async_mtx.lock();
        _enabled = false;
        _async_mtx.unlock();
        _async_cv.notify_all();
        _loading_thd.join();
    }
    void try_load_resource(RC<Resource> &&res) {
        if (res->_status.exchange(EResourceLoadingStatus::Loading) != EResourceLoadingStatus::Unloaded) {
            return;
        }
        auto ptr = res.get();
        loading_queue.push(std::move(res), ptr->_async_load());
        _async_mtx.lock();
        _async_mtx.unlock();
        _async_cv.notify_one();
    }
    void try_unload_resource(RC<Resource> &&res) {
        coro::coroutine c{[](RCWeak<Resource> &&res) -> coro::coroutine {
            while (true) {
                {
                    auto res_ptr = res.lock();
                    if (!res_ptr) {
                        co_return;
                    }
                    res_ptr->unload();
                    co_return;
                }
                co_await coro::awaitable{};
            }
        }(RCWeak<Resource>{res})};
        auto ptr = res.get();
        loading_queue.push(std::move(res), c);
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
    LUISA_ASSERT(_res_loader);
    _res_loader->_meta_path = meta_path;
    _res_loader->load_all_resources();
}
RC<Resource> load_resource(vstd::Guid const &guid, bool async_load_from_file) {
    auto obj = get_object(guid);

    if (obj) return RC<Resource>{obj};

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
    RC<Resource> res{v->res.lock().get()};
    if (res) {
        lck.unlock();
        if (async_load_from_file) {
            _res_loader->try_load_resource(RC<Resource>{res});
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
    res = static_cast<Resource *>(create_object_with_guid_test_base(type_id, guid, BaseObjectType::Resource));
    if (!res) [[unlikely]] {
        remove_value();
        return {};
    }
    ObjDeSerialize obj_deser{.ser = deser};
    v->res = res;
    res->deserialize_meta(obj_deser);
    lck.unlock();
    if (async_load_from_file)
        _res_loader->try_load_resource(RC<Resource>{res});
    return res;
}
coro::awaitable Resource::await_loading() {
    if (!_current_loading_res) [[unlikely]] {
        LUISA_ERROR("Currently not in loading thread's coroutine.");
    }
    if (_current_loading_res->res.get() == this) [[unlikely]] {
        LUISA_ERROR("Shoud never wait on self.");
    }
    auto st = _status.load(std::memory_order_relaxed);
    if (st == EResourceLoadingStatus::Loaded) {
        return {true};
    }
    _res_loader->try_load_resource(RC<Resource>{this});
    _current_loading_res->depended_res.emplace_back(RCWeak<Resource>{this});
    return {};
}
void Resource::unload() {
    EResourceLoadingStatus old = EResourceLoadingStatus::Loaded;
    if (_status.compare_exchange_strong(
            old,
            EResourceLoadingStatus::Unloaded)) {
        _unload();
        return;
    }
    _res_loader->try_unload_resource(RC<Resource>{this});
}
void Resource::wait_load_finished() const {
    while (_status.load(std::memory_order_relaxed) != EResourceLoadingStatus::Loaded) {
        std::this_thread::yield();
    }
}
}// namespace rbc::world