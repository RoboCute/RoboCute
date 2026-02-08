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
    vstd::HashMap<vstd::Guid, RC<Resource>> _resources;
    vstd::HashMap<
        luisa::string,
        BasicDeserDataType>
        _infos;

public:
    // Info API
    [[nodiscard]] BasicDeserDataType get_info(luisa::string_view name) const;
    void set_info(luisa::string_view name, BasicDeserDataType const &data);
    [[nodiscard]] bool has_info(luisa::string_view name) const;
    [[nodiscard]] bool remove_info(luisa::string_view name);
    [[nodiscard]] uint64_t info_count() const noexcept;
    void clear_infos() noexcept;

    // Resource API
    [[nodiscard]] RC<Resource> get_resource(vstd::Guid const &guid) const;
    void set_resource(RC<Resource> const &resource);
    [[nodiscard]] bool has_resource(vstd::Guid const &guid) const;
    [[nodiscard]] void remove_resource(vstd::Guid const &guid);
    [[nodiscard]] uint64_t resource_count() const noexcept;
    void clear_resources() noexcept;

    void serialize_meta(ObjSerialize const &obj) const override;
    void deserialize_meta(ObjDeSerialize const &obj) override;
};
}// namespace rbc::world

RBC_RTTI(rbc::world::DataComponent);
