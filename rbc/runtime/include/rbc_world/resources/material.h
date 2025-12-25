#pragma once
#include <rbc_core/shared_atomic_mutex.h>
#include <rbc_graphics/object_types.h>
#include <rbc_world/resource_base.h>
namespace rbc::world {
struct RBC_RUNTIME_API MaterialResource final : ResourceBaseImpl<MaterialResource> {
    DECLARE_WORLD_OBJECT_FRIEND(MaterialResource)
private:
    using BaseType = ResourceBaseImpl<MaterialResource>;
    mutable rbc::shared_atomic_mutex _async_mtx;
    luisa::vector<RC<Resource>> _depended_resources;

    MaterialResource();
    ~MaterialResource();
    MaterialStub::MatDataType _mat_data;
    MatCode _mat_code;
    bool _loaded : 1 {false};
    bool _dirty : 1 {true};
public:
    static MatCode default_mat_code();
    auto &mat_code() const { return _mat_code; }
    auto &mat_data() const { return _mat_data; }
    // prepare host data and emplace
    bool init_device_resource();
    luisa::BinaryBlob write_content_to();
    void serialize_meta(ObjSerialize const &obj) const override;
    
    rbc::coroutine _async_load() override;
    void load_from_json(luisa::string_view json_vec);
protected:
    void _load_from_json(luisa::string_view json_vec, bool set_to_loaded);
    bool _async_load_from_file();
    void _unload() override;
    bool unsafe_save_to_path() const override;
};
};// namespace rbc::world
RBC_RTTI(rbc::world::MaterialResource)