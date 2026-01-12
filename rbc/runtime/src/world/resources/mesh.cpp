#include <rbc_world/resources/mesh.h>
#include <rbc_world/resource_importer.h>
#include <rbc_core/binary_file_writer.h>
#include <rbc_world/type_register.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/device_transforming_mesh.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_graphics/graphics_utils.h>
namespace rbc::world {
MeshResource::MeshResource() {
    _origin_mesh.reset();
}
MeshResource::~MeshResource() {
    auto inst = AssetsManager::instance();
    if (!inst) return;
    for (auto &i : _custom_properties) {
        if (i.second.device_buffer) {
            inst->dispose_after_render_frame(std::move(i.second.device_buffer));
        }
    }
}

void MeshResource::serialize_meta(ObjSerialize const &ser) const {
    std::shared_lock lck{_async_mtx};
    ser.ar.start_array();
    for (auto &i : _custom_properties) {
        ser.ar.value(i.first);
        ser.ar.start_object();
        auto &v = i.second;
        ser.ar.value(v.offset_bytes, "offset_bytes");
        ser.ar.value(v.size_bytes, "size_bytes");
        ser.ar.end_object();
    }
    ser.ar.end_array("properties");
    // skinning mesh
    if (_origin_mesh) {
        ser.ar.value(_origin_mesh->guid(), "origin_mesh");
        return;
    }

    ser.ar.value(_contained_normal, "contained_normal");
    ser.ar.value(_contained_tangent, "contained_tangent");
    ser.ar.value(is_transforming_mesh(), "is_transforming_mesh");
    ser.ar.value(_vertex_count, "vertex_count");
    ser.ar.value(_triangle_count, "triangle_count");
    ser.ar.value(_uv_count, "uv_count");
    ser.ar.start_array();
    for (auto &i : _submesh_offsets) {
        ser.ar.value(i);
    }
    ser.ar.end_array("submesh_offsets");
}

bool MeshResource::empty() const {
    std::shared_lock lck{_async_mtx};
    return !_device_res;
}

void MeshResource::deserialize_meta(ObjDeSerialize const &ser) {
    std::shared_lock lck{_async_mtx};
    uint64_t size;
    if (ser.ar.start_array(size, "properties")) {
        auto dsp = vstd::scope_exit([&] {
            ser.ar.end_scope();
        });
        if ((size & 1) != 0) {
            return;
        }
        _custom_properties.reserve(size / 2);
        for (auto i : vstd::range(size)) {
            luisa::string s;
            if (!ser.ar.value(s)) return;
            if (!ser.ar.start_object()) return;
            auto dsp = vstd::scope_exit([&] {
                ser.ar.end_scope();
            });
            uint64_t offset_bytes, size_bytes;
            if (!ser.ar.value(offset_bytes, "offset_bytes")) continue;
            if (!ser.ar.value(size_bytes, "size_bytes")) continue;
            auto &v = _custom_properties.emplace(std::move(s)).value();
            v.offset_bytes = offset_bytes;
            v.size_bytes = size_bytes;
        }
    }

    vstd::Guid origin_mesh_guid;
    // this is a skinning mesh
    if (ser.ar.value(origin_mesh_guid, "origin_mesh")) {
        _origin_mesh = load_resource(origin_mesh_guid, false);
        _copy_from_mesh(_origin_mesh.get());
        return;
    }
#define RBC_MESH_LOAD(m)           \
    {                              \
        decltype(_##m) m;          \
        if (ser.ar.value(m, #m)) { \
            _##m = m;              \
        }                          \
    }
    RBC_MESH_LOAD(contained_normal)
    RBC_MESH_LOAD(contained_tangent)
    RBC_MESH_LOAD(vertex_count)
    RBC_MESH_LOAD(triangle_count)
    RBC_MESH_LOAD(uv_count)

#undef RBC_MESH_LOAD
    if (ser.ar.start_array(size, "submesh_offsets")) {
        _submesh_offsets.reserve(size);
        for (auto i : vstd::range(size)) {
            uint v;
            if (ser.ar.value(v)) {
                _submesh_offsets.push_back(v);
            }
        }
        ser.ar.end_scope();
    }
}
luisa::vector<std::byte> *MeshResource::host_data() {
    auto mesh = device_mesh();
    std::shared_lock lck{_async_mtx};
    if (mesh) {
        return &mesh->host_data_ref();
    } else
        return nullptr;
}
uint64_t MeshResource::basic_size_bytes() const {
    return DeviceMesh::get_mesh_size(_vertex_count, _contained_normal, _contained_tangent, _uv_count, _triangle_count);
}
uint64_t MeshResource::extra_size_bytes() const {
    uint64_t size = 0;
    for (auto &i : _custom_properties) {
        size += i.second.size_bytes;
    }
    return size;
}
uint64_t MeshResource::desire_size_bytes() const {
    return basic_size_bytes() + extra_size_bytes();
}

void MeshResource::create_empty(
    luisa::vector<uint> &&submesh_offsets,
    uint32_t vertex_count,
    uint32_t triangle_count,
    uint32_t uv_count,
    bool contained_normal,
    bool contained_tangent) {
    while (_status == EResourceLoadingStatus::Loading) {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    _status = EResourceLoadingStatus::Unloaded;
    std::lock_guard lck{_async_mtx};
    _device_res.reset();
    _submesh_offsets = std::move(submesh_offsets);
    _vertex_count = vertex_count;
    _triangle_count = triangle_count;
    _uv_count = uv_count;
    _contained_normal = contained_normal;
    _contained_tangent = contained_tangent;
    _origin_mesh.reset();
    _device_res = new DeviceMesh{};
    unsafe_set_loaded();
}
void MeshResource::_copy_from_mesh(MeshResource *origin_mesh) {
    _submesh_offsets.clear();
    vstd::push_back_all(_submesh_offsets, origin_mesh->submesh_offsets());
    _vertex_count = origin_mesh->vertex_count();
    _triangle_count = origin_mesh->triangle_count();
    _uv_count = origin_mesh->uv_count();
    _contained_normal = origin_mesh->contained_normal();
    _contained_tangent = origin_mesh->contained_tangent();
    _origin_mesh = origin_mesh;
}
void MeshResource::create_as_morphing_instance(MeshResource *origin_mesh) {
    std::lock_guard lck{_async_mtx};
    _copy_from_mesh(origin_mesh);
    auto tr_mesh = new DeviceTransformingMesh{};
    _device_res = tr_mesh;
    unsafe_set_loaded();
}

bool MeshResource::_install() {
    auto render_device = RenderDevice::instance_ptr();
    if (!is_transforming_mesh()) {
        auto mesh = device_mesh();
        if (!render_device || !mesh || mesh->mesh_data()) return false;
        auto host_data_ = host_data();
        auto file_size = desire_size_bytes();
        LUISA_ASSERT(host_data_->empty() || host_data_->size() == file_size, "Host data length {} mismatch with required length {}.", host_data_->size(), file_size);
        {
            std::lock_guard lck{_async_mtx};
            mesh->create_mesh(
                render_device->lc_main_cmd_list(),
                _vertex_count,
                _contained_normal,
                _contained_tangent,
                _uv_count,
                _triangle_count,
                vstd::vector<uint>(_submesh_offsets));
        }
        auto graphics = GraphicsUtils::instance();
        if (graphics) {
            graphics->update_mesh_data(mesh, false);
        }
    } else {
        if (!render_device || !_device_res) return false;
        auto coro = [](MeshResource *mesh_res) -> coroutine {
            co_await mesh_res->await_loading();
        }(_origin_mesh.get());
        while (!coro.done()) {
            std::this_thread::yield();
            coro.resume();
            std::this_thread::yield();
        }
        auto tr_mesh = static_cast<DeviceTransformingMesh *>(_device_res.get());
        if (tr_mesh->mesh_data()) return false;
        tr_mesh->create_from_origin(_origin_mesh->device_mesh());
        tr_mesh->copy_from_origin(render_device->lc_main_cmd_list());
    }
    return true;
}

bool MeshResource::unsafe_save_to_path() const {
    if (is_transforming_mesh()) return false;
    std::shared_lock lck{_async_mtx};
    auto mesh = device_mesh();
    if (!mesh || mesh->host_data().empty()) return false;
    BinaryFileWriter writer{luisa::to_string(path())};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(mesh->host_data());
    return true;
}
rbc::coroutine MeshResource::_async_load() {
    _status = EResourceLoadingStatus::Loading;
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) co_return;
    auto file_size = desire_size_bytes();
    auto path = this->path();
    if (path.empty()) {
        co_return;
    }
    std::lock_guard lck{_async_mtx};
    if (_device_res) {
        co_return;
    }
    if (!is_transforming_mesh()) {
        auto mesh = new DeviceMesh{};
        _device_res = mesh;
        mesh->async_load_from_file(
            path,
            _vertex_count,
            _triangle_count,
            _contained_normal,
            _contained_tangent,
            _uv_count, vstd::vector<uint>{_submesh_offsets},
            false, true,
            0,
            true, extra_size_bytes());
    } else {
        if (!_origin_mesh) {
            co_return;
        }
        co_await _origin_mesh->await_loading();
        auto origin_mesh = _origin_mesh->device_mesh();
        if (!origin_mesh)
            co_return;
        auto mesh = new DeviceTransformingMesh();
        _device_res = mesh;
        mesh->async_load(RC<DeviceMesh>{origin_mesh});
    }

    while (!_device_res->load_finished()) {
        co_await std::suspend_always{};
    }
    unsafe_set_installed();
    co_return;
}

// import
bool MeshResource::decode(luisa::filesystem::path const &path) {
    if (!empty()) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }

    auto &registry = ResourceImporterRegistry::instance();
    auto *importer = registry.find_importer(path, TypeInfo::get<MeshResource>().md5());

    if (!importer) {
        LUISA_WARNING("No importer found for mesh file: {}", luisa::to_string(path));
        return false;
    }

    // Avoid dynamic_cast across DLL boundaries - use resource_type() check instead
    if (importer->resource_type() != TypeInfo::get<MeshResource>().md5()) {
        LUISA_WARNING("Invalid importer type for mesh file: {}", luisa::to_string(path));
        return false;
    }
    // Safe to use static_cast after type check
    auto *mesh_importer = static_cast<IMeshImporter *>(importer);

    return mesh_importer->import(this, path);
}

auto MeshResource::add_property(luisa::string &&name, size_t size_bytes) -> std::pair<CustomProperty &, luisa::span<std::byte>> {
    auto h = host_data();
    LUISA_ASSERT(h, "Can not add property to empty mesh");
    auto offset = desire_size_bytes();
    offset = (offset + 15ull) & (~15ull);
    auto iter = _custom_properties.try_emplace(std::move(name));
    auto &v = iter.first.value();
    if (!iter.second) {
        LUISA_WARNING("Try to add property {} multiple times, ignored.", iter.first.key());
        return {v, {}};
    }
    v.offset_bytes = offset;
    v.size_bytes = size_bytes;
    offset += size_bytes;
    h->resize_uninitialized(offset);
    return {v, luisa::span{*h}.subspan(v.offset_bytes, v.size_bytes)};
}

luisa::span<std::byte> MeshResource::get_property_host(luisa::string_view name) {
    auto iter = _custom_properties.find(name);
    if (!iter) return {};
    auto h = host_data();
    if (!h) return {};
    auto &v = iter.value();
    return luisa::span{*h}.subspan(v.offset_bytes, v.size_bytes);
}

luisa::compute::ByteBufferView MeshResource::get_property_buffer(luisa::string_view name) {
    auto iter = _custom_properties.find(name);
    if (!iter) return {};
    auto &v = iter.value();
    std::shared_lock lck{v.mtx};
    return v.device_buffer;
}
luisa::compute::ByteBufferView MeshResource::get_or_create_property_buffer(luisa::string_view name) {
    auto iter = _custom_properties.find(name);
    if (!iter) return {};
    auto &v = iter.value();
    std::shared_lock lck{v.mtx};
    auto &b = v.device_buffer;
    if (!b) {
        lck.unlock();
        std::lock_guard lck{v.mtx};
        b = RenderDevice::instance().lc_device().create_byte_buffer(
            (v.size_bytes + 15ull) & (~15ull));
    }
    return b;
}
bool MeshResource::is_transforming_mesh() const {
    if (_device_res) {
        return _device_res->resource_type() == DeviceResource::Type::TransformingMesh;
    }
    return _origin_mesh.get();
}
DeviceMesh *MeshResource::device_mesh() const {
    if (_device_res && _device_res->resource_type() == DeviceResource::Type::Mesh) {
        return static_cast<DeviceMesh *>(_device_res.get());
    }
    return nullptr;
}
DeviceTransformingMesh *MeshResource::device_transforming_mesh() const {
    if (_device_res && _device_res->resource_type() == DeviceResource::Type::TransformingMesh) {
        return static_cast<DeviceTransformingMesh *>(_device_res.get());
    }
    return nullptr;
}
MeshManager::MeshData *MeshResource::mesh_data() const {
    if (!_device_res) return nullptr;
    if (is_transforming_mesh()) {
        return static_cast<DeviceTransformingMesh *>(_device_res.get())->mesh_data();
    }
    return static_cast<DeviceMesh *>(_device_res.get())->mesh_data();
}
DECLARE_WORLD_OBJECT_REGISTER(MeshResource)
}// namespace rbc::world