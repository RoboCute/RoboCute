#pragma once
#include <rbc_world/component.h>
#include <rbc_world/resource_base.h>
#include <rbc_core/base.h>
#include <luisa/vstl/common.h>

namespace rbc::world {
struct RBC_RUNTIME_API DataComponent final : ComponentDerive<DataComponent> {
    DECLARE_WORLD_OBJECT_FRIEND(DataComponent)
private:
    DataComponent();
    ~DataComponent();

public:
    vstd::HashMap<vstd::Guid, RC<Resource>> _resources;
    vstd::HashMap<
        luisa::string,
        BasicDeserDataType>
        _infos;

    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;
};
}// namespace rbc::world

RBC_RTTI(rbc::world::DataComponent);
