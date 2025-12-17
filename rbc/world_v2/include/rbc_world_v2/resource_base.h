#pragma once
#include <rbc_world_v2/base_object.h>

namespace rbc ::world {
struct Resource;
template<typename Derive>
struct ResourceBaseImpl;
struct Resource : BaseObject {
    template<typename Derive>
    friend struct ResourceBaseImpl;
protected:
    luisa::filesystem::path _path;
    uint64_t _file_offset{};
    Resource();
    ~Resource();
public:
    [[nodiscard]] luisa::filesystem::path const &path() const {
        return _path;
    }
    [[nodiscard]] uint64_t file_offset() const { return _file_offset; }
    ///////// Function call must be atomic
    // return true if this resource is never created, loaded or updated
    virtual bool empty() const = 0;
    // return true if this resource is executed or finished by loading-thread
    virtual bool load_executed() const = 0;
    // return true if this resource is finished by loading-thread
    virtual bool load_finished() const = 0;
    // require this resource load from Resource::_path, return false if the resource is non-empty, loading, loaded or destination file is invalid.
    virtual bool async_load_from_file() = 0;
    // init device side resource, return true if it's non-empty but not loaded.
    virtual bool init_device_resource() = 0;
    // unload this resource and make it as empty
    virtual void unload() = 0;
    // wait until the loading logic finished in host-side
    virtual void wait_load_executed() const = 0;
    // wait until the loading logic finished in both host-side and device-side
    virtual void wait_load_finished() const = 0;
    // save host_data to Resource::_path
    RBC_WORLD_API bool save_to_path();
    // set Resource::_path, valid only if this resource is empty
    RBC_WORLD_API void set_path(
        luisa::filesystem::path const &path,
        uint64_t const &file_offset);
    // serialize meta information
    RBC_WORLD_API virtual void serialize(ObjSerialize const &obj) const;
    // deserialize meta information
    RBC_WORLD_API virtual void deserialize(ObjDeSerialize const &obj);
    // RBC_WORLD_API virtual void (ObjDeSerialize const&obj);
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
    virtual void serialize(ObjSerialize const &obj) const override {
        Resource::serialize(obj);
    }
    virtual void deserialize(ObjDeSerialize const &obj) override {
        Resource::deserialize(obj);
    }
    ResourceBaseImpl() = default;
    ~ResourceBaseImpl() = default;
};
}// namespace rbc::world