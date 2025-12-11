#pragma once
#include <rbc_world_v2/base_object.h>

namespace rbc ::world {
struct ResourceBase {
protected:
    luisa::filesystem::path _path;
    uint64_t _file_offset{};
    void _rbc_objser(JsonSerializer &ser) const;
    void _rbc_objdeser(JsonDeSerializer &ser);
    ResourceBase() = default;
    ~ResourceBase() = default;
public:
    [[nodiscard]] luisa::filesystem::path const &path() const {
        return _path;
    }
    [[nodiscard]] uint64_t file_offset() const { return _file_offset; }
};
template<typename Derive>
struct ResourceBaseImpl : BaseObjectDerive<Derive, BaseObjectType::Resource>, ResourceBase {
protected:
    virtual void rbc_objser(JsonSerializer &ser) const {
        return ResourceBaseImpl::_rbc_objser(ser);
    }
    virtual void rbc_objdeser(JsonDeSerializer &ser) {
        return ResourceBaseImpl::_rbc_objdeser(ser);
    }
    virtual bool loaded() const = 0;
    virtual bool async_load_from_file() = 0;
    virtual void unload() = 0;
    virtual void wait_load() const = 0;
    ResourceBaseImpl() = default;
    ~ResourceBaseImpl() = default;
};
}// namespace rbc::world