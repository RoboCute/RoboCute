#include <rbc_world/resources/mesh.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_world/type_register.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/device_assets/device_mesh.h>
namespace rbc::world {
MeshResource::MeshResource() = default;
MeshResource::~MeshResource() {
}

void MeshResource::serialize_meta(ObjSerialize const &ser) const {
    std::shared_lock lck{_async_mtx};
    BaseType::serialize_meta(ser);
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

bool MeshResource::empty() const {
    std::shared_lock lck{_async_mtx};
    return !_device_mesh;
}

void MeshResource::deserialize_meta(ObjDeSerialize const &ser) {
    std::shared_lock lck{_async_mtx};
    BaseType::deserialize_meta(ser);
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
        ser.ser.end_scope();
    }
}
luisa::vector<std::byte> *MeshResource::host_data() {
    std::shared_lock lck{_async_mtx};
    if (_device_mesh) {
        return &_device_mesh->host_data_ref();
    } else
        return nullptr;
}
uint64_t MeshResource::basic_size_bytes() const {
    return DeviceMesh::get_mesh_size(_vertex_count, _contained_normal, _contained_tangent, _uv_count, _triangle_count);
}
uint64_t MeshResource::desire_size_bytes() const {
    return basic_size_bytes() + _vertex_count * _skinning_weight_count * sizeof(float) + _vertex_count * _vertex_color_channels;
}

void MeshResource::create_empty(
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
    _status = EResourceLoadingStatus::Loading;
    if (_device_mesh) [[unlikely]] {
        LUISA_ERROR("Can not create on exists mesh.");
    }
    std::lock_guard lck{_async_mtx};
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
    _device_mesh = new DeviceMesh{};
}
bool MeshResource::init_device_resource() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device || !_device_mesh || _device_mesh->mesh_data()) return false;
    auto host_data_ = host_data();
    LUISA_ASSERT(host_data_->empty() || host_data_->size() == desire_size_bytes(), "Host data length {} mismatch with required length {}.", host_data_->size(), desire_size_bytes());
    {
        std::lock_guard lck{_async_mtx};
        _device_mesh->create_mesh(
            render_device->lc_main_cmd_list(),
            _vertex_count,
            _contained_normal,
            _contained_tangent,
            _uv_count,
            _triangle_count,
            vstd::vector<uint>(_submesh_offsets));
    }
    _status = EResourceLoadingStatus::Loaded;
    return true;
}
bool MeshResource::_async_load_from_file() {
    _status = EResourceLoadingStatus::Loading;
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;
    auto file_size = desire_size_bytes();
    if (_path.empty()) {
        return false;
    }
    std::lock_guard lck{_async_mtx};
    if (_device_mesh) {
        return false;
    }
    _device_mesh = new DeviceMesh{};
    _device_mesh->async_load_from_file(
        _path,
        _vertex_count,
        _triangle_count,
        _contained_normal,
        _contained_tangent,
        _uv_count, vstd::vector<uint>{_submesh_offsets},
        false, true,
        _file_offset,
        true, desire_size_bytes() - basic_size_bytes());
    return true;
}
bool MeshResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    if (!_device_mesh || _device_mesh->host_data().empty()) return false;
    BinaryFileWriter writer{luisa::to_string(_path)};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(_device_mesh->host_data());
    return true;
}
rbc::coro::coroutine MeshResource::_async_load() {
    if (!_async_load_from_file()) co_return;
    while (!_device_mesh->load_finished()) {
        co_await std::suspend_always{};
    }
    _status = EResourceLoadingStatus::Loaded;
    co_return;
}
void MeshResource::_unload() {
    std::lock_guard lck{_async_mtx};
    _device_mesh.reset();
}
luisa::span<SkinWeight const> MeshResource::skin_weights() const {
    if (!_device_mesh || _device_mesh->host_data_ref().empty() || _skinning_weight_count == 0) return {};
    return luisa::span{
        (SkinWeight *)(_device_mesh->host_data_ref().data() + basic_size_bytes()),
        _skinning_weight_count * _vertex_count};
}
luisa::span<float const> MeshResource::vertex_colors() const {
    if (!_device_mesh || _device_mesh->host_data_ref().empty() || _vertex_color_channels == 0) return {};
    return luisa::span{
        (float *)(skin_weights().data() + skin_weights().size()),
        _vertex_color_channels * _vertex_count};
}
DECLARE_WORLD_OBJECT_REGISTER(MeshResource)
}// namespace rbc::world