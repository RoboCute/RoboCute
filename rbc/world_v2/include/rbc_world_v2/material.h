#pragma once
#include <rbc_graphics/object_types.h>
#include <rbc_world_v2/resource_base.h>
namespace rbc::world {
struct Material : ResourceBaseImpl<Material> {
    friend struct MaterialImpl;
private:
    Material() = default;
    ~Material() = default;
    MaterialStub::MatDataType _mat_data;
    MatCode _mat_code;
    luisa::spin_mutex _mtx;
    bool _loaded{false};
public:
    auto &mat_code() const { return _mat_code; }
    virtual void prepare_material() = 0;
    virtual void update_material() = 0;
    virtual luisa::BinaryBlob write_content_to() = 0;
    virtual void load_from_json(luisa::string_view json_vec) = 0;
};
};// namespace rbc::world
RBC_RTTI(rbc::world::Material)