#include <rbc_graphics/scene_manager.h>
#include <luisa/core/logging.h>
#include <rbc_graphics/pipeline_context.h>
#include <luisa/runtime/context.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_graphics/managed_device.h>
namespace rbc
{
SceneManager::SceneManager(
    Context& ctx,
    Device& device, luisa::filesystem::path const& shader_path,
    bool use_lmdb
)
    : _ctx(ctx)
    , _device(device)
    , _mesh_mng(device)
    , _temp_buffer(vstd::make_unique<HostBufferManager>(device))
    , _bdls_mng(device)
    , _uploader()
    , _bf_alloc(device)
    , _accel_mng(device)
    , _mat_mng(device, true)
    , _light_accel(device)
{
    ShaderManager::create_instance(device, shader_path, use_lmdb);
    bindless_allocator().set_reserved_buffer(heap_indices::buffer_allocator_heap_index, _bf_alloc.buffer());
}

void SceneManager::refresh_pipeline(
    CommandList& cmdlist,
    Stream& stream
)
{
    before_rendering(cmdlist, stream);
    on_frame_end(cmdlist, stream);
    stream << synchronize();
}
void SceneManager::before_rendering(
    CommandList& cmdlist,
    Stream& stream
)
{
    {
        std::lock_guard lck{ _evt_mtx };
        for (auto& i : _before_render_evts)
        {
            i.second->scene_manager_tick();
        }
    }

    _accel_mng.build_accel(cmdlist);
    _uploader.commit(cmdlist, *_temp_buffer);
    if (_bf_alloc.dirty())
    {
        _bf_alloc.mark_clean();
        bindless_allocator().set_reserved_buffer(heap_indices::buffer_allocator_heap_index, _bf_alloc.buffer());
    }

    if (bindless_allocator().require_sync())
    {
        LUISA_WARNING("Bindless resource update synchronize.");
        stream.synchronize();
    }
    bindless_allocator().commit(cmdlist);
    _mesh_mng.execute_compute_bounding(
        cmdlist,
        _bdls_mng.buffer_heap(),
        *_temp_buffer,
        dispose_queue()
    );
}
void SceneManager::add_before_render_event(vstd::string_view name, SceneManagerEvent* func)
{
    std::lock_guard lck{ _evt_mtx };
    _before_render_evts.try_emplace(name, std::move(func));
}
void SceneManager::add_on_frame_end_event(vstd::string_view name, SceneManagerEvent* func)
{
    std::lock_guard lck{ _evt_mtx };
    _before_render_evts.try_emplace(name, std::move(func));
}
bool SceneManager::on_frame_end(
    CommandList& cmdlist,
    Stream& stream,
    ManagedDevice* managed_device
)
{
    {
        std::lock_guard lck{ _evt_mtx };
        for (auto& i : _on_frame_end_evts)
        {
            i.second->scene_manager_tick();
        }
    }
    // mesh manager dispose data
    _mesh_mng.on_frame_end(cmdlist, _bdls_mng.alloc());

    // store transform in accel
    _accel_mng.update_last_transform(cmdlist);

    // cleanup temp buffer after rendering done
    cmdlist.add_callback([t = std::move(_temp_buffer), this]() mutable {
        t->clear();
        _temp_buffers.push(std::move(t));
    });

    // create new temp buffer for next frame
    if (auto new_temp_buffer = _temp_buffers.pop())
    {
        _temp_buffer = std::move(*new_temp_buffer);
    }
    else
    {
        _temp_buffer = vstd::make_unique<HostBufferManager>(_device);
    }

    // push dispose queue callback
    _dsp_queue.on_frame_end(cmdlist);

    // dispose after commit
    auto disp = vstd::scope_exit([&]() {
        _after_commit_dsp_queue.force_clear();
        for (auto& i : _commit_callback)
        {
            i();
        }
        _commit_callback.clear();
    });

    // commit command list
    if (cmdlist.empty()) return false;
    _temp_buffer->flush();
    if (managed_device)
    {
        managed_device->dispatch(stream.handle(), std::move(cmdlist));
        cmdlist.clear();
    }
    else
    {
        stream << cmdlist.commit();
    }
    return true;
}
void SceneManager::load_shader()
{
    luisa::fiber::counter counter;
    _mesh_mng.load_shader(counter);
    _accel_mng.load_shader(counter);
    _uploader.load_shader(counter);
    _tex_uploader.load_shader(counter);
    _light_accel.load_shader(counter);
    counter.wait();
}

void SceneManager::add_after_commit_callback(vstd::function<void()>&& callback)
{
    _commit_callback.emplace_back(std::move(callback));
}
namespace scene_mng_detail
{
static SceneManager* _inst = nullptr;
}
SceneManager& SceneManager::instance()
{
    return *scene_mng_detail::_inst;
}
SceneManager* SceneManager::instance_ptr()
{
    return scene_mng_detail::_inst;
}
void SceneManager::set_instance(SceneManager* scene_manager)
{
    LUISA_ASSERT(scene_manager != nullptr, "Scene manager must not be nullptr");
    scene_mng_detail::_inst = scene_manager;
}
SceneManager::~SceneManager()
{
    ShaderManager::destroy_instance();
    if (scene_mng_detail::_inst == this)
    {
        scene_mng_detail::_inst = nullptr;
    }
}
void SceneManager::remove_before_render_event(vstd::string_view name)
{
    std::lock_guard lck{ _evt_mtx };
    _before_render_evts.remove(name);
}
void SceneManager::remove_on_frame_end_event(vstd::string_view name)
{
    std::lock_guard lck{ _evt_mtx };
    _on_frame_end_evts.remove(name);
}
} // namespace rbc