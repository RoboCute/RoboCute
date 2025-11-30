#pragma once
#include <rbc_config.h>
#include <luisa/runtime/sparse_command_list.h>
#include <luisa/vstl/common.h>
#include <luisa/vstl/functional.h>
#include <luisa/vstl/spin_mutex.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/dispose_queue.h>
#include <rbc_graphics/device_assets/device_resource.h>
#include <rbc_io/io_service.h>
#include <rbc_graphics/host_buffer_manager.h>
#include <rbc_graphics/buffer_uploader.h>
#include <rbc_graphics/texture_uploader.h>
namespace rbc
{
struct DeviceResource;
struct SceneManager;
struct DisposeQueue;
struct LoadTaskArgs {
    DisposeQueue* disp_queue;
    BufferUploader* buffer_uploader;
    HostBufferManager* temp_buffer;
    Device& device;
    IOCommandList& io_cmdlist;
    IOCommandList& mem_io_cmdlist;
    CommandList& cmdlist;
    uint64_t load_frame;
};
struct RBC_RUNTIME_API AssetsManager : vstd::IOperatorNewBase {
public:
    static constexpr uint FRAME_COUNT = 4;
    template <typename T>
    struct CallQueue {
        friend struct AssetsManager;

    public:
        using FuncVectorType = vstd::vector<vstd::function<T>>;

    private:
        FuncVectorType _funcs;
        vstd::spin_mutex _mtx;
        RBC_RUNTIME_API vstd::vector<vstd::function<T>> _get_funcs(vstd::vector<vstd::function<T>>&& new_vec) &&;
        RBC_RUNTIME_API CallQueue();
        RBC_RUNTIME_API ~CallQueue();

    public:
        void push(vstd::function<T>&& func)
        {
            std::lock_guard lck{ _mtx };
            _funcs.emplace_back(std::move(func));
        }
    };

    CallQueue<void()> render_thd_queue;
    using LoadTaskType = void(LoadTaskArgs const&);
    CallQueue<LoadTaskType> load_thd_queue;
    static AssetsManager* instance();
    static void init_instance(RenderDevice& render_device, SceneManager* scene_mng);
    static void destroy_instance();
    AssetsManager(RenderDevice& render_device, SceneManager* scene_mng);
    void wake_load_thread(uint64_t allowed_max_delay_frame = FRAME_COUNT);
    void execute_render_thread();
    ~AssetsManager();

private:
    struct AsyncFrameResource {
        std::atomic_uint64_t disk_io_fence = 0;
        std::atomic_uint64_t mem_io_fence = 0;
        std::atomic_uint64_t compute_fence = 0;
        vstd::optional<HostBufferManager> temp_buffer;
    };
    AsyncFrameResource _async_frame_res[FRAME_COUNT];
    RenderDevice& _render_device;
    // where load thread start commiting
    std::atomic_uint64_t _load_thd_frame_index = 0;
    // where load thread finish commit, start executing
    std::atomic_uint64_t _load_executive_thd_frame_index = 0;
    // where load thread finish executing.
    std::atomic_uint64_t _finished_frame_index = 0;
    luisa::fiber::mutex _load_thd_mtx;
    luisa::fiber::condition_variable _load_thd_cv;
    luisa::fiber::mutex _load_executive_thd_mtx;
    luisa::fiber::condition_variable _load_executive_thd_cv;
    std::thread _load_thd;
    std::thread _load_executive_thd;
    vstd::LockFreeArrayQueue<vstd::function<void()>> _load_executive_queue;
    DisposeQueue _load_stream_disqueue;
    DisposeQueue _render_stream_disqueue;
    TextureUploader _load_tex_uploader;
    BufferUploader _load_buffer_uploader;
    vstd::spin_mutex _disqueue_mtx;
    luisa::filesystem::path _assets_root_path;
    SceneManager* _scene_mng;
    luisa::filesystem::path _get_asset_path(luisa::filesystem::path const& p);
    luisa::filesystem::path _get_read_path(luisa::filesystem::path const& p);

public:
    void set_root_path(luisa::filesystem::path assets_root_path);

    [[nodiscard]] auto const& root_path() const { return _assets_root_path; }
    [[nodiscard]] uint64 load_started_index() const { return _load_thd_frame_index.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64 load_executed_index() const { return _load_executive_thd_frame_index.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64 load_finished_index() const { return _finished_frame_index.load(std::memory_order_relaxed); }
    [[nodiscard]] auto& lc_device() const { return _render_device.lc_device(); }
    [[nodiscard]] auto& scene_mng() const { return _scene_mng; }
    [[nodiscard]] auto& render_device() const { return _render_device; }
    [[nodiscard]] auto& render_stream_disqueue() { return _render_stream_disqueue; }
    [[nodiscard]] auto& load_tex_uploader() { return _load_tex_uploader; }
    [[nodiscard]] auto& load_stream_disqueue() { return _load_stream_disqueue; }
    template <typename T>
    requires(!std::is_reference_v<T> && std::is_move_constructible_v<T> && !std::is_trivially_destructible_v<T>)
    void dispose_after_render_frame(T&& t)
    {
        _render_stream_disqueue.dispose_after_queue(std::move(t), _disqueue_mtx);
    }
};
} // namespace rbc