#include <rbc_world_v2/resource_loader.h>
#include <rbc_world_v2/type_register.h>
#include <rbc_core/shared_atomic_mutex.h>

namespace rbc::world {
struct ResourceLoader {
    luisa::filesystem::path _root_path;
    luisa::filesystem::path _meta_path;
    struct ResourceHandle {
        RCWeak<Resource> res;
        rbc::shared_atomic_mutex mtx;
        ResourceHandle() {}
        ResourceHandle(ResourceHandle &&rhs) : res(std::move(rhs.res)) {
        }
    };
    rbc::shared_atomic_mutex _resmap_mtx;
    vstd::HashMap<vstd::Guid, ResourceHandle> resource_types;
    ResourceLoader() {
    }
    void load_all_resources() {
        // iterate all files
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
    }
    ~ResourceLoader() {
    }
};
RuntimeStatic<ResourceLoader> _res_loader;
void init_resource_loader(luisa::filesystem::path const &root_path, luisa::filesystem::path const &meta_path) {
    LUISA_ASSERT(_res_loader && _res_loader->_root_path.empty());
    _res_loader->_root_path = root_path;
    _res_loader->_meta_path = root_path / meta_path;
    _res_loader->load_all_resources();
}
RC<Resource> load_resource(vstd::Guid const &guid, bool async_load_from_file) {
    auto obj = get_object(guid);
    if (obj) return obj;
    LUISA_DEBUG_ASSERT(_res_loader && !_res_loader->_root_path.empty());
    std::shared_lock lck{_res_loader->_resmap_mtx};
    auto iter = _res_loader->resource_types.find(guid);
    if (!iter) {
        return {};
    }
    auto &v = iter.value();
    RC<Resource> res = v.res.lock();
    if (res) {
        if (async_load_from_file) {
            if (!res->empty())
                res->async_load_from_file();
        }
        return res;
    }
    auto guid_str = guid.to_string();
    luisa::BinaryFileStream file_stream(luisa::to_string(_res_loader->_meta_path / (guid_str + ".rbcmt")));
    auto remove_value = [&]() {
        lck.unlock();
        std::lock_guard lck{_res_loader->_resmap_mtx};
        _res_loader->resource_types.remove(guid);
    };
    if (!file_stream) [[unlikely]] {
        remove_value();
        return {};
    }
    luisa::vector<std::byte> vec;
    vec.push_back_uninitialized(file_stream.length());
    file_stream.read(vec);
    JsonDeSerializer deser{luisa::string_view{(char const *)vec.data(), vec.size()}};
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
    v.res = res;
    res->deserialize_meta(obj_deser);
    if (async_load_from_file) res->async_load_from_file();
    return res;
}
}// namespace rbc::world