#pragma once
#include <rbc_graphics/object_types.h>
#include <rbc_world_v2/resource_base.h>
namespace rbc::world {
struct RBC_WORLD_API Material final : ResourceBaseImpl<Material> {
    DECLARE_WORLD_OBJECT_FRIEND(Material)
private:
    using BaseType = ResourceBaseImpl<Material>;
    luisa::fiber::event _event;
    luisa::vector<RC<Resource>> _depended_resources;

    Material() = default;
    ~Material();
    MaterialStub::MatDataType _mat_data;
    MatCode _mat_code;
    luisa::spin_mutex _mtx;
    bool _loaded{false};
public:
    auto &mat_code() const { return _mat_code; }
    void prepare_material();
    void update_material();
    luisa::BinaryBlob write_content_to();
    void load_from_json(luisa::string_view json_vec);
    bool loaded() const override;
    void rbc_objser(rbc::JsonSerializer &ser_obj) const override;
    void rbc_objdeser(rbc::JsonDeSerializer &obj) override;
    void dispose() override;
    bool async_load_from_file() override;
    void unload() override;
    void wait_load() const override;
};
};// namespace rbc::world
RBC_RTTI(rbc::world::Material)