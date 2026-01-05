#include <rbc_world/resource_base.h>
#include <rbc_world/type_register.h>
#include <rbc_core/runtime_static.h>
#include <rbc_core/shared_atomic_mutex.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_core/containers/rbc_concurrent_queue.h>
#include <rbc_core/sqlite_cpp.h>
#include <rbc_world/importers/register_importers.h>

namespace rbc ::world {
void Resource::set_path(luisa::filesystem::path const &path) {
    if (!_path.empty()) [[unlikely]] {
        LUISA_ERROR("Resource path already setted.");
    }
    _path = path;
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
struct ResourceLoader : RBCStruct {
    luisa::filesystem::path _meta_path;
    luisa::filesystem::path _binary_path;
    SqliteCpp _meta_db;

    struct ResourceHandle {
        InstanceID res;
        luisa::string json;
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
    void init_db() {
        if (!luisa::filesystem::is_directory(_meta_path)) {
            luisa::filesystem::create_directories(_meta_path);
        }
        vstd::reset(_meta_db, luisa::to_string(_meta_path / ".meta_db"));
        if (!_meta_db.check_table_exists("RBC_FILE_META"sv)) {
            luisa::fixed_vector<SqliteCpp::ColumnDesc, 2> columns;
            columns.emplace_back(
                SqliteCpp::ColumnDesc{
                    .name = "GUID",
                    .type = SqliteCpp::DataType::String,
                    .primary_key = true,
                    .unique = true,
                    .not_null = true});
            columns.emplace_back(
                SqliteCpp::ColumnDesc{
                    .name = "META",
                    .type = SqliteCpp::DataType::String,
                    .not_null = true});
            _meta_db.create_table(
                "RBC_FILE_META"sv,
                columns);
        }
    }
    void register_resource(Resource *res) {
        JsonSerializer js;
        rbc::ArchiveWriteJson adapter(js);
        res->serialize_meta(world::ObjSerialize{adapter});
        _resmap_mtx.lock();
        auto &v = resource_types.force_emplace(res->guid()).value();
        _resmap_mtx.unlock();
        js.write_to(v.json);
        LUISA_ASSERT(v.json.size() > 0);
        auto columns = {luisa::string("GUID"), luisa::string("META")};
        auto values = {
            SqliteCpp::ValueVariant{res->guid().to_base64()},
            SqliteCpp::ValueVariant{v.json}};
        LUISA_ASSERT(_meta_db.insert_values("RBC_FILE_META"sv, columns, values, true).is_success(), "Write SQLITE failed.");
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
    luisa::string to_binary(vstd::Guid guid) {
        luisa::string result;
        auto file_name = guid.to_base64();
        _meta_db.read_columns_with("RBC_FILE_META"sv, [&](SqliteCpp::ColumnValue &&v) { result = std::move(v.value); }, "META"sv, "GUID"sv, file_name);
        return result;
    }
    void load_meta_files(
        luisa::span<vstd::Guid const> meta_files_guid) {
        std::lock_guard lck{_resmap_mtx};
        luisa::spin_mutex remove_mtx;
        luisa::vector<vstd::Guid> removed_guid;
        auto func = [&](uint32_t i) {
            auto &&guid = meta_files_guid[i];
            auto iter = resource_types.try_emplace(guid);
            if (!iter.second) {
                return;
            }
            auto &v = iter.first.value();
            auto json = to_binary(guid);
            if (json.empty()) {
                std::lock_guard lck{remove_mtx};
                LUISA_WARNING("Read file meta {} failed.", guid.to_string());
                removed_guid.emplace_back(guid);
                return;
            }
            v.json = std::move(json);
        };
        luisa::fiber::parallel(meta_files_guid.size(), func, 32);
        for (auto &i : removed_guid) {
            resource_types.remove(i);
        }
    }

    void load_all_resources() {
        std::lock_guard lck{_resmap_mtx};
        _meta_db.read_columns_with("RBC_FILE_META"sv, [&](SqliteCpp::ColumnValue &&v) {
            auto parse_result = vstd::Guid::TryParseGuid(v.value);
            if (!parse_result) [[unlikely]] {
                LUISA_ERROR("Database is broken. please re-generate from project.");
            }
            resource_types.emplace(*parse_result);
        },
                                   "GUID"sv);
        auto iter = resource_types.begin();
        luisa::spin_mutex _iter_mtx;
        luisa::spin_mutex remove_mtx;
        luisa::vector<vstd::Guid> removed_guid;
        luisa::fiber::parallel(resource_types.size(), [&](size_t) {
            ResourceHandle *v;
            vstd::Guid guid;
            _iter_mtx.lock();
            guid = iter->first;
            v = &iter->second;
            iter++;
            _iter_mtx.unlock();
            auto json = to_binary(guid);
            if (json.empty()) {
                std::lock_guard lck{remove_mtx};
                LUISA_WARNING("Read file meta {} failed.", guid.to_string());
                removed_guid.emplace_back(guid);
                return;
            }
            v->json = std::move(json);
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
    void dispose() {
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
        dispose();
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
                    static_cast<Resource *>(res_ptr.get())->delete_this();
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

ResourceLoader *_res_loader{};
void register_resource(Resource *res) {
    _res_loader->register_resource(res);
}
void load_all_resources_from_meta() {
    LUISA_ASSERT(_res_loader);
    _res_loader->load_all_resources();
}
void load_meta_file(luisa::span<vstd::Guid const> meta_files_guid) {
    LUISA_ASSERT(_res_loader);
    _res_loader->load_meta_files(meta_files_guid);
}
void init_resource_loader(luisa::filesystem::path const &meta_path, luisa::filesystem::path const &binary_path) {
    // Register all built-in resource importers
    register_builtin_importers();
    _res_loader = new ResourceLoader{};
    _res_loader->_meta_path = meta_path;
    _res_loader->_binary_path = binary_path;
    _res_loader->init_db();
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
    rbc::ArchiveReadJson adapter(deser);
    vstd::Guid type_id;
    if (!adapter.value(type_id, "__typeid__")) [[unlikely]] {
        remove_value();
        return {};
    }
    res = static_cast<Resource *>(_zz_create_object_with_guid_test_base(type_id, guid, BaseObjectType::Resource));
    if (!res) [[unlikely]] {
        remove_value();
        return {};
    }
    ObjDeSerialize obj_deser{adapter};
    v->res = res->instance_id();
    res->set_path(guid_str + ".rbcb");
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
void dispose_resource_loader() {
    if (!_res_loader) return;
    _res_loader->dispose();
    delete _res_loader;
    _res_loader = nullptr;
}
void Resource::serialize_meta(ObjSerialize const &obj) const {
    auto type_id = this->type_id();
    obj.ar.value(reinterpret_cast<vstd::Guid &>(type_id), "__typeid__");
}
void Resource::deserialize_meta(ObjDeSerialize const &obj) {
    if (!_path.empty()) {
        _path = _res_loader->_binary_path / _path;
    }
}
}// namespace rbc::world