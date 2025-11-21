#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/stl/algorithm.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc
{
template <typename T>
vstd::vector<vstd::function<T>> AssetsManager::CallQueue<T>::_get_funcs(vstd::vector<vstd::function<T>>&& new_vec) &&
{
    {
        std::lock_guard lck{ _mtx };
        luisa::swap(new_vec, _funcs);
    }
    return std::move(new_vec);
}

template <typename T>
AssetsManager::CallQueue<T>::CallQueue() = default;
template <typename T>
AssetsManager::CallQueue<T>::~CallQueue() = default;
namespace asset_mng_detail
{
vstd::unique_ptr<AssetsManager> _inst{};
template <typename T>
void atomic_max(std::atomic<T>& a, T b)
{
    uint64 prev_value = a;
    while (prev_value < b && !a.compare_exchange_weak(prev_value, b))
    {
        std::this_thread::yield();
    }
};
struct FinishCallback : vstd::IOperatorNewBase {
    std::atomic_uint64_t* finish_frame;
    uint64_t frame;
    using Vec = vstd::vector<vstd::function<AssetsManager::LoadTaskType>>;
    Vec callbacks;
    FinishCallback(
        std::atomic_uint64_t* finish_frame,
        uint64_t frame,
        Vec&& vec
    )
        : finish_frame(finish_frame)
        , frame(frame)
        , callbacks(std::move(vec))
    {
    }

    ~FinishCallback()
    {
        atomic_max(*finish_frame, frame);
    }
};
}; // namespace asset_mng_detail
AssetsManager* AssetsManager::instance()
{
    return asset_mng_detail::_inst.get();
}
void AssetsManager::init_instance(RenderDevice& render_device, SceneManager* scene_mng)
{
    if (!asset_mng_detail::_inst)
    {
        asset_mng_detail::_inst = vstd::make_unique<AssetsManager>(render_device, scene_mng);
        luisa::fiber::counter counter;
        asset_mng_detail::_inst->_load_buffer_uploader.load_shader(counter);
        asset_mng_detail::_inst->_load_tex_uploader.load_shader(counter);
        counter.wait();
    }
}
void AssetsManager::destroy_instance()
{
    if (asset_mng_detail::_inst)
    {
        asset_mng_detail::_inst.reset();
    }
}
void AssetsManager::set_root_path(luisa::filesystem::path assets_root_path)
{
    _assets_root_path = assets_root_path.lexically_normal().make_preferred();
    std::error_code ec;
    _assets_root_path = luisa::filesystem::canonical(_assets_root_path, ec);
    if (ec)
    {
        LUISA_ERROR("Illegal root path {} with message {}", luisa::to_string(assets_root_path), ec.message());
    }
}

AssetsManager::AssetsManager(RenderDevice& render_device, SceneManager* scene_mng)
    : _render_device(render_device)
{
    _scene_mng = scene_mng;
    for (auto& i : _async_frame_res)
    {
        i.temp_buffer.create(render_device.lc_device());
    }
    _load_thd = std::thread([this]() {
        uint64_t executed_frame = 0;
        auto sync_frame_res = [&](AsyncFrameResource& frame_res) {
            auto wait_executive_thread = [&](auto& v) {
                while (v == std::numeric_limits<uint64_t>::max())
                {
                    std::this_thread::yield();
                }
            };
            wait_executive_thread(frame_res.disk_io_fence);
            if (frame_res.disk_io_fence != 0)
            {
                _render_device.io_service()->synchronize(frame_res.disk_io_fence);
            }
            wait_executive_thread(frame_res.mem_io_fence);
            if (frame_res.mem_io_fence != 0)
            {
                _render_device.mem_io_service()->synchronize(frame_res.mem_io_fence);
            }
            wait_executive_thread(frame_res.compute_fence);
            if (frame_res.compute_fence != 0)
            {
                _render_device.lc_async_timeline().synchronize(frame_res.compute_fence);
            }
            frame_res.disk_io_fence = 0;
            frame_res.mem_io_fence = 0;
            frame_res.compute_fence = 0;
            frame_res.temp_buffer->clear();
        };
        while (true)
        {
            {
                luisa::fiber::lock lck(_load_thd_mtx);
                _load_thd_cv.wait(lck, [&]() {
                    auto idx = _load_thd_frame_index.load();
                    return executed_frame < idx || idx == std::numeric_limits<uint64_t>::max();
                });
            }
            if (_load_thd_frame_index == std::numeric_limits<uint64_t>::max())
            {
                break;
            }
            auto& frame_res = _async_frame_res[executed_frame % FRAME_COUNT];
            executed_frame++;
            sync_frame_res(frame_res);
            IOCommandList io_cmdlist, mem_io_cmdlist;
            CommandList cmdlist;

            decltype(load_thd_queue)::FuncVectorType vec;
            vec.reserve(16);
            auto funcs = std::move(load_thd_queue)._get_funcs(std::move(vec));
            if (funcs.empty())
            {
                continue;
            }
            LoadTaskArgs load_task{
                &_load_stream_disqueue,
                &_load_buffer_uploader,
                frame_res.temp_buffer.ptr(),
                _render_device.lc_device(),
                io_cmdlist,
                mem_io_cmdlist,
                cmdlist,
                executed_frame
            };
            for (auto& i : funcs)
            {
                i(load_task);
            }
            auto finish_callbak = luisa::make_shared<asset_mng_detail::FinishCallback>(&_finished_frame_index, executed_frame, std::move(funcs));
            _load_buffer_uploader.commit(cmdlist, *frame_res.temp_buffer);
            _load_stream_disqueue.on_frame_end(cmdlist);
            _scene_mng->mesh_manager().execute_build_cmds(cmdlist, _scene_mng->bindless_allocator(), *frame_res.temp_buffer);
            auto set_cmdlist = [&](auto& cmdlist, auto& value) {
                value = cmdlist.empty() ? 0ull : std::numeric_limits<uint64_t>::max();
            };
            set_cmdlist(io_cmdlist, frame_res.disk_io_fence);
            set_cmdlist(mem_io_cmdlist, frame_res.mem_io_fence);
            set_cmdlist(cmdlist, frame_res.compute_fence);
            _load_executive_queue.push([this,
                                        &frame_res,
                                        executed_frame,
                                        mem_io_cmdlist = std::move(mem_io_cmdlist),
                                        io_cmdlist = std::move(io_cmdlist),
                                        cmdlist = std::move(cmdlist),
                                        finish_callbak = std::move(finish_callbak)] mutable {
                if (!io_cmdlist.empty())
                {
                    if (cmdlist.empty())
                    {
                        io_cmdlist.add_callback([finish_callbak]() {});
                    }
                    frame_res.disk_io_fence = _render_device.io_service()->execute(std::move(io_cmdlist));
                }

                if (!mem_io_cmdlist.empty())
                {
                    if (cmdlist.empty())
                    {
                        mem_io_cmdlist.add_callback([finish_callbak]() {});
                    }
                    frame_res.mem_io_fence = _render_device.mem_io_service()->execute(std::move(mem_io_cmdlist));
                }
                if (!cmdlist.empty())
                {
                    cmdlist.add_callback([finish_callbak]() {});
                    if (frame_res.disk_io_fence > 0)
                    {
                        // _render_device.io_service()->synchronize(frame_res.disk_io_fence);
                        _render_device.lc_async_stream() << _render_device.io_service()->wait(frame_res.disk_io_fence);
                    }
                    if (frame_res.mem_io_fence > 0)
                    {
                        // _render_device.mem_io_service()->synchronize(frame_res.mem_io_fence);
                        _render_device.lc_async_stream() << _render_device.mem_io_service()->wait(frame_res.mem_io_fence);
                    }
                    _render_device.lc_async_stream() << cmdlist.commit() << _render_device.lc_async_timeline().signal(executed_frame);
                    // _render_device.lc_async_stream() << synchronize();
                    frame_res.compute_fence = executed_frame;
                }
            });
            _load_executive_thd_mtx.lock();
            ++_load_executive_thd_frame_index;
            _load_executive_thd_mtx.unlock();
            _load_executive_thd_cv.notify_all();
        }
        _load_executive_thd_cv.notify_all();
        _load_executive_thd.join();
        while (auto p = _load_executive_queue.pop())
        {
            (*p)();
        }
        for (auto& frame_res : _async_frame_res)
        {
            sync_frame_res(frame_res);
        }
    });
    _load_executive_thd = std::thread([this] {
        uint64_t executed_frame = 0;
        while (true)
        {
            {
                luisa::fiber::lock lck(_load_executive_thd_mtx);
                _load_executive_thd_cv.wait(lck, [&]() {
                    auto idx = _load_executive_thd_frame_index.load();
                    return executed_frame < idx || idx == std::numeric_limits<uint64_t>::max();
                });
            }
            if (_load_executive_thd_frame_index == std::numeric_limits<uint64_t>::max())
            {
                break;
            }
            ++executed_frame;
            while (auto p = _load_executive_queue.pop())
            {
                (*p)();
            }
        }
    });
}

void AssetsManager::wake_load_thread(uint64_t allowed_max_delay_frame)
{
    {
        std::lock_guard lck{ load_thd_queue._mtx };
        if (load_thd_queue._funcs.empty())
            return;
    }
    if (_load_thd_frame_index.load() - _finished_frame_index.load() >= allowed_max_delay_frame) return;
    {
        std::lock_guard lck(_load_thd_mtx);
        _load_thd_frame_index++;
    }
    _load_thd_cv.notify_one();
}
void AssetsManager::execute_render_thread()
{
    decltype(render_thd_queue)::FuncVectorType vec;
    vec.reserve(16);
    auto funcs = std::move(render_thd_queue)._get_funcs(std::move(vec));
    for (auto& i : funcs)
    {
        i();
    }
    {
        std::lock_guard lck{ _disqueue_mtx };
        _render_stream_disqueue.on_frame_end(RenderDevice::instance().lc_main_cmd_list());
    }
}

AssetsManager::~AssetsManager()
{
    {
        std::lock_guard lck(_load_thd_mtx);
        std::lock_guard lck1(_load_executive_thd_mtx);
        _load_thd_frame_index = std::numeric_limits<uint64_t>::max();
        _load_executive_thd_frame_index = std::numeric_limits<uint64_t>::max();
    }
    _load_thd_cv.notify_one();
    _load_thd.join();
    _render_stream_disqueue.force_clear();
    _load_stream_disqueue.force_clear();
}
luisa::filesystem::path AssetsManager::_get_asset_path(luisa::filesystem::path const& p)
{
    if (p.is_absolute())
    {
        return luisa::filesystem::relative(p, _assets_root_path).make_preferred().lexically_normal();
    }
    auto v = p.lexically_normal();
    v.make_preferred();
    return v;
}

luisa::filesystem::path AssetsManager::_get_read_path(luisa::filesystem::path const& p)
{
    return _assets_root_path / p;
}
} // namespace rbc