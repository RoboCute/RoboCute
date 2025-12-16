#pragma once
#include <rbc_core/shared_atomic_mutex.h>
#include <rbc_graphics/object_types.h>
#include <rbc_world_v2/resource_base.h>
namespace rbc::world {
struct RBC_WORLD_API Material final : ResourceBaseImpl<Material> {
    DECLARE_WORLD_OBJECT_FRIEND(Material)
private:
    using BaseType = ResourceBaseImpl<Material>;
    mutable rbc::shared_atomic_mutex _async_mtx;
    luisa::fiber::event _event;
    luisa::vector<RC<Resource>> _depended_resources;

    Material();
    ~Material();
    MaterialStub::MatDataType _mat_data;
    MatCode _mat_code;
    bool _loaded : 1 {false};
    bool _dirty : 1 {true};
public:
    auto &mat_code() const { return _mat_code; }
    auto &mat_data() const { return _mat_data; }
    // prepare host data and emplace
    bool init_device_resource() override;
    luisa::BinaryBlob write_content_to();
    void load_from_json(luisa::string_view json_vec);
    bool loaded() const override;
    void serialize(ObjSerialize const &obj) const override;
    void dispose() override;
    bool async_load_from_file() override;
    void unload() override;
    void wait_load() const override;
protected:
    bool unsafe_save_to_path() const override;
};
};// namespace rbc::world
RBC_RTTI(rbc::world::Material)