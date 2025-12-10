#include <rbc_world_v2/mesh.h>
#include "type_register.h"
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/device_assets/device_mesh.h>

namespace rbc::world {
struct MeshImpl : Mesh {
    MeshImpl() {
    }
    void rbc_objser(JsonSerializer &ser) const override {
        if (!_path.empty()) {
            ser._store(luisa::to_string(_path), "path");
            ser._store(_file_offset, "file_offset");
        }
        ser._store(_contained_normal, "contained_normal");
        ser._store(_contained_tangent, "contained_tangent");
        ser._store(_vertex_count, "vertex_count");
        ser._store(_triangle_count, "triangle_count");
        ser._store(_uv_count, "uv_count");
        ser.start_array();
        for (auto &i : _submesh_offsets) {
            ser._store(i);
        }
        ser.add_last_scope_to_object("submesh_offsets");
    }
    void rbc_objdeser(JsonDeSerializer &ser) override {
        luisa::string path_str;
        if (ser._load(path_str, "path")) {
            _path = path_str;
        }
        ser._load(_file_offset, "file_offset");
#define RBC_MESH_LOAD(m)        \
    {                           \
        decltype(_##m) m;       \
        if (ser._load(m, #m)) { \
            _##m = m;           \
        }                       \
    }
        RBC_MESH_LOAD(file_offset)
        RBC_MESH_LOAD(contained_normal)
        RBC_MESH_LOAD(contained_tangent)
        RBC_MESH_LOAD(vertex_count)
        RBC_MESH_LOAD(triangle_count)
        RBC_MESH_LOAD(uv_count)
#undef RBC_MESH_LOAD
        uint64_t size;
        if (ser.start_array(size, "submesh_offsets")) {
            _submesh_offsets.reserve(size);
            for (auto i : vstd::range(size)) {
                uint v;
                if (ser._load(v)) {
                    _submesh_offsets.push_back(v);
                }
            }
        }
    }
    void dispose() override;
    void wait_load() const override {
        if (_device_mesh)
            _device_mesh->wait_finished();
    }
    luisa::vector<std::byte> *host_data() override {
        if (_device_mesh)
            return &_device_mesh->host_data_ref();
        else
            return nullptr;
    }
    uint64_t desire_size_bytes() override {
        return DeviceMesh::get_mesh_size(_vertex_count, _contained_normal, _contained_tangent, _uv_count, _triangle_count);
    }
    void create_empty(
        luisa::filesystem::path &&path,
        luisa::span<uint const> submesh_offsets,
        uint64_t file_offset,
        uint32_t vertex_count,
        uint32_t triangle_count,
        uint32_t uv_count,
        bool contained_normal,
        bool contained_tangent) override {
        if (_device_mesh)
            _device_mesh->wait_finished();
        else {
            _device_mesh = new DeviceMesh{};
        }
        _submesh_offsets.clear();
        vstd::push_back_all(_submesh_offsets, submesh_offsets);
        _path = std::move(path);
        _file_offset = file_offset;
        _vertex_count = vertex_count;
        _triangle_count = triangle_count;
        _uv_count = uv_count;
        _contained_normal = contained_normal;
        _contained_tangent = contained_tangent;
        LUISA_ASSERT(host_data()->empty() || host_data()->size() == desire_size_bytes(), "Invalid host data length.");

        auto render_device = RenderDevice::instance_ptr();
        if (render_device) {
            LUISA_ASSERT(!_device_mesh->mesh_data(), "Mesh can not be re-load");
            _device_mesh->create_mesh(
                render_device->lc_main_cmd_list(),
                _vertex_count,
                _contained_normal,
                _contained_tangent,
                _uv_count,
                _triangle_count,
                vstd::vector<uint>(_submesh_offsets));
        }
    }
    bool async_load_from_file() override {
        auto render_device = RenderDevice::instance_ptr();
        if (!render_device) return false;
        if (!_device_mesh)
            _device_mesh = new DeviceMesh{};

        auto file_size = desire_size_bytes();
        if (_path.empty()) {
            return false;
        }
        LUISA_ASSERT(host_data()->empty() || host_data()->size() == file_size, "Invalid host data length.");
        _device_mesh->async_load_from_file(
            _path,
            _vertex_count,
            _contained_normal,
            _contained_tangent,
            _uv_count, vstd::vector<uint>{_submesh_offsets},
            false, true,
            _file_offset,
            file_size, !_device_mesh->host_data_ref().empty());

        return true;
    }
    void unload() override {
        _device_mesh.reset();
    }
};
DECLARE_TYPE_REGISTER(Mesh)
}// namespace rbc::world