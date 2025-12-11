#pragma once
#include <rbc_core/rc.h>
#include <rbc_world_v2/base_object.h>

namespace rbc ::world {
struct Resource;
template<typename Derive>
struct ResourceBaseImpl;
struct ResourceBase : BaseObject {
    template<typename Derive>
    friend struct ResourceBaseImpl;
    friend struct Resource;
    RBC_RC_IMPL
    inline void rbc_rc_delete() {
        unload();
    }
protected:
    luisa::filesystem::path _path;
    uint64_t _file_offset{};
    ResourceBase() = default;
    ~ResourceBase() = default;
private:
    virtual void _rbc_objser(JsonSerializer &ser) const = 0;
    virtual void _rbc_objdeser(JsonDeSerializer &ser) = 0;
    virtual void _set_path(
        luisa::filesystem::path const &path,
        uint64_t const &file_offset) = 0;
public:
    [[nodiscard]] luisa::filesystem::path const &path() const {
        return _path;
    }
    [[nodiscard]] uint64_t file_offset() const { return _file_offset; }
    virtual bool loaded() const = 0;
    virtual bool async_load_from_file() = 0;
    virtual void unload() = 0;
    virtual void wait_load() const = 0;
};
struct Resource : ResourceBase {
private:
    void _rbc_objser(JsonSerializer &ser) const override;
    void _rbc_objdeser(JsonDeSerializer &ser) override;
    void _set_path(
        luisa::filesystem::path const &path,
        uint64_t const &file_offset) override;
public:
    void set_path(
        luisa::filesystem::path const &path,
        uint64_t const &file_offset) {
        static_cast<ResourceBase *>(this)->_set_path(
            path, file_offset);
    }
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
    virtual void rbc_objser(JsonSerializer &ser) const {
        static_cast<ResourceBase const *>(this)->_rbc_objser(ser);
    }
    virtual void rbc_objdeser(JsonDeSerializer &ser) {
        static_cast<ResourceBase *>(this)->_rbc_objdeser(ser);
    }
    ResourceBaseImpl() = default;
    ~ResourceBaseImpl() = default;
};
}// namespace rbc::world