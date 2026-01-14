#include <rbc_graphics/scene_manager.h>
#include <luisa/core/logging.h>
#include <luisa/runtime/context.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_graphics/managed_device.h>
namespace rbc {
SceneManager::SceneManager(
    Context &ctx,
    Device &device, Stream &copy_stream,
    IOService &io_service,
    CommandList &cmdlist,
    luisa::filesystem::path const &shader_path)
    : _ctx(ctx),
      _device(device),
      _mesh_mng(device),
      _temp_buffer(vstd::make_unique<HostBufferManager>(device)),
      _bdls_mng(device),
      _uploader(),
      _bf_alloc(device),
      _mat_mng(device),
      _light_accel(device),
      light_accel_event(luisa::fiber::event::Mode::Auto, false) {
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
            light_accel_event.signal();
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
    _before_render_evts.try_emplace(name, std::move(func));
}
void SceneManager::add_on_frame_end_event(vstd::string_view name, SceneManagerEvent *func) {
    std::lock_guard lck{_evt_mtx};
    _before_render_evts.try_emplace(name, std::move(func));
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
    light_accel_event.wait();
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