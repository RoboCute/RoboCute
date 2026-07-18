#include <rbc_graphics/scene_manager.h>
#include <luisa/core/logging.h>
#include <luisa/runtime/context.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_graphics/managed_device.h>
namespace rbc {

void SceneManager::_apply_shader_feature_transition_locked(
    uint64_t old_mask,
    bool old_active,
    uint64_t new_mask,
    bool new_active) {
    auto const old_features = old_active ? old_mask : 0u;
    auto const new_features = new_active ? new_mask : 0u;
    auto const removed_features = old_features & ~new_features;
    auto const added_features = new_features & ~old_features;
    auto const previous_mask = _shader_feature_mask;
    for (uint32_t bit = 0u; bit < _shader_feature_counts.size(); ++bit) {
        auto const feature = 1ull << bit;
        if ((removed_features & feature) != 0u) {
            auto &count = _shader_feature_counts[bit];
            LUISA_DEBUG_ASSERT(count != 0u);
            if (--count == 0u) {
                _shader_feature_mask &= ~feature;
            }
        }
        if ((added_features & feature) != 0u) {
            auto &count = _shader_feature_counts[bit];
            if (count++ == 0u) {
                _shader_feature_mask |= feature;
            }
        }
    }
    if (_shader_feature_mask != previous_mask) {
        _published_shader_feature_mask.store(
            _shader_feature_mask, std::memory_order_release);
    }
}

SceneShaderFeatureSnapshot SceneManager::shader_features() const {
    return SceneShaderFeatureSnapshot{
        .mask = _published_shader_feature_mask.load(std::memory_order_acquire)};
}

void SceneManager::set_shader_feature_source(void const *source, uint64_t mask) {
    if (source == nullptr) return;
    std::lock_guard lock{_shader_feature_mtx};
    auto [iter, inserted] = _shader_feature_sources.try_emplace(source);
    auto &state = iter->second;
    if (!inserted && state.directly_active && state.mask == mask) return;
    auto const old_mask = state.mask;
    auto const old_active = state.active();
    state.mask = mask;
    state.directly_active = true;
    _apply_shader_feature_transition_locked(
        old_mask, old_active, state.mask, state.active());
}

void SceneManager::update_shader_feature_source(void const *source, uint64_t mask) {
    if (source == nullptr) return;
    std::lock_guard lock{_shader_feature_mtx};
    auto [iter, inserted] = _shader_feature_sources.try_emplace(source);
    auto &state = iter->second;
    if (!inserted && state.mask == mask) return;
    auto const old_mask = state.mask;
    auto const old_active = state.active();
    state.mask = mask;
    _apply_shader_feature_transition_locked(
        old_mask, old_active, state.mask, state.active());
}

void SceneManager::bind_shader_feature_source(void const *source, uint64_t mask) {
    if (source == nullptr) return;
    std::lock_guard lock{_shader_feature_mtx};
    auto [iter, inserted] = _shader_feature_sources.try_emplace(source);
    auto &state = iter->second;
    auto const old_mask = state.mask;
    auto const old_active = state.active();
    // Do not overwrite a newer material update with a stale mask sampled by
    // the caller before this lock was acquired.
    if (inserted) {
        state.mask = mask;
    }
    ++state.binding_count;
    _apply_shader_feature_transition_locked(
        old_mask, old_active, state.mask, state.active());
}

void SceneManager::unbind_shader_feature_source(void const *source) {
    if (source == nullptr) return;
    std::lock_guard lock{_shader_feature_mtx};
    auto iter = _shader_feature_sources.find(source);
    if (iter == _shader_feature_sources.end() ||
        iter->second.binding_count == 0u) return;
    auto const old_mask = iter->second.mask;
    auto const old_active = iter->second.active();
    --iter->second.binding_count;
    _apply_shader_feature_transition_locked(
        old_mask, old_active, iter->second.mask, iter->second.active());
}

void SceneManager::remove_shader_feature_source(void const *source) {
    if (source == nullptr) return;
    std::lock_guard lock{_shader_feature_mtx};
    auto iter = _shader_feature_sources.find(source);
    if (iter == _shader_feature_sources.end()) return;
    auto const old_mask = iter->second.mask;
    auto const old_active = iter->second.active();
    _shader_feature_sources.erase(iter);
    _apply_shader_feature_transition_locked(old_mask, old_active, 0u, false);
}

SceneManager::SceneManager(
    Context &ctx,
    Device &device, Stream &copy_stream,
    IOService &io_service,
    CommandList &cmdlist,
    luisa::filesystem::path const &shader_path)
    : _ctx(ctx),
      _device(device),
      _bf_alloc(device),
      _mesh_mng(device),
      _temp_buffer(vstd::make_unique<HostBufferManager>(device)),
      _uploader(),
      _bdls_mng(device),
      _mat_mng(device),
      _light_accel(device),
      _light_accel_event(luisa::fiber::event::Mode::Auto, false) {
    ShaderManager::create_instance(device, shader_path);
    _tex_streamer.create(
        device, copy_stream, io_service, cmdlist, _bdls_mng);
    set_instance(this);
}

luisa::span<luisa::unique_ptr<AccelManager> const> SceneManager::accel_managers() {
    LUISA_DEBUG_ASSERT(!_accel_mngs.empty());
    return _accel_mngs;
}

void SceneManager::refresh_pipeline(
    CommandList &cmdlist,
    Stream &stream,
    bool start_new_frame,
    bool sync) {
    before_rendering(cmdlist, stream);
    on_frame_end(cmdlist, stream);
    if (sync)
        stream << synchronize();
    if (start_new_frame)
        prepare_frame();
}
void SceneManager::sync_bindless_heap(CommandList &cmdlist, Stream &stream) {
    if (_bf_alloc.dirty()) {
        _bf_alloc.mark_clean();
        bindless_allocator().set_reserved_buffer(heap_indices::buffer_allocator_heap_index, _bf_alloc.buffer());
    }

    if (bindless_allocator().require_sync()) {
        LUISA_WARNING("Bindless resource update synchronize.");
        stream.synchronize();
    }
    bindless_allocator().commit(cmdlist);
}
bool SceneManager::accel_dirty() const {
    if (!_build_meshes.empty()) return true;
    for (auto &i : _accel_mngs) {
        if (i->dirty()) return true;
    }
    return false;
}
void SceneManager::execute_io(
    IOService *io_service,
    Stream &main_stream,
    uint64_t event_handle,
    uint64_t fence_index) {
    auto execute_io = [&](
                          IOCommandList &&cmdlist,
                          IOService *io_service) {
        if (cmdlist.empty()) return;
        uint64_t handle = invalid_resource_handle;
        uint64_t fence = 0;
        if (_io_cmdlist_require_sync) {
            _io_cmdlist_require_sync = false;
            handle = event_handle;
            fence = fence_index;
        }
        auto io_fence = io_service->execute(std::move(cmdlist), handle, fence);
        // support direct-storage
        if (io_fence > 0) [[likely]] {
            main_stream << io_service->wait(io_fence);
        }
    };
    execute_io(std::move(_frame_mem_io_list), io_service);
}
void SceneManager::build_mesh_in_frame(Mesh *mesh, RC<RCBase> &&mesh_rc) {
    std::lock_guard lck{_build_mesh_mtx};
    _build_meshes.try_emplace(mesh, std::move(mesh_rc));
}
AccelManager &SceneManager::accel_manager() {
    LUISA_DEBUG_ASSERT(!_accel_mngs.empty());
    return *_accel_mngs[0];
}
Accel &SceneManager::accel() {
    return _accel_mngs[0]->accel();
}
void SceneManager::before_rendering(
    CommandList &cmdlist,
    Stream &stream) {
    _build_mesh_mtx.lock();
    auto build_meshes = std::move(_build_meshes);
    _build_mesh_mtx.unlock();
    for (auto &i : build_meshes) {
        auto build = [&](auto &&mesh) {
            if (mesh) {
                cmdlist << mesh.build();
            }
            for (auto &i : _accel_mngs) {
                if (i->mesh_instance_size() > 0)
                    i->mark_dirty();
            }
        };
        build(*i.first);
    }
    build_meshes.clear();
    {
        light_accel().reserve_tlas();
        light_accel().update_tlas(cmdlist, dispose_queue());
        luisa::fiber::schedule([this]() {
            light_accel().build_tlas();
            _light_accel_event.signal();
        });
    }

    {
        std::lock_guard lck{_evt_mtx};
        for (auto &i : _before_render_evts) {
            i.second->scene_manager_tick();
        }
    }
    _tex_streamer->before_rendering(
        stream,
        host_upload_buffer(),
        cmdlist);
    for (auto &i : _accel_mngs) {
        i->build_accel(cmdlist);
    }
    _uploader.commit(cmdlist, *_temp_buffer);
    sync_bindless_heap(cmdlist, stream);
    _mesh_mng.execute_compute_bounding(
        cmdlist,
        _bdls_mng.buffer_heap(),
        *_temp_buffer,
        dispose_queue());
}
void SceneManager::add_before_render_event(vstd::string_view name, SceneManagerEvent *func) {
    std::lock_guard lck{_evt_mtx};
    _before_render_evts.try_emplace(name, func);
}
void SceneManager::add_on_frame_end_event(vstd::string_view name, SceneManagerEvent *func) {
    std::lock_guard lck{_evt_mtx};
    _before_render_evts.try_emplace(name, func);
}
void SceneManager::prepare_frame() {
    if (!_temp_buffer) {
        if (auto new_temp_buffer = _temp_buffers.pop()) {
            _temp_buffer = std::move(*new_temp_buffer);
        } else {
            _temp_buffer = vstd::make_unique<HostBufferManager>(_device);
        }
    }
}
bool SceneManager::on_frame_end(
    CommandList &cmdlist,
    Stream &stream,
    ManagedDevice *managed_device) {
    _light_accel_event.wait();
    {
        std::lock_guard lck{_evt_mtx};
        for (auto &i : _on_frame_end_evts) {
            i.second->scene_manager_tick();
        }
    }
    // mesh manager dispose data
    _mesh_mng.on_frame_end(&cmdlist, _bdls_mng.alloc());

    // store transform in accel
    for (auto &i : _accel_mngs)
        i->update_last_transform(cmdlist);
    if (_temp_buffer) {
        _temp_buffer->flush();
        // cleanup temp buffer after rendering done
        cmdlist.add_callback([t = std::move(_temp_buffer), this]() mutable {
            t->clear();
            _temp_buffers.push(std::move(t));
        });
    }

    // create new temp buffer for next frame

    // push dispose queue callback
    _dsp_queue.on_frame_end(cmdlist);

    // dispose after commit
    auto disp = vstd::scope_exit([&]() {
        _after_commit_dsp_queue.force_clear();
    });

    // commit command list
    if (cmdlist.empty()) return false;
    if (managed_device) {
        managed_device->dispatch(stream.handle(), std::move(cmdlist));
        cmdlist.clear();
    } else {
        
        stream << cmdlist.commit();
    }
    return true;
}
void SceneManager::load_shader(luisa::fiber::counter &init_counter) {
    _mesh_mng.load_shader(init_counter);
    // default scene
    if (_accel_mngs.empty())
        _accel_mngs.emplace_back(luisa::make_unique<AccelManager>(_device));
    for (auto &i : _accel_mngs)
        i->load_shader(init_counter);
    _uploader.load_shader(init_counter);
    _tex_uploader.load_shader(init_counter);
    _light_accel.load_shader(init_counter);
    _skinning.load_shader(init_counter);
}
namespace scene_mng_detail {
static SceneManager *_inst = nullptr;
}// namespace scene_mng_detail
SceneManager &SceneManager::instance() {
    return *scene_mng_detail::_inst;
}
SceneManager *SceneManager::instance_ptr() {
    return scene_mng_detail::_inst;
}
void SceneManager::set_instance(SceneManager *scene_manager) {
    LUISA_ASSERT(scene_manager != nullptr, "Scene manager must not be nullptr");
    scene_mng_detail::_inst = scene_manager;
}
SceneManager::~SceneManager() {
    _tex_streamer.destroy();
    ShaderManager::destroy_instance();
    if (scene_mng_detail::_inst == this) {
        scene_mng_detail::_inst = nullptr;
    }
}
void SceneManager::remove_before_render_event(vstd::string_view name) {
    std::lock_guard lck{_evt_mtx};
    _before_render_evts.remove(name);
}
void SceneManager::remove_on_frame_end_event(vstd::string_view name) {
    std::lock_guard lck{_evt_mtx};
    _on_frame_end_evts.remove(name);
}
}// namespace rbc
