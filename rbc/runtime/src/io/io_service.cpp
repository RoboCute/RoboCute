#include <rbc_io//io_service.h>
#include "dx12_dstorage/dstorage_config.h"
#include <luisa/core/stl/algorithm.h>
#include <luisa/backends/ext/pinned_memory_ext.hpp>
#include <luisa/core/binary_file_stream.h>
#include <luisa/vstl/functional.h>
// use 32m staging
namespace rbc {
TimelineEvent::Wait IOService::wait(uint64_t timeline) const {
    while (!timeline_signaled(timeline)) {
        std::this_thread::yield();
    }
    return {_evt.handle(), timeline};
}
void IOService::synchronize(uint64_t timeline) const {
    dstorage_stream->sync_event(device, _evt.handle(), _evt.native_handle(), timeline);
}

namespace ioservice_detail {
struct CallbackThread {
    vstd::LockFreeArrayQueue<vstd::function<void()>> callbacks;
    vstd::LockFreeArrayQueue<std::pair<IOService *, bool>> task_queue;
    vstd::vector<IOService *> all_service;
    std::atomic_bool _enabled = true;
    std::thread _thd;
    void execute() {
        while (auto p = task_queue.pop()) {
            if (p->second) {
                auto idx = all_service.size();
                auto &v = all_service.emplace_back(std::move(p->first));
                v->_self_idx = idx;
            } else {
                auto self_idx = p->first->_self_idx;
                p->first->_join();
                if (self_idx != all_service.size() - 1) {
                    all_service[self_idx] = std::move(all_service.back());
                    all_service[self_idx]->_self_idx = self_idx;
                }
                all_service.pop_back();
                p->first->~IOService();
                vengine_free(p->first);
            }
        }
        while (auto c = callbacks.pop()) {
            (*c)();
        }
        for (auto &i : all_service) {
            i->_tick();
        }
    }
    CallbackThread()
        : _thd([this]() {
              while (_enabled) {
                  execute();
                  std::this_thread::sleep_for(std::chrono::milliseconds(1));
              }
              while (auto c = callbacks.pop()) {
                  (*c)();
              }
              while (auto p = task_queue.pop()) {
                  if (p->second) {
                      p->first->~IOService();
                      vengine_free(p->first);
                  }
              }
              for (auto &i : all_service) {
                  i->_join();
                  i->~IOService();
                  vengine_free(i);
              }
              while (auto c = callbacks.pop()) {
                  (*c)();
              }
          }) {
    }
    ~CallbackThread() {
        _enabled = false;
        _thd.join();
    }
};
vstd::unique_ptr<CallbackThread> _thds;
}// namespace ioservice_detail
void IOService::add_callback(vstd::function<void()> &&callback) {
    ioservice_detail::_thds->callbacks.push(std::move(callback));
}
IOService *IOService::create_service(
    Device &device,
    DStorageSrcType src_type,
    QueueType queue_type) {
    // TODO: other platforms' implementation
    auto new_ser = new (vengine_malloc(sizeof(IOService))) IOService{queue_type, device, src_type};
    ioservice_detail::_thds->task_queue.push(new_ser, true);
    return new_ser;
}
void IOService::dispose_service(IOService *ser) {
    ioservice_detail::_thds->task_queue.push(ser, false);
}
static IOService::QueueType _io_service_queue_type{};
void IOService::init(
    QueueType queue_type,
    luisa::filesystem::path const &runtime_dir,
    bool force_hdd) {
    _io_service_queue_type = queue_type;
    switch (queue_type) {
        case QueueType::DX12:
            DStorageStream::init_dx12(runtime_dir, force_hdd);
            break;
        default:
            DStorageStream::init_fallback();
            break;
    }
    ioservice_detail::_thds = vstd::make_unique<ioservice_detail::CallbackThread>();
}
bool IOService::timeline_signaled(uint64_t timeline) const {
    return dstorage_stream->timeline_signaled(timeline);
}
void IOService::_join() {
    auto lock_page = [&]() {
        std::lock_guard lck{_callback_mtx};
        return _callbacks.pop();
    };
    while (auto p = lock_page()) {
        dstorage_stream->sync_event(device, _evt.handle(), _evt.native_handle(), p->timeline);
        _clear_res(*p);
    }
    dstorage_stream->free_queue();
}

void IOService::dispose() {
    ioservice_detail::_thds = nullptr;
    switch (_io_service_queue_type) {
        case QueueType::DX12:
            DStorageStream::dispose_dx12();
            break;
        default:
            break;
    }
}
void IOService::_tick() {
    // {
    //     auto cmd = [&]() {
    //         std::lock_guard lck{ _cmd_mtx };
    //         return _cmds.pop();
    //     }();
    //     if (cmd)
    //     {
    //         _execute_cmdlist(cmd->first, cmd->second);
    //     }
    // }
    std::lock_guard lck{_callback_mtx};
    if (auto v = _callbacks.front()) {
        if (dstorage_stream->is_event_complete(
                device,
                _evt.handle(),
                _evt.native_handle(),
                v->timeline)) {
            _clear_res(*v);
            _callbacks.pop_discard();
        }
    }
}
auto IOService::queue_type() -> QueueType {
    return _io_service_queue_type;
}
IOService::IOService(QueueType queue_type, Device &device, DStorageSrcType src_type)
    : _evt(device.create_timeline_event()) {
    if (queue_type == QueueType::Default)
        queue_type = _io_service_queue_type;
    switch (queue_type) {
        case QueueType::DX12:
            dstorage_stream = DStorageStream::create_dx12(device, src_type);
            break;
        default:
            dstorage_stream = DStorageStream::create_fallback(device, src_type);
            break;
    }
    this->device = device.impl();
}
void IOService::_clear_res(Callbacks &r) {
    r.files.clear();
    for (auto &i : r.callbacks) {
        i();
    }
    r.callbacks.clear();
}
IOService::~IOService() {
    dstorage_stream->dispose();
}

void IOService::_execute_cmdlist(IOCommandList &cmd, uint64_t timeline, uint64_t wait_on_event_handle, uint64_t wait_on_fence_index) {
    extra_cmds.clear();
    auto &&cmds = std::move(cmd).steal_commands();
    auto &&files = std::move(cmd).steal_files();
    auto &&callbacks = std::move(cmd).steal_callbacks();
    bool require_signal = false;
    auto add_callback = vstd::scope_exit([&]() {
        cmds.clear();
        if (require_signal) {
            dstorage_stream->enqueue_signal(_evt.handle(), _evt.native_handle(), timeline);
            dstorage_stream->submit();
        }
        if (files.empty() && callbacks.empty()) return;
        std::lock_guard lck{_callback_mtx};
        _callbacks.push(timeline, std::move(files), std::move(callbacks));
    });

    if (cmds.empty()) {
        return;
    }
    require_signal = true;
    if (wait_on_event_handle != invalid_resource_handle && wait_on_fence_index > 0) {
        if (!dstorage_stream->support_wait()) [[unlikely]] {
            LUISA_ERROR("Direct-storage not support wait.");
        }
        dstorage_stream->enqueue_wait(wait_on_event_handle, wait_on_fence_index);
    }

    _fragment_buffer.clear();
    _staging_buffer.clear();
    auto staging_size = dstorage_stream->staging_size();
    split_commands(cmds, extra_cmds, staging_size);
    auto iter_cmd_loop = [&](auto &i) {
        auto sz = i.size_bytes();
        if (sz < staging_size) {
            _fragment_buffer.emplace_back(&i, sz);
        } else {
            _staging_buffer.emplace_back(&i, sz);
        }
    };
    for (auto &i : cmds) {
        iter_cmd_loop(i);
    }
    for (auto &i : extra_cmds) {
        iter_cmd_loop(i);
    }
    luisa::sort(_fragment_buffer.begin(), _fragment_buffer.end(), [&](auto &&a, auto &&b) {
        return a.second < b.second;
    });
    // TODO: clean
    bool require_execute = false;
    auto execute_stream = [&]() {
        if (!require_execute) return;
        require_execute = false;
        dstorage_stream->submit();
    };
    if (!_fragment_buffer.empty()) {
        size_t byte_offset = 0;

        for (auto &pair : _fragment_buffer) {
            auto cmd = pair.first;
            if (pair.second > staging_size - byte_offset) {
                execute_stream();
                byte_offset = 0;
            }
            require_execute = true;
            auto const &file = cmd->file_data;
            luisa::visit([&](auto &&src) {
                luisa::visit([&]<typename T>(T const &dst) {
                    if constexpr (std::is_same_v<T, IOBufferSubView>) {
                        dstorage_stream->enqueue_request(
                            src,
                            cmd->offset_bytes,
                            dst.handle,
                            dst.native_handle,
                            dst.offset_bytes,
                            dst.size_bytes);
                    } else if constexpr (std::is_same_v<T, vstd::span<std::byte>>) {
                        dstorage_stream->enqueue_request(
                            src,
                            cmd->offset_bytes,
                            dst.data(),
                            dst.size_bytes());
                    } else {
                        dstorage_stream->enqueue_request(
                            src,
                            cmd->offset_bytes,
                            dst.handle,
                            dst.native_handle,
                            dst.storage,
                            uint3(dst.offset[0], dst.offset[1], dst.offset[2]),
                            uint3(dst.size[0], dst.size[1], dst.size[2]),
                            dst.level);
                    }
                },
                             cmd->dst);
            },
                         file);

            byte_offset += pair.second;
        }
    }
    if (!_staging_buffer.empty()) {
        for (auto &pair : _staging_buffer) {
            execute_stream();
            require_execute = true;
            auto cmd = pair.first;
            luisa::visit([&](auto &&src) {
                luisa::visit([&]<typename T>(T const &dst) {
                    if constexpr (std::is_same_v<T, IOBufferSubView>) {
                        dstorage_stream->enqueue_request(
                            src,
                            cmd->offset_bytes,
                            dst.handle,
                            dst.native_handle,
                            dst.offset_bytes,
                            dst.size_bytes);
                    } else if constexpr (std::is_same_v<T, vstd::span<std::byte>>) {
                        dstorage_stream->enqueue_request(
                            src,
                            cmd->offset_bytes,
                            dst.data(),
                            dst.size_bytes());
                    } else {
                        dstorage_stream->enqueue_request(
                            src,
                            cmd->offset_bytes,
                            dst.handle,
                            dst.native_handle,
                            dst.storage,
                            uint3(dst.offset[0], dst.offset[1], dst.offset[2]),
                            uint3(dst.size[0], dst.size[1], dst.size[2]),
                            dst.level);
                    }
                },
                             cmd->dst);
            },
                         cmd->file_data);
        }
    }
}

void IOService::split_commands(vector<IOCommand> &commands, vector<IOCommand> &extra_commands, uint64_t staging_size) {
    size_t cmd_count = commands.size();
    for (size_t i = 0; i < cmd_count; ++i) {
        auto &cmd_ref = commands[i];
        size_t size = cmd_ref.size_bytes();
        if (size <= staging_size)
            continue;
        auto cmd = std::move(cmd_ref);
        if (i < cmd_count - 1) [[likely]] {
            cmd_ref = std::move(commands.back());
        }
        commands.pop_back();
        i--;
        cmd_count--;
        if (size == 0) [[unlikely]]
            continue;
        luisa::visit(
            [&]<typename T>(T &t) {
                if constexpr (std::is_same_v<T, IOBufferSubView>) {
                    while (true) {
                        size_t sub_buffer_size = std::min<size_t>(t.size_bytes, staging_size);
                        auto sub_buffer = t;
                        sub_buffer.size_bytes = sub_buffer_size;
                        extra_commands.emplace_back(cmd.file_data, cmd.offset_bytes, std::move(sub_buffer));
                        if (t.size_bytes - sub_buffer_size > 0) {
                            cmd.offset_bytes += sub_buffer_size;
                            t.offset_bytes += sub_buffer_size;
                            t.size_bytes -= sub_buffer_size;
                        } else {
                            break;
                        }
                    }
                } else if constexpr (std::is_same_v<T, vstd::span<std::byte>>) {
                    while (true) {
                        size_t sub_buffer_size = std::min<size_t>(t.size_bytes(), staging_size);
                        auto sub_buffer = t.subspan(0, sub_buffer_size);
                        extra_commands.emplace_back(cmd.file_data, cmd.offset_bytes, sub_buffer);
                        if (t.size_bytes() - sub_buffer_size > 0) {
                            cmd.offset_bytes += sub_buffer_size;
                            t = t.subspan(sub_buffer_size);
                        } else {
                            break;
                        }
                    }
                } else {
                    size_t volume_size = pixel_storage_size(t.storage, uint3(t.size[0], t.size[1], t.size[2]));
                    if (volume_size < staging_size) {
                        extra_commands.emplace_back(
                            cmd.file_data, cmd.offset_bytes,
                            IOTextureSubView{
                                t.handle,
                                t.native_handle,
                                {t.offset[0], t.offset[1], t.offset[2]},
                                t.storage,
                                {t.size[0], t.size[1], t.size[2]},
                                t.level});
                        return;
                    }
                    // every plane:
                    size_t plane_size = pixel_storage_size(t.storage, uint3(t.size[0], t.size[1], 1));
                    if (plane_size <= staging_size) {
                        auto copy_plane_size = std::min<uint>(staging_size / plane_size, t.size[2]);
                        for (uint depth = 0; depth < t.size[2]; depth += copy_plane_size) {
                            auto copy_size = std::min<uint>(copy_plane_size, t.size[2] - depth);
                            extra_commands.emplace_back(
                                cmd.file_data, cmd.offset_bytes,
                                IOTextureSubView{
                                    t.handle,
                                    t.native_handle,
                                    {t.offset[0], t.offset[1], t.offset[2] + uint(depth)},
                                    t.storage,
                                    {t.size[0], t.size[1], copy_size},
                                    t.level});
                            cmd.offset_bytes += copy_size * plane_size;
                        }
                        return;
                    }
                    uint block_height = t.size[1];
                    bool is_bc = is_block_compressed(t.storage);
                    if (is_bc) {
                        block_height /= 4;
                    }
                    size_t row_size = plane_size / block_height;
                    size_t copy_col_size = staging_size / row_size;
                    if (is_bc) {
                        copy_col_size *= 4;
                    }
                    for (uint depth : vstd::range(t.size[2]))
                        for (uint col = 0; col < t.size[1]; col += copy_col_size) {
                            auto copy_size = std::min<uint>(copy_col_size, t.size[1] - col);
                            extra_commands.emplace_back(
                                cmd.file_data, cmd.offset_bytes,
                                IOTextureSubView{
                                    t.handle,
                                    t.native_handle,
                                    {t.offset[0], t.offset[1] + col, t.offset[2] + depth},
                                    t.storage,
                                    {t.size[0], copy_size, 1u},
                                    t.level});
                            cmd.offset_bytes += pixel_storage_size(t.storage, uint3(t.size[0], copy_size, 1));
                        }
                }
            },
            cmd.dst);
    }
}
uint64_t IOService::execute(IOCommandList &&cmdlist, uint64_t wait_on_event_handle, uint64_t wait_on_fence_index) {
    uint64_t timeline;
    {
        std::lock_guard lck{_cmd_mtx};
        if (!cmdlist.commands().empty()) {
            timeline = ++_timeline;
        } else {
            timeline = _timeline;
        }
        // _cmds.push(std::move(cmdlist), timeline);
        _execute_cmdlist(cmdlist, timeline, wait_on_event_handle, wait_on_fence_index);
    }
    return timeline;
}
}// namespace rbc