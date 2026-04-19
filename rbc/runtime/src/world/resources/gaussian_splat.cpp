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
#include <rbc_graphics/graphics_utils.h>

namespace rbc::world {

GaussianSplatResource::GaussianSplatResource() {
    surface.buffer_id = ~0u;
}

GaussianSplatResource::~GaussianSplatResource() {
    remove_procedural_instance();
}

void GaussianSplatResource::_compute_all_aabbs(CommandList &cmdlist) {
    Shader1D<
        Buffer<luisa::compute::AABB>,// &output_buffer,
        Buffer<GaussianProbe>        // &probe_buffer
        > const *compute_aabb_shader = nullptr;
    ShaderManager::instance()->load(
        "procedural_prim/gs_compute_aabb.bin",
        compute_aabb_shader);
    if (!compute_aabb_shader) [[unlikely]] {
        LUISA_ERROR("Failed to load compute_aabb shader for GaussianSplatResource.");
        return;
    }
    auto const probe_buffer = _device_buffer->buffer().view().as<GaussianProbe>();
    if (_num_gaussians == 0) [[unlikely]] {
        LUISA_ERROR("No Gaussians to compute AABBs for.");
        return;
    }
    if (_aabb_buffer.size() != _num_gaussians) [[unlikely]] {
        LUISA_ERROR(
            "AABB buffer size {} does not match Gaussian buffer size {}.",
            _aabb_buffer.size(), _num_gaussians);
        return;
    }
    cmdlist << (*compute_aabb_shader)(
                   _aabb_buffer.view(),
                   probe_buffer)
                   .dispatch(_num_gaussians);
}

void GaussianSplatResource::_assert_size_align(uint64_t size) const {
    uint64_t const expected_size = total_size_bytes();
    if (size != expected_size) [[unlikely]] {
        LUISA_ERROR(
            "GaussianSplatResource size {} does not match expected size {} "
            "for {} Gaussians with SH degree {}.",
            size, expected_size, _num_gaussians, _sh_degree);
    }
}

uint64_t GaussianSplatResource::total_size_bytes() const {
    return probe_data_size_bytes() + sh_data_size_bytes();// + material_data_size_bytes();
}

luisa::span<GaussianProbe const> GaussianSplatResource::host_probes() const {
    if (_num_gaussians == 0) return {};
    auto const host_data = _device_buffer->host_data();
    if (host_data.size_bytes() < probe_data_size_bytes()) return {};
    auto const probe_data = host_data.subspan(0, probe_data_size_bytes());
    return {reinterpret_cast<GaussianProbe const *>(probe_data.data()), _num_gaussians};
}

luisa::span<GaussianProbe> GaussianSplatResource::host_probes() {
    if (_num_gaussians == 0) return {};
    auto const host_data = _device_buffer->host_data();
    if (host_data.size_bytes() < probe_data_size_bytes()) return {};
    auto const probe_data = host_data.subspan(0, probe_data_size_bytes());
    return {reinterpret_cast<GaussianProbe *>(probe_data.data()), _num_gaussians};
}

luisa::span<float const> GaussianSplatResource::host_sh_coeffs() const {
    if (_num_gaussians == 0 || _sh_degree == 0) return {};
    auto const host_data = _device_buffer->host_data();
    uint64_t const sh_size = sh_data_size_bytes();
    auto const sh_offset = this->sh_offset();
    if (host_data.size_bytes() < sh_offset + sh_size) return {};
    auto const sh_data = host_data.subspan(sh_offset, sh_size);
    uint32_t const num_floats = SphereHarmonic::num_floats(_sh_degree) * _num_gaussians;
    return {reinterpret_cast<float const *>(sh_data.data()), num_floats};
}

luisa::span<float> GaussianSplatResource::host_sh_coeffs() {
    if (_num_gaussians == 0 || _sh_degree == 0) return {};
    auto const host_data = _device_buffer->host_data();
    uint64_t const sh_size = sh_data_size_bytes();
    auto const sh_offset = this->sh_offset();
    if (host_data.size_bytes() < sh_offset + sh_size) return {};
    auto const sh_data = host_data.subspan(sh_offset, sh_size);
    uint32_t const num_floats = SphereHarmonic::num_floats(_sh_degree) * _num_gaussians;
    return {reinterpret_cast<float *>(sh_data.data()), num_floats};
}

// luisa::span<material::OpenPBRParticle const> GaussianSplatResource::host_materials() const {
//     if (_num_gaussians == 0) return {};
//     auto host_data = _device_buffer->host_data();
//     uint64_t mat_size = material_data_size_bytes();
//     auto mat_offset = this->material_offset();
//     if (host_data.size_bytes() < mat_offset + mat_size) return {};
//     auto mat_data = host_data.subspan(mat_offset, mat_size);
//     return {reinterpret_cast<material::OpenPBRParticle const *>(mat_data.data()), _num_gaussians};
// }

// luisa::span<material::OpenPBRParticle> GaussianSplatResource::host_materials() {
//     if (_num_gaussians == 0) return {};
//     auto host_data = _device_buffer->host_data();
//     uint64_t mat_size = material_data_size_bytes();
//     auto mat_offset = this->material_offset();
//     if (host_data.size_bytes() < mat_offset + mat_size) return {};
//     auto mat_data = host_data.subspan(mat_offset, mat_size);
//     return {reinterpret_cast<material::OpenPBRParticle *>(mat_data.data()), _num_gaussians};
// }

void GaussianSplatResource::build_procedural_primitive(
    luisa::compute::CommandList &cmdlist,
    luisa::compute::ProceduralPrimitive &procedural_prim,
    DisposeQueue &disp_queue) {
    std::lock_guard lck{_async_mtx};
    auto gu = GraphicsUtils::instance();
    if (!gu) return;
    // Use GraphicsUtils::update_buffer to upload data
    gu->update_buffer(_device_buffer.get(), 0, total_size_bytes());

    auto render_device = RenderDevice::instance_ptr();
    if (!render_device) return;

    auto &device = render_device->lc_device();

    // Create AABB buffer with size = number of Gaussians (one AABB per Gaussian)
    if (!_aabb_buffer || !_aabb_buffer.valid() || _aabb_buffer.size() != _num_gaussians) {
        _aabb_buffer = device.create_buffer<luisa::compute::AABB>(_num_gaussians);
    }

    // Compute per-Gaussian AABBs (uses fiber parallelism for large counts)
    _compute_all_aabbs(cmdlist);

    // Create procedural primitive (BLAS) with per-Gaussian AABBs
    procedural_prim = device.create_procedural_primitive(
        _aabb_buffer,
        luisa::compute::AccelOption{.allow_compaction = false});

    // Build the procedural primitive
    cmdlist << procedural_prim.build();

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
    BindlessAllocator &bindless_alloc = sm.bindless_allocator();

    // Local procedural primitive
    luisa::compute::ProceduralPrimitive procedural_prim;

    // Create procedural primitive
    build_procedural_primitive(cmdlist, procedural_prim, disp_queue);
    std::lock_guard lck{_async_mtx};

    if (!procedural_prim.valid()) {
        LUISA_ERROR("Create procedural primitive failed.");
        return ~0u;
    }
    if (surface.buffer_id == ~0u) {
        surface.buffer_id = bindless_alloc.allocate_buffer(_device_buffer->buffer());
    }
    surface.probe_offset = 0;
    surface.sh_degree = _sh_degree;
    surface.mat_buffer_offset = sh_offset();
    // Emplace the procedural instance in AccelManager
    _procedural_instance_id = accel_manager.emplace_procedural_instance(
        cmdlist,
        temp_buffer,
        buffer_allocator,
        uploader,
        disp_queue,
        std::move(procedural_prim),
        surface,
        transform,
        visibility_mask);

    _procedural_prim_dirty = false;

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
    BindlessAllocator &bindless_alloc = sm.bindless_allocator();

    std::lock_guard lck{_async_mtx};
    if (surface.buffer_id != ~0u) {
        bindless_alloc.deallocate_buffer(surface.buffer_id);
        surface.buffer_id = ~0u;
    }
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
    ser.ar.value(_procedural_prim_dirty, "procedural_prim_dirty");
}

void GaussianSplatResource::deserialize_meta(ObjDeSerialize const &ser) {
    std::shared_lock lck{_async_mtx};
    uint32_t num_gaussians = 0;
    uint32_t sh_degree = 0;
    uint32_t proc_inst_id = ~0u;
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
    if (ser.ar.value(procedural_prim_dirty, "procedural_prim_dirty")) {
        _procedural_prim_dirty = procedural_prim_dirty;
    }
}

bool GaussianSplatResource::empty() const {
    std::shared_lock lck{_async_mtx};
    return _num_gaussians == 0;
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
    // Calculate total size needed
    uint64_t total_size = total_size_bytes();

    // Resize host buffer to fit all data
    _device_buffer.reset();
    _device_buffer = new DeviceBuffer{};
    _device_buffer->create_empty(total_size, DeviceBuffer::FileLoadType::All);

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
    if (!_device_buffer) return false;
    auto const host_data = _device_buffer->host_data();
    if (host_data.empty()) return false;
    BinaryFileWriter writer{luisa::to_string(path())};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    writer.write(host_data);
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
    _device_buffer = new DeviceBuffer{};
    _device_buffer->async_load_from_file(
        path,
        0,
        total_size_bytes(),
        DeviceBuffer::FileLoadType::DeviceOnly);
    // For now, just mark as installed

    // Mark procedural primitive as dirty since data was loaded
    _procedural_prim_dirty = true;

    unsafe_set_installed();
    co_return;
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
