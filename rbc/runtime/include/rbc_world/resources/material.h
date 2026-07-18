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
    std::atomic_uint64_t _installed_shader_feature_mask{};
    std::atomic_uint64_t _reinstall_generation{};
    std::atomic_bool _gpu_material_installed{};
    std::atomic_bool _reinstall_queued{};
    bool _loaded : 1 {false};
    bool _dirty : 1 {true};
    void _write_content_to(JsonSerializer &json_ser);
    void _publish_shader_features(uint64_t mask) const;
    void _enqueue_reinstall();
    void _run_queued_reinstall();
public:
    static MatCode default_mat_code();
    auto &mat_code() const { return _mat_code; }
    auto &mat_data() const { return _mat_data; }
    [[nodiscard]] uint64_t shader_feature_mask() const noexcept;
    // prepare host data and emplace
    luisa::BinaryBlob write_content_to();
    luisa::string write_content_to_str();

    rbc::coroutine _async_load() override;
    void load_from_json(luisa::string_view json_vec);
    static RC<MaterialResource> try_get_resource(MatCode code);
protected:
    bool _install() override;
    void _load_from_json(luisa::string_view json_vec, bool set_to_loaded);
    bool _async_load_from_file();
    bool unsafe_save_to_path() const override;
};
};// namespace rbc::world
RBC_RTTI(rbc::world::MaterialResource)
