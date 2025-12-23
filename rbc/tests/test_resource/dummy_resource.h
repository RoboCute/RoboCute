#pragma once
#include <rbc_world_v2/resource_base.h>
namespace rbc {
struct DummyResource : world::ResourceBaseImpl<DummyResource> {
    using BaseType = ResourceBaseImpl<DummyResource>;
    // make internal allocate and deallcate function friend
    DECLARE_WORLD_OBJECT_FRIEND(DummyResource)
    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;
    void dispose() override;
    rbc::coro::coroutine _async_load() override;
    luisa::string_view value() const;
    void create_empty(std::initializer_list<RC<DummyResource>> depended, luisa::string_view name);
protected:
    bool unsafe_save_to_path() const override;
    void _unload() override;

    // depended resources
    luisa::vector<RC<DummyResource>> _depended;
    luisa::vector<char> _value;
};
};// namespace rbc
RBC_RTTI(rbc::DummyResource)