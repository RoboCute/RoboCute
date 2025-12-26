#pragma once
#include <rbc_config.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/event.h>
#include <luisa/runtime/buffer.h>
#include <luisa/vstl/lockfree_array_queue.h>
#include "io_command_list.h"
#include "dstorage_interface.h"
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
namespace ioservice_detail {
struct CallbackThread;
}// namespace ioservice_detail
struct RBC_RUNTIME_API IOService {
    enum struct QueueType {
        Default,
        DX12,
        Fallback
    };
private:
    friend struct ioservice_detail::CallbackThread;
    uint64_t _self_idx;

private:
    ~IOService();
    DeviceInterface *device;
    struct Callbacks {
        uint64_t timeline{0};
        vstd::vector<IOFile> files;
        vstd::vector<vstd::function<void()>> callbacks;
    };
    DStorageStream *dstorage_stream;
    TimelineEvent _evt;
    vector<IOCommand> extra_cmds;
    uint _res_index{0};
    void split_commands(vector<IOCommand> &commands, vector<IOCommand> &extra_commands, uint64_t staging_size);
    // vstd::SingleThreadArrayQueue<std::pair<IOCommandList, uint64_t>> _cmds;
    vstd::spin_mutex _cmd_mtx;
    //////// async area
    vector<std::pair<IOCommand const *, uint64_t>> _fragment_buffer;
    vector<std::pair<IOCommand const *, uint64_t>> _staging_buffer;
    luisa::spin_mutex _callback_mtx;
    vstd::SingleThreadArrayQueue<Callbacks> _callbacks;
    uint64_t _timeline{0};
    void _join();
    void _tick();
    void _execute_cmdlist(IOCommandList &cmd, uint64_t timeline, uint64_t wait_on_event_handle, uint64_t wait_on_fence_index);
    void _clear_res(Callbacks &r);
    IOService(
        QueueType queue_type,
        Device &device,
        DStorageSrcType src_type);

public:

    static QueueType queue_type();
    static void add_callback(vstd::function<void()> &&callback);
    static void init(
        QueueType queue_type,
        luisa::filesystem::path const &runtime_dir,
        bool force_hdd);
    static void dispose();
    [[nodiscard]] TimelineEvent::Wait wait(uint64_t timeline) const;
    void synchronize(uint64_t timeline) const;
    static IOService *create_service(
        Device &device,
        DStorageSrcType src_type,
        QueueType queue_type = QueueType::Default);
    static void dispose_service(IOService *);
    uint64_t execute(IOCommandList &&cmdlist, uint64_t wait_on_event_handle = invalid_resource_handle, uint64_t wait_on_fence_index = 0);
    bool timeline_signaled(uint64_t timeline) const;
};

}// namespace rbc