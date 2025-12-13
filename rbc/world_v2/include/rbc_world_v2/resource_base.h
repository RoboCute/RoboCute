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
    Resource() = default;
    ~Resource() = default;
public:
    [[nodiscard]] luisa::filesystem::path const &path() const {
        return _path;
    }
    [[nodiscard]] uint64_t file_offset() const { return _file_offset; }
    virtual bool loaded() const = 0;
    virtual bool async_load_from_file() = 0;
    virtual void unload() = 0;
    virtual void wait_load() const = 0;
    RBC_WORLD_API void set_path(
        luisa::filesystem::path const &path,
        uint64_t const &file_offset);
    RBC_WORLD_API virtual void serialize(ObjSerialize const&obj) const;
    RBC_WORLD_API virtual void deserialize(ObjDeSerialize const&obj);
    // RBC_WORLD_API virtual void (ObjDeSerialize const&obj);
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
    virtual void serialize(ObjSerialize const&obj) const override {
        Resource::serialize(obj);
    }
    virtual void deserialize(ObjDeSerialize const&obj) override {
        Resource::deserialize(obj);
    }
    ResourceBaseImpl() = default;
    ~ResourceBaseImpl() = default;
};
}// namespace rbc::world