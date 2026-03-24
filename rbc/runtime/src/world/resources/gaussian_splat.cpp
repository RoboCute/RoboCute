#include <rbc_world/resources/gaussian_splat.h>
#include <rbc_world/resource_importer.h>
#include <rbc_world/type_register.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/accel_manager.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <rbc_core/utils/thread_waiter.h>
#include <rbc_core/binary_file_writer.h>
#include <luisa/core/fiber.h>
#include <rbc_graphics/shader_manager.h>
#include <luisa/core/logging.h>
#include <rbc_graphics/scene_manager.h>

namespace rbc::world {

GaussianSplatResource::GaussianSplatResource() = default;

GaussianSplatResource::~GaussianSplatResource() {
    auto inst = AssetsManager::instance();
    if (!inst) return;
    // Procedural primitive cleanup is handled by DisposeQueue or device destruction
    // Device resource cleanup is handled by RC
}

void GaussianSplatResource::_compute_all_aabbs(CommandList &cmdlist) {
    Shader1D<
        Buffer<AABB>,        // &output_buffer,
        Buffer<GaussianProbe>// &probe_buffer
        > const *compute_aabb_shader = nullptr;
    ShaderManager::instance()->load(
        "gaussian/compute_aabb.bin",
        compute_aabb_shader);
    if (!compute_aabb_shader) [[unlikely]] {
        LUISA_ERROR("Failed to load compute_aabb shader for GaussianSplatResource.");
        return;
    }
    auto gs_buffer = _device_buffer.buffer().view().as<GaussianProbe>();
    if (_aabb_buffer.size() != gs_buffer.size()) [[unlikely]] {
        LUISA_ERROR(
            "AABB buffer size {} does not match Gaussian buffer size {}.",
            _aabb_buffer.size(), gs_buffer.size());
        return;
    }
    cmdlist << (*compute_aabb_shader)(
                   _aabb_buffer,
                   gs_buffer)
                   .dispatch(_aabb_buffer.size());
}

void GaussianSplatResource::_assert_size_align(uint64_t size) const {
    if (size % sizeof(GaussianProbe) != 0) [[unlikely]] {
        LUISA_ERROR(
            "GaussianSplatResource size {} is not aligned to GaussianProbe size {}. "
            "Size must be a multiple of {}.",
            size, sizeof(GaussianProbe), sizeof(GaussianProbe));
    } else if ((size / sizeof(GaussianProbe)) != _num_gaussians) [[unlikely]] {
        LUISA_ERROR(
            "GaussianSplatResource Gaussian count {} does not match expected count {} "
            "from buffer size {}.",
            _num_gaussians, size / sizeof(GaussianProbe), size);
    }
}

void GaussianSplatResource::_update_offsets() {
    // Calculate the number of floats per Gaussian for features
    uint32_t sh_coeffs_per_channel = (_sh_degree + 1) * (_sh_degree + 1);
    uint32_t feature_floats_per_gaussian = sh_coeffs_per_channel * 3;

    // Calculate offsets in floats
    _pos_offset = 0;
    _feature_offset = _pos_offset + static_cast<uint64_t>(_num_gaussians) * 3;
    _opacity_offset = _feature_offset + static_cast<uint64_t>(_num_gaussians) * feature_floats_per_gaussian;
    _scale_offset = _opacity_offset + _num_gaussians;
    _rotq_offset = _scale_offset + static_cast<uint64_t>(_num_gaussians) * 3;
}

void GaussianSplatResource::build_procedural_primitive(
    luisa::compute::CommandList &cmdlist,
    DisposeQueue &disp_queue) {
    std::lock_guard lck{_async_mtx};

    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return;

    auto &device = render_device->lc_device();

    // Compute per-Gaussian AABBs (uses fiber parallelism for large counts)
    _compute_all_aabbs(cmdlist);

    // Create AABB buffer with size = number of Gaussians (one AABB per Gaussian)
    if (!_aabb_buffer || !_aabb_buffer.valid() || _aabb_buffer.size() != _num_gaussians) {
        _aabb_buffer = device.create_buffer<luisa::compute::AABB>(_num_gaussians);
    }

    // Create procedural primitive (BLAS) with per-Gaussian AABBs
    _procedural_prim = device.create_procedural_primitive(
        _aabb_buffer,
        luisa::compute::AccelOption{.allow_compaction = false});

    // Build the procedural primitive
    cmdlist << _procedural_prim.build();

    _procedural_prim_dirty = false;
}

uint GaussianSplatResource::emplace_procedural_instance(
    luisa::float4x4 const &transform,
    uint8_t visibility_mask) {
    auto &sm = SceneManager::instance();
    AccelManager &accel_manager = sm.accel_manager();
    luisa::compute::CommandList &cmdlist = RenderDevice::instance().lc_main_cmd_list();
    HostBufferManager &temp_buffer = sm.host_upload_buffer();
    BufferAllocator &buffer_allocator = sm.buffer_allocator();
    BufferUploader &uploader = sm.buffer_uploader();
    DisposeQueue &disp_queue = sm.dispose_queue();

    std::lock_guard lck{_async_mtx};

    // Create procedural primitive if needed
    if (!_procedural_prim.valid() || _procedural_prim_dirty) {
        build_procedural_primitive(cmdlist, disp_queue);
    }

    if (!_procedural_prim.valid()) {
        return ~0u;// Failed to create procedural primitive
    }

    // Create procedural data variant - Gaussian splats are typically rendered
    // as voxel-like surfaces or custom procedural geometry
    // Using VoxelSurface as the closest match for Gaussian splatting
    geometry::VoxelSurface voxel_surface{
        .aabb_buffer_heap_idx = 0,// Will be set by buffer allocator
        .aabb_buffer_offset = 0};

    // Emplace the procedural instance in AccelManager
    _procedural_instance_id = accel_manager.emplace_procedural_instance(
        cmdlist,
        temp_buffer,
        buffer_allocator,
        uploader,
        disp_queue,
        std::move(_procedural_prim),
        std::move(voxel_surface),
        transform,
        visibility_mask);

    return _procedural_instance_id;
}

void GaussianSplatResource::set_procedural_instance(
    luisa::float4x4 const &transform,
    uint8_t visibility_mask,
    bool opaque) {
    std::lock_guard lck{_async_mtx};
    auto &sm = SceneManager::instance();
    AccelManager &accel_manager = sm.accel_manager();

    if (_procedural_instance_id == ~0u) {
        LUISA_WARNING("Cannot set procedural instance: instance not emplaced yet.");
        return;
    }

    accel_manager.set_procedural_instance(
        _procedural_instance_id,
        transform,
        visibility_mask,
        opaque);
}

void GaussianSplatResource::remove_procedural_instance() {
    auto &sm = SceneManager::instance();
    AccelManager &accel_manager = sm.accel_manager();
    BufferAllocator &buffer_allocator = sm.buffer_allocator();
    BufferUploader &uploader = sm.buffer_uploader();
    DisposeQueue &disp_queue = sm.dispose_queue();

    std::lock_guard lck{_async_mtx};

    if (_procedural_instance_id == ~0u) {
        return;// Not emplaced, nothing to do
    }

    accel_manager.remove_procedural_instance(
        buffer_allocator,
        uploader,
        disp_queue,
        _procedural_instance_id);

    _procedural_instance_id = ~0u;
}

void GaussianSplatResource::serialize_meta(ObjSerialize const &ser) const {
    std::shared_lock lck{_async_mtx};
    ser.ar.value(_num_gaussians, "num_gaussians");
    ser.ar.value(_sh_degree, "sh_degree");
    ser.ar.value(_procedural_instance_id, "procedural_instance_id");
    ser.ar.value(_pos_offset, "pos_offset");
    ser.ar.value(_feature_offset, "feature_offset");
    ser.ar.value(_opacity_offset, "opacity_offset");
    ser.ar.value(_scale_offset, "scale_offset");
    ser.ar.value(_rotq_offset, "rotq_offset");
    ser.ar.value(_procedural_prim_dirty, "procedural_prim_dirty");
}

void GaussianSplatResource::deserialize_meta(ObjDeSerialize const &ser) {
    std::shared_lock lck{_async_mtx};
    uint32_t num_gaussians = 0;
    uint32_t sh_degree = 0;
    uint32_t proc_inst_id = ~0u;
    uint64_t pos_offset = 0;
    uint64_t feature_offset = 0;
    uint64_t opacity_offset = 0;
    uint64_t scale_offset = 0;
    uint64_t rotq_offset = 0;
    bool procedural_prim_dirty = true;

    if (ser.ar.value(num_gaussians, "num_gaussians")) {
        _num_gaussians = num_gaussians;
    }
    if (ser.ar.value(sh_degree, "sh_degree")) {
        _sh_degree = sh_degree;
    }
    if (ser.ar.value(proc_inst_id, "procedural_instance_id")) {
        _procedural_instance_id = proc_inst_id;
    }
    if (ser.ar.value(pos_offset, "pos_offset")) {
        _pos_offset = pos_offset;
    }
    if (ser.ar.value(feature_offset, "feature_offset")) {
        _feature_offset = feature_offset;
    }
    if (ser.ar.value(opacity_offset, "opacity_offset")) {
        _opacity_offset = opacity_offset;
    }
    if (ser.ar.value(scale_offset, "scale_offset")) {
        _scale_offset = scale_offset;
    }
    if (ser.ar.value(rotq_offset, "rotq_offset")) {
        _rotq_offset = rotq_offset;
    }
    if (ser.ar.value(procedural_prim_dirty, "procedural_prim_dirty")) {
        _procedural_prim_dirty = procedural_prim_dirty;
    }
}

bool GaussianSplatResource::empty() const {
    std::shared_lock lck{_async_mtx};
    return _num_gaussians == 0;
}

uint64_t GaussianSplatResource::data_size_bytes() const {
    std::shared_lock lck{_async_mtx};
    return host_data().size() * sizeof(float);
}

void GaussianSplatResource::create_empty(uint32_t num_gaussians, uint32_t sh_degree) {
    ThreadWaiter waiter;
    while (loading_status() == EResourceLoadingStatus::Loading) {
        waiter.wait(std::chrono::microseconds(10), "Last Gaussian splat loading.");
    }
    unsafe_set_loading_status_min(EResourceLoadingStatus::Unloaded);
    std::lock_guard lck{_async_mtx};
    _device_res.reset();
    _num_gaussians = num_gaussians;
    _sh_degree = sh_degree;

    // Update offsets based on new parameters
    _update_offsets();

    // Calculate total size needed
    // Positions: 3 floats per Gaussian (packed float3: [x, y, z])
    // Features: N * (sh_degree+1)^2 * 3 floats
    // Opacity: 1 float per Gaussian
    // Scale: 3 floats per Gaussian
    // Rotation: 4 floats per Gaussian (quaternion xyzw)
    uint32_t sh_coeffs_per_channel = (sh_degree + 1) * (sh_degree + 1);
    uint64_t total_floats = static_cast<uint64_t>(num_gaussians) * (3 + sh_coeffs_per_channel * 3 + 1 + 3 + 4);

    // Resize host buffer to fit all data
    _device_buffer.create_empty(total_floats * sizeof(float), DeviceBuffer::FileLoadType::All);

    _procedural_prim_dirty = true;

    // Note: Device resource would be created separately
    unsafe_set_loaded();
}

bool GaussianSplatResource::_install() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return false;

    // Mark procedural primitive as dirty since data changed
    _procedural_prim_dirty = true;

    return true;
}

bool GaussianSplatResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    auto data = host_data();
    if (data.empty()) return false;
    BinaryFileWriter writer{luisa::to_string(path())};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(luisa::span{
        reinterpret_cast<std::byte const *>(data.data()),
        data.size() * sizeof(GaussianProbe)});
    return true;
}

rbc::coroutine GaussianSplatResource::_async_load() {
    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) co_return;

    auto path = this->path();
    if (path.empty()) {
        co_return;
    }

    std::lock_guard lck{_async_mtx};
    if (_device_res) {
        co_return;
    }

    // For now, just mark as installed

    // Mark procedural primitive as dirty since data was loaded
    _procedural_prim_dirty = true;

    unsafe_set_installed();
    co_return;
}

bool GaussianSplatResource::decode(luisa::filesystem::path const &path) {
    if (!empty()) [[unlikely]] {
        LUISA_WARNING("Can not create on exists Gaussian splat.");
        return false;
    }

    auto &registry = ResourceImporterRegistry::instance();
    auto *importer = registry.find_importer(path, TypeInfo::get<GaussianSplatResource>().md5());

    if (!importer) {
        LUISA_WARNING("No importer found for Gaussian splat file: {}", luisa::to_string(path));
        return false;
    }

    // Avoid dynamic_cast across DLL boundaries - use resource_type() check instead
    if (importer->resource_type() != TypeInfo::get<GaussianSplatResource>().md5()) {
        LUISA_WARNING("Invalid importer type for Gaussian splat file: {}", luisa::to_string(path));
        return false;
    }

    // TODO: Implement IGaussianSplatImporter interface and Gaussian splat PLY loading
    // This requires parsing PLY files with Gaussian-specific attributes like
    // scale, rotation (quaternion), opacity, and spherical harmonics coefficients
    LUISA_WARNING("Gaussian splat PLY loading not yet implemented for file: {}", luisa::to_string(path));
    return false;
}

BaseObjectType GaussianSplatResource::base_type() const {
    return BaseObjectType::Resource;
}

MD5 GaussianSplatResource::type_id() const {
    return rbc_rtti_detail::is_rtti_type<GaussianSplatResource>::get_md5();
}

const char *GaussianSplatResource::type_name() const {
    return rbc_rtti_detail::is_rtti_type<GaussianSplatResource>::name;
}

DECLARE_WORLD_OBJECT_REGISTER(GaussianSplatResource)

}// namespace rbc::world
