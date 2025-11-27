#include <rbc_ipc/rpc_server.h>
#include <luisa/core/stl/memory.h>

namespace rbc {
RPCServer::RPCServer(
    luisa::unique_ptr<ipc::IMessageReceiver> &&receiver,
    luisa::unique_ptr<ipc::IMessageSender> &&sender,
    uint64_t async_queue_count)
    : receiver(std::move(receiver)),
      sender(std::move(sender)) {
    LUISA_ASSERT(async_queue_count < 128);
    _threads_count = async_queue_count;
    _threads = luisa::allocate_with_allocator<ThreadData>(_threads_count);
    for (auto i : vstd::range(_threads_count)) {
        std::construct_at(_threads + i);
    }
    receiver_poper.reset(this->receiver.get());
}
RPCServer::~RPCServer() {
    std::destroy(_threads, _threads + _threads_count);
    luisa::deallocate_with_allocator(_threads);
}
bool RPCServer::tick_push() {
    send_data.clear();
    auto push = [&](void *ptr, size_t size) {
        auto i = send_data.size();
        send_data.push_back_uninitialized(size);
        std::memcpy(send_data.data() + i, ptr, size);
    };
    auto push_blob = [&](uint8_t id, luisa::BinaryBlob &blb) {
        ipc::TypedHeader header{
            .custom_id = id,
            .size = (uint)blb.size()};
        push(&header, sizeof(header));
        push(blb.data(), blb.size());
    };
    for (auto i : vstd::range(_threads_count)) {
        auto &thd = _threads[i];
        thd.push_mtx.lock();
        auto return_values = std::move(thd.return_values);
        auto commits = std::move(thd.commits);
        thd.push_mtx.unlock();
        auto id = get_return_id(i);
        for (auto &blb : return_values) {
            push_blob(id, blb);
        }
        id = get_arg_id(i);
        for (auto &blb : commits) {
            push_blob(id, blb);
        }
    }
    luisa::span send_data_span{send_data};
    while (!send_data_span.empty()) {
        auto size = std::min<uint64_t>(send_data_span.size(), sender->max_size());
        if (!sender->push(send_data_span.subspan(0, size))) return false;
        send_data_span = send_data_span.subspan(size, send_data_span.size() - size);
    }
    return true;
}

void RPCServer::tick_pop() {
    if (receiver_poper.next_step()) {
        return;
    }
    auto id = receiver_poper.id();
    auto data = receiver_poper.steal_data();
    receiver_poper.reset(receiver.get());
    auto &thd = _threads[id / 2];
    // is argument
    if ((id & 1) == 0) {
        thd.tasks.push(std::move(data));
    }
    // is return value
    else {
        RPCCommandList::readback(
            [&](uint64_t handle) {
                std::lock_guard lck{thd.map_mtx};
                auto iter = thd.return_futures.find(handle);
                LUISA_DEBUG_ASSERT(iter != thd.return_futures.end());
                auto v = std::move(iter->second);
                thd.handles.push_back(iter->first);
                thd.return_futures.erase(iter);
                return v;
            },
            data);
        // auto iter= return_futures.find()
    }
}

void RPCServer::execute_remote(uint thread_id) {
    auto &thd = _threads[thread_id];
    while (auto task = thd.tasks.pop()) {
        // LUISA_INFO("get arg {}", luisa::string_view{(char const*)task->data(), task->size()});
        auto return_blob = RPCCommandList::server_execute(
            ~0ull,
            *task);
        if (!return_blob.empty()) {
            // LUISA_INFO("get return {}", luisa::string_view{(char const*)return_blob.data(), return_blob.size()});
            thd.push_mtx.lock();
            thd.return_values.emplace_back(std::move(return_blob));
            thd.push_mtx.unlock();
        }
    }
}
RPCCommandList RPCServer::create_cmdlist(uint thread_id) {
    auto &thd = _threads[thread_id];
    uint64_t handle;
    thd.map_mtx.lock();
    if (thd.handles.empty()) {
        handle = thd.handle_count++;
    } else {
        handle = thd.handles.back();
        thd.handles.pop_back();
    }
    thd.map_mtx.unlock();
    return {this, thread_id, handle};
}
void RPCServer::commit(RPCCommandList &&cmdlist) {
    auto &thd = _threads[cmdlist.thread_id()];
    auto cmd = cmdlist.commit_commands();
    auto id = cmdlist.id();
    if (cmd.ret_values_handle) {
        thd.map_mtx.lock();
        thd.return_futures.force_emplace(id, std::move(cmd.ret_values_handle));
        thd.map_mtx.unlock();
    } else {
        thd.map_mtx.lock();
        thd.handles.emplace_back(id);
        thd.map_mtx.unlock();
    }
    thd.push_mtx.lock();
    thd.commits.emplace_back(std::move(cmd.arguments));
    thd.push_mtx.unlock();
}
}// namespace rbc