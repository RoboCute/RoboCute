#pragma once
#include <rbc_world_v2/resource_base.h>
namespace rbc {
struct DummyJsonResource : world::ResourceBaseImpl<DummyJsonResource> {
    using BaseType = ResourceBaseImpl<DummyJsonResource>;
    // make internal allocate and deallcate function friend
    DECLARE_WORLD_OBJECT_FRIEND(DummyJsonResource)
    void serialize_meta(world::ObjSerialize const &ser) const override;
    void deserialize_meta(world::ObjDeSerialize const &ser) override;
    void dispose() override;
    rbc::coro::coroutine _async_load() override;
    luisa::string_view value() const;
    void create_empty(std::initializer_list<RC<DummyJsonResource>> depended, luisa::string_view name);
protected:
    bool unsafe_save_to_path() const override;
    void _unload() override;

    // depended resources
    luisa::vector<RC<DummyJsonResource>> _depended;
    luisa::vector<char> _value;
};
};// namespace rbc
RBC_RTTI(rbc::DummyJsonResource)