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
luisa::filesystem::path Resource::path() const {
    return binary_root_path() / (guid().to_string() + ".rbcb");
}
bool Resource::save_to_path() {
    register_resource_meta(this);
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
            luisa::fixed_vector<SqliteCpp::ColumnDesc, 3> columns;
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
            columns.emplace_back(
                SqliteCpp::ColumnDesc{
                    .name = "TYPEID",
                    .type = SqliteCpp::DataType::String,
                    .not_null = true});
            _meta_db.create_table(
                "RBC_FILE_META"sv,
                columns);
        }
    }
    void register_resource(vstd::Guid resource_guid,
                           luisa::string &&meta_info,
                           std::array<uint64_t, 2> type_id) {
        _resmap_mtx.lock();
        auto &v = resource_types.force_emplace(resource_guid).value();
        _resmap_mtx.unlock();
        LUISA_ASSERT(meta_info.size() > 0);
        auto columns = {luisa::string("GUID"), luisa::string("META"), luisa::string("TYPEID")};
        auto values = {
            SqliteCpp::ValueVariant{resource_guid.to_base64()},
            SqliteCpp::ValueVariant{std::move(meta_info)},
            SqliteCpp::ValueVariant{reinterpret_cast<vstd::Guid &>(type_id).to_base64()}};
        LUISA_ASSERT(_meta_db.insert_values("RBC_FILE_META"sv, columns, values, true).is_success(), "Write SQLITE failed.");
    }
    void register_resource(Resource *res) {
        JsonSerializer js;
        rbc::ArchiveWriteJson adapter(js);
        res->serialize_meta(world::ObjSerialize{adapter});
        _resmap_mtx.lock();
        auto &v = resource_types.force_emplace(res->guid()).value();
        _resmap_mtx.unlock();
        luisa::string meta_info;
        js.write_to(meta_info);
        LUISA_ASSERT(meta_info.size() > 0);
        auto columns = {luisa::string("GUID"), luisa::string("META"), luisa::string("TYPEID")};
        auto type_id = res->type_id();
        auto values = {
            SqliteCpp::ValueVariant{res->guid().to_base64()},
            SqliteCpp::ValueVariant{std::move(meta_info)},
            SqliteCpp::ValueVariant{reinterpret_cast<vstd::Guid &>(type_id).to_base64()}};
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
    // meta, type-id
    std::pair<luisa::string, vstd::Guid> to_binary(vstd::Guid guid) {
        luisa::string result;
        vstd::Guid type_id;
        type_id.reset();
        auto file_name = guid.to_base64();
        _meta_db.read_columns_with("RBC_FILE_META"sv, [&](SqliteCpp::ColumnValue &&v) { 
            if(v.name == "META"sv) {
                result = std::move(v.value);
            } else {
                auto id = vstd::Guid::TryParseGuid(v.value);
                if(!id) [[unlikely]] {
                    LUISA_ERROR("Database is broken. please re-generate from project.");
                }
                type_id = *id;
            } }, "META, TYPEID"sv, "GUID"sv, file_name);
        return {result, type_id};
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
void register_resource_meta(Resource *res) {
    _res_loader->register_resource(res);
}
void register_resource_meta(
    vstd::Guid resource_guid,
    luisa::string &&meta_info,
    std::array<uint64_t, 2> type_id) {
    _res_loader->register_resource(resource_guid, std::move(meta_info), type_id);
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
        auto iter = _res_loader->resource_types.emplace(guid);
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
    auto obj_meta = _res_loader->to_binary(guid);
    if (!obj_meta.second) return {};
    auto remove_value = [&]() {
        std::lock_guard lck{_res_loader->_resmap_mtx};
        _res_loader->resource_types.remove(guid);
    };
    if (obj_meta.first.empty()) {
        obj_meta.first = "{}";
    }
    JsonDeSerializer deser{obj_meta.first};
    if (!deser.valid()) [[unlikely]] {
        remove_value();
        return {};
    }
    rbc::ArchiveReadJson adapter(deser);
    res = static_cast<Resource *>(_zz_create_object_with_guid_test_base(obj_meta.second, guid, BaseObjectType::Resource));
    if (!res) [[unlikely]] {
        remove_value();
        return {};
    }
    ObjDeSerialize obj_deser{adapter};
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
void dispose_resource_loader() {
    if (!_res_loader) return;
    _res_loader->dispose();
    delete _res_loader;
    _res_loader = nullptr;
}
luisa::filesystem::path const &Resource::meta_root_path() {
    return _res_loader->_meta_path;
}
luisa::filesystem::path const &Resource::binary_root_path() {
    return _res_loader->_binary_path;
}
}// namespace rbc::world