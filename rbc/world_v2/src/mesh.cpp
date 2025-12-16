#include <rbc_world_v2/mesh.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_world_v2/type_register.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/device_assets/device_mesh.h>
namespace rbc::world {
Mesh::Mesh() = default;
Mesh::~Mesh() {
}

void Mesh::serialize(ObjSerialize const &ser) const {
    BaseType::serialize(ser);
    ser.ser._store(_contained_normal, "contained_normal");
    ser.ser._store(_contained_tangent, "contained_tangent");
    ser.ser._store(_vertex_count, "vertex_count");
    ser.ser._store(_triangle_count, "triangle_count");
    ser.ser._store(_uv_count, "uv_count");
    ser.ser._store(_vertex_color_channels, "vertex_color_channels");
    ser.ser._store(_skinning_weight_count, "skinning_weight_count");
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
    RBC_MESH_LOAD(vertex_color_channels)
    RBC_MESH_LOAD(skinning_weight_count)
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
uint64_t Mesh::basic_size_bytes() const {
    return DeviceMesh::get_mesh_size(_vertex_count, _contained_normal, _contained_tangent, _uv_count, _triangle_count);
}
uint64_t Mesh::desire_size_bytes() const {
    return basic_size_bytes() + _vertex_count * _skinning_weight_count * sizeof(float) + _vertex_count * _vertex_color_channels;
}

void Mesh::create_empty(
    luisa::filesystem::path &&path,
    luisa::vector<uint> &&submesh_offsets,
    uint64_t file_offset,
    uint32_t vertex_count,
    uint32_t triangle_count,
    uint32_t uv_count,
    bool contained_normal,
    bool contained_tangent,
    uint vertex_color_channels,
    uint skinning_weight_count) {
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
    _skinning_weight_count = skinning_weight_count;
    _vertex_color_channels = vertex_color_channels;
    if (_device_mesh) {
        if (_device_mesh->loaded()) [[unlikely]] {
            LUISA_ERROR("Can not be create repeatly.");
        }
    } else {
        _device_mesh = new DeviceMesh{};
    }
}
bool Mesh::init_device_resource() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device || !_device_mesh || loaded()) return false;
    LUISA_ASSERT(host_data()->empty() || host_data()->size() == desire_size_bytes(), "Invalid host data length.");
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
bool Mesh::unsafe_save_to_path() const {
    if (!_device_mesh || _device_mesh->host_data().empty()) return false;
    BinaryFileWriter writer{luisa::to_string(_path)};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(_device_mesh->host_data());
    return true;
}
bool Mesh::loaded() const {
    return _device_mesh && (_device_mesh->loaded() || _device_mesh->mesh_data());
}
void Mesh::unload() {
    _device_mesh.reset();
}
luisa::span<SkinWeight const> Mesh::skin_weights() const {
    if (!_device_mesh || _device_mesh->host_data_ref().empty() || _skinning_weight_count == 0) return {};
    return luisa::span{
        (SkinWeight *)(_device_mesh->host_data_ref().data() + basic_size_bytes()),
        _skinning_weight_count * _vertex_count};
}
luisa::span<float const> Mesh::vertex_colors() const {
    if (!_device_mesh || _device_mesh->host_data_ref().empty() || _vertex_color_channels == 0) return {};
    return luisa::span{
        (float *)(skin_weights().data() + skin_weights().size()),
        _vertex_color_channels * _vertex_count};
}
DECLARE_WORLD_OBJECT_REGISTER(Mesh)
}// namespace rbc::world