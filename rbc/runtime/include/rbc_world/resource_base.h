#pragma once
#include <rbc_world/base_object.h>
#include <rbc_core/coroutine.h>

namespace rbc ::world {
struct Resource;
struct ResourceLoader;
struct IResourceImporter;
template<typename Derive>
struct ResourceBaseImpl;
enum struct EResourceLoadingStatus : uint8_t {
    Unloaded,
    Loading,
    Loaded,
    Installing,
    Installed
};
struct ResourceAwait : rbc::i_awaitable<ResourceAwait> {
    friend struct Resource;
    RBC_RUNTIME_API bool await_ready();
private:
    RCWeak<Resource> res_ptr;
    ResourceAwait() = default;
    ResourceAwait(RCWeak<Resource> &&res_ptr) : res_ptr(std::move(res_ptr)) {}
};
struct Resource : BaseObject {
    friend struct ResourceLoader;
    friend struct IResourceImporter;
    template<typename Derive>
    friend struct ResourceBaseImpl;
private:
    std::atomic<EResourceLoadingStatus> _status{EResourceLoadingStatus::Unloaded};
protected:
    RBC_RUNTIME_API Resource();
    RBC_RUNTIME_API ~Resource();
    virtual rbc::coroutine _async_load() = 0;
    virtual bool _install() { return true; }

public:
    [[nodiscard]] RBC_RUNTIME_API luisa::filesystem::path path() const;

    ///////// Function call must be atomic
    EResourceLoadingStatus loading_status() const { return _status.load(std::memory_order_relaxed); }
    bool loaded() const { return loading_status() >= EResourceLoadingStatus::Loaded; }
    bool installed() const { return loading_status() >= EResourceLoadingStatus::Installed; }
    RBC_RUNTIME_API EResourceLoadingStatus unsafe_set_loading_status_min(EResourceLoadingStatus dst_status);
    RBC_RUNTIME_API EResourceLoadingStatus unsafe_set_loading_status_max(EResourceLoadingStatus dst_status);
    void unsafe_set_loaded();
    void unsafe_set_installed();
    RBC_RUNTIME_API bool install();
    // await until the loading logic finished in both host-side and device-side
    RBC_RUNTIME_API void load();
    RBC_RUNTIME_API ResourceAwait await_loading();
    RBC_RUNTIME_API void wait_loading();
    // save host_data to Resource::_path
    RBC_RUNTIME_API bool save_to_path();
    // RBC_RUNTIME_API virtual void (ObjDeSerialize const&obj);
    RBC_RUNTIME_API static luisa::filesystem::path const &meta_root_path();
    RBC_RUNTIME_API static luisa::filesystem::path const &binary_root_path();
protected:
    virtual bool unsafe_save_to_path() const = 0;
};

template<typename Derive>
struct ResourceBaseImpl : Resource {
    static constexpr BaseObjectType base_object_type_v = BaseObjectType::Resource;
private:
    [[nodiscard]] const char *type_name() const override {
        return rbc_rtti_detail::is_rtti_type<Derive>::name;
    }
    [[nodiscard]] MD5 type_id() const override {
        return rbc_rtti_detail::is_rtti_type<Derive>::get_md5();
    }
    [[nodiscard]] BaseObjectType base_type() const override {
        return base_object_type_v;
    }
protected:
    void serialize_meta(ObjSerialize const &obj) const override {
        Resource::serialize_meta(obj);
    }
    void deserialize_meta(ObjDeSerialize const &obj) override {
        Resource::deserialize_meta(obj);
    }
    ResourceBaseImpl() = default;
    ~ResourceBaseImpl() = default;
};
RBC_RUNTIME_API luisa::spin_mutex &get_resource_mutex(vstd::Guid const &guid);
RBC_RUNTIME_API bool resource_exists(vstd::Guid const &guid);
RBC_RUNTIME_API RC<Resource> load_resource(vstd::Guid const &guid, bool async_load_from_file = true);
template<typename T>
    requires std::is_base_of_v<Resource, T>
RC<T> load_resource(vstd::Guid const &guid, bool async_load_from_file = true) {
    auto res = load_resource(guid, async_load_from_file);
    if (res) {
        LUISA_ASSERT(res->type_id() == TypeInfo::get<T>().md5());
        return std::move(res).cast_static<T>();
    } else {
        return {};
    }
}
RBC_RUNTIME_API void register_resource_meta(
    vstd::Guid resource_guid,
    luisa::string &&meta_info,
    MD5 type_id);
RBC_RUNTIME_API void register_resource_meta(Resource *res);
}// namespace rbc::world