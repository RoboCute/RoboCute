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
    Loaded
};
struct ResourceAwait : rbc::i_awaitable<ResourceAwait> {
    friend struct Resource;
    RBC_RUNTIME_API bool await_ready();
private:
    InstanceID _inst_id;
    ResourceAwait(InstanceID inst_id) : _inst_id(inst_id) {}
};
struct Resource : BaseObject {
    friend struct ResourceLoader;
    friend struct IResourceImporter;
    template<typename Derive>
    friend struct ResourceBaseImpl;
protected:
    std::atomic<EResourceLoadingStatus> _status{EResourceLoadingStatus::Unloaded};
    RBC_RUNTIME_API Resource();
    RBC_RUNTIME_API ~Resource();
    virtual rbc::coroutine _async_load() = 0;

public:
    [[nodiscard]] RBC_RUNTIME_API luisa::filesystem::path path() const;

    ///////// Function call must be atomic
    EResourceLoadingStatus loading_status() const { return _status.load(std::memory_order_relaxed); }
    void unsafe_set_loaded() { _status = EResourceLoadingStatus::Loaded; }// It is user's responsibility to make the state valid
    bool loaded() const { return loading_status() == EResourceLoadingStatus::Loaded; }

    // await until the loading logic finished in both host-side and device-side
    RBC_RUNTIME_API ResourceAwait await_loading();
    // save host_data to Resource::_path
    RBC_RUNTIME_API bool save_to_path();
    // serialize_meta meta information
    RBC_RUNTIME_API void serialize_meta(ObjSerialize const &obj) const override;
    // deserialize_meta meta information
    RBC_RUNTIME_API void deserialize_meta(ObjDeSerialize const &obj) override;
    // RBC_RUNTIME_API virtual void (ObjDeSerialize const&obj);
    RBC_RUNTIME_API static luisa::filesystem::path const &meta_root_path();
    RBC_RUNTIME_API static luisa::filesystem::path const &binary_root_path();
    /**
     * @brief Decode resource from file using registered importers
     * @param path Path to the resource file
     * @return true if decode succeeded, false otherwise
     */
    RBC_RUNTIME_API bool decode(luisa::filesystem::path const &path);
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
    [[nodiscard]] std::array<uint64_t, 2> type_id() const override {
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
RBC_RUNTIME_API RC<Resource> load_resource(vstd::Guid const &guid, bool async_load_from_file = true);
RBC_RUNTIME_API void register_resource(Resource *res);
// No project or origin-files, load all meta-files from meta dir
RBC_RUNTIME_API void load_all_resources_from_meta();
RBC_RUNTIME_API void load_meta_file(luisa::span<vstd::Guid const> meta_files_guid);

}// namespace rbc::world