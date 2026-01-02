#pragma once
#include <rbc_config.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/event.h>
#include <luisa/vstl/lockfree_array_queue.h>
#include <rbc_io/io_command_list.h>
#include <rbc_io/io_service.h>
#include <rbc_graphics/buffer_allocator.h>
#include <rbc_graphics/bindless_manager.h>
#include <rbc_graphics/buffer_uploader.h>
#include "shader_manager.h"
#include "texture_uploader.h"
#include "texture/tex_stream_manager.h"
#include "accel_manager.h"
#include "mat_manager.h"
#include "light_accel.h"
#include "skinning.h"
namespace rbc {
struct ManagedDevice;
struct PipelineCtxMutable;
struct SceneManagerEvent {
    virtual void scene_manager_tick() = 0;
};
struct RBC_RUNTIME_API SceneManager {
private:
    // [DROP]
    Context &_ctx;
    Device &_device;

    // [MAIN]
    BufferAllocator _bf_alloc;// 分页器

    // [MAIN]
    DisposeQueue _dsp_queue;
    DisposeQueue _after_commit_dsp_queue;

    // [SINGLETON]
    MeshManager _mesh_mng;

    // [SINGLETON] 临时 Buffer 分配器
    vstd::LockFreeArrayQueue<vstd::unique_ptr<HostBufferManager>> _temp_buffers;
    vstd::unique_ptr<HostBufferManager> _temp_buffer;

    // [SINGLETON]

    // [SINGLETON]
    BindlessManager _bdls_mng;

    // [SINGLETON]
    BufferUploader _uploader;// 命令打包器

    // [SINGLETON]
    luisa::vector<luisa::unique_ptr<AccelManager>> _accel_mngs;
    Skinning _skinning;
    MatManager _mat_mng;
    LightAccel _light_accel;
    TextureUploader _tex_uploader;
    vstd::optional<TexStreamManager> _tex_streamer;

    luisa::spin_mutex _evt_mtx;
    vstd::HashMap<vstd::string, SceneManagerEvent *> _before_render_evts;
    vstd::HashMap<vstd::string, SceneManagerEvent *> _on_frame_end_evts;
    luisa::fiber::event light_accel_event;
    IOCommandList _frame_mem_io_list;
    bool _io_cmdlist_require_sync : 1 {false};
    luisa::spin_mutex _build_mesh_mtx;
    luisa::unordered_map<Mesh *, RC<RCBase>> _build_meshes;

public:
    ///////////// properties
    bool accel_dirty() const;
    [[nodiscard]] auto &tex_streamer() { return *_tex_streamer; }
    [[nodiscard]] auto &ctx() const { return _ctx; }
    [[nodiscard]] auto &device() const { return _device; }
    [[nodiscard]] auto &dispose_queue() { return _dsp_queue; }
    [[nodiscard]] auto &after_commit_dsp_queue() { return _after_commit_dsp_queue; }
    [[nodiscard]] auto const &bindless_manager() const { return _bdls_mng; }
    [[nodiscard]] auto &bindless_allocator() { return _bdls_mng.alloc(); }
    [[nodiscard]] auto const &buffer_allocator() const { return _bf_alloc; }
    [[nodiscard]] auto &bindless_manager() { return _bdls_mng; }
    [[nodiscard]] auto &buffer_allocator() { return _bf_alloc; }
    [[nodiscard]] AccelManager &accel_manager();
    [[nodiscard]] auto &mat_manager() { return _mat_mng; }
    [[nodiscard]] auto &host_upload_buffer() { return *_temp_buffer; }
    [[nodiscard]] auto &buffer_uploader() { return _uploader; }
    [[nodiscard]] auto &frame_mem_io_list() { return _frame_mem_io_list; }
    void set_io_cmdlist_require_sync() { _io_cmdlist_require_sync = true; }
    [[nodiscard]] auto const &buffer_heap() const { return _bdls_mng.buffer_heap(); }
    [[nodiscard]] auto &buffer_heap() { return _bdls_mng.buffer_heap(); }
    [[nodiscard]] auto const &image_heap() const { return _bdls_mng.image_heap(); }
    [[nodiscard]] auto &image_heap() { return _bdls_mng.image_heap(); }
    [[nodiscard]] auto const &volume_heap() const { return _bdls_mng.volume_heap(); }
    [[nodiscard]] auto &volume_heap() { return _bdls_mng.volume_heap(); }
    [[nodiscard]] Accel &accel();
    [[nodiscard]] auto &mesh_manager() { return _mesh_mng; }
    [[nodiscard]] auto &light_accel() { return _light_accel; }
    [[nodiscard]] auto &tex_uploader() { return _tex_uploader; }
    static SceneManager &instance();
    static SceneManager *instance_ptr();
    static void set_instance(SceneManager *scene_manager);

    ///////////// properties
    SceneManager(Context &ctx,
                 Device &device,
                 Stream &copy_stream,
                 IOService &io_service,
                 CommandList &cmdlist,
                 luisa::filesystem::path const &shader_path);
    void build_mesh_in_frame(Mesh *mesh, RC<RCBase> &&mesh_rc);
    void add_before_render_event(vstd::string_view name, SceneManagerEvent *func);
    void add_on_frame_end_event(vstd::string_view name, SceneManagerEvent *func);
    void remove_before_render_event(vstd::string_view name);
    void remove_on_frame_end_event(vstd::string_view name);
    void load_shader(luisa::fiber::counter &init_counter);
    void prepare_frame();
    void execute_io(
        IOService *io_service,
        Stream &main_stream,
        uint64_t event_handle,
        uint64_t fence_index);
    void before_rendering(CommandList &cmdlist, Stream &stream);
    void sync_bindless_heap(CommandList &cmdlist, Stream &stream);
    bool on_frame_end(
        CommandList &cmdlist,
        Stream &stream,
        ManagedDevice *managed_device = nullptr);
    SceneManager(SceneManager const &) = delete;
    SceneManager(SceneManager &&) = delete;
    ~SceneManager();
    // should be called during window resize
    void refresh_pipeline(
        CommandList &cmdlist,
        Stream &stream,
        bool start_new_frame,
        bool sync);
    template<typename T>
        requires(!std::is_reference_v<T> && std::is_move_constructible_v<T> && !std::is_trivially_destructible_v<T>)
    void dispose_after_commit(T &&t) {
        _after_commit_dsp_queue.dispose_after_queue(std::forward<T>(t));
    }
    template<typename T>
        requires(!std::is_reference_v<T> && std::is_move_constructible_v<T> && !std::is_trivially_destructible_v<T>)
    void dispose_after_sync(T &&t) {
        _dsp_queue.dispose_after_queue(std::forward<T>(t));
    }
};
}// namespace rbc