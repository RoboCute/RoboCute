#include <rbc_world_v2/mesh.h>
#include <rbc_world_v2/type_register.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/device_assets/device_mesh.h>

namespace rbc::world {
Mesh::~Mesh() {
    int x = 0;
}

void Mesh::serialize(ObjSerialize const &ser) const {
    BaseType::serialize(ser);
    ser.ser._store(_contained_normal, "contained_normal");
    ser.ser._store(_contained_tangent, "contained_tangent");
    ser.ser._store(_vertex_count, "vertex_count");
    ser.ser._store(_triangle_count, "triangle_count");
    ser.ser._store(_uv_count, "uv_count");
    ser.ser.start_array();
    for (auto &i : _submesh_offsets) {
        ser.ser._store(i);
    }
    ser.ser.add_last_scope_to_object("submesh_offsets");
}
void Mesh::deserialize(ObjDeSerialize const &ser) {
    BaseType::deserialize(ser);
#define RBC_MESH_LOAD(m)            \
    {                               \
        decltype(_##m) m;           \
        if (ser.ser._load(m, #m)) { \
            _##m = m;               \
        }                           \
    }
    RBC_MESH_LOAD(file_offset)
    RBC_MESH_LOAD(contained_normal)
    RBC_MESH_LOAD(contained_tangent)
    RBC_MESH_LOAD(vertex_count)
    RBC_MESH_LOAD(triangle_count)
    RBC_MESH_LOAD(uv_count)
#undef RBC_MESH_LOAD
    uint64_t size;
    if (ser.ser.start_array(size, "submesh_offsets")) {
        _submesh_offsets.reserve(size);
        for (auto i : vstd::range(size)) {
            uint v;
            if (ser.ser._load(v)) {
                _submesh_offsets.push_back(v);
            }
        }
    }
}
void Mesh::wait_load() const {
    if (_device_mesh)
        _device_mesh->wait_finished();
}
luisa::vector<std::byte> *Mesh::host_data() {
    if (_device_mesh)
        return &_device_mesh->host_data_ref();
    else
        return nullptr;
}
uint64_t Mesh::desire_size_bytes() {
    return DeviceMesh::get_mesh_size(_vertex_count, _contained_normal, _contained_tangent, _uv_count, _triangle_count);
}
void Mesh::decode(luisa::filesystem::path const &path) {
    std::lock_guard lck{_async_mtx};
    wait_load();
    if (loaded()) [[unlikely]] {
        LUISA_ERROR("Can not create on exists mesh.");
    }
    
}
void Mesh::create_empty(
    luisa::filesystem::path &&path,
    luisa::vector<uint> &&submesh_offsets,
    uint64_t file_offset,
    uint32_t vertex_count,
    uint32_t triangle_count,
    uint32_t uv_count,
    bool contained_normal,
    bool contained_tangent) {
    std::lock_guard lck{_async_mtx};
    wait_load();
    if (loaded()) [[unlikely]] {
        LUISA_ERROR("Can not create on exists mesh.");
    }
    _submesh_offsets = std::move(submesh_offsets);
    _path = std::move(path);
    _file_offset = file_offset;
    _vertex_count = vertex_count;
    _triangle_count = triangle_count;
    _uv_count = uv_count;
    _contained_normal = contained_normal;
    _contained_tangent = contained_tangent;
    if (_device_mesh) {
        if (_device_mesh->loaded()) [[unlikely]] {
            LUISA_ERROR("Can not be create repeatly.");
        }
    } else {
        _device_mesh = new DeviceMesh{};
    }
    LUISA_ASSERT(host_data()->empty() || host_data()->size() == desire_size_bytes(), "Invalid host data length.");
}
bool Mesh::init_device_resource() {
    std::lock_guard lck{_async_mtx};
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device || !_device_mesh || loaded()) return false;
    _device_mesh->create_mesh(
        render_device->lc_main_cmd_list(),
        _vertex_count,
        _contained_normal,
        _contained_tangent,
        _uv_count,
        _triangle_count,
        vstd::vector<uint>(_submesh_offsets));
    return true;
}
bool Mesh::async_load_from_file() {
    std::lock_guard lck{_async_mtx};
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;
    if (_device_mesh) {
        if (_device_mesh->loaded()) [[unlikely]] {
            return false;
        }
    } else {
        _device_mesh = new DeviceMesh{};
    }

    auto file_size = desire_size_bytes();
    if (_path.empty()) {
        return false;
    }
    LUISA_ASSERT(host_data()->empty() || host_data()->size() == file_size, "Invalid host data length.");
    _device_mesh->async_load_from_file(
        _path,
        _vertex_count,
        _triangle_count,
        _contained_normal,
        _contained_tangent,
        _uv_count, vstd::vector<uint>{_submesh_offsets},
        false, true,
        _file_offset,
        !_device_mesh->host_data_ref().empty());

    return true;
}
bool Mesh::loaded() const {
    return _device_mesh && (_device_mesh->loaded() || _device_mesh->mesh_data());
}
void Mesh::unload() {
    _device_mesh.reset();
}
DECLARE_WORLD_OBJECT_REGISTER(Mesh)
}// namespace rbc::world