#pragma once
#include <rbc_config.h>
#include <rbc_ipc/command_list.h>
#include <rbc_ipc/message_manager.h>
#include <luisa/vstl/lockfree_array_queue.h>
namespace rbc {
struct RPCCommandList;
struct RBC_IPC_API RPCServer {
    friend struct RPCCommandList;
    RPCServer(
        luisa::unique_ptr<ipc::IMessageReceiver> &&receiver,
        luisa::unique_ptr<ipc::IMessageSender> &&sender,
        uint64_t async_queue_count);
    ~RPCServer();
    bool tick_push();
    void tick_pop();
    void execute_remote(uint thread_id);
    RPCCommandList create_cmdlist(uint thread_id);
    void commit(RPCCommandList &&cmdlist);

private:
    struct ThreadData {
        // pop
        vstd::LockFreeArrayQueue<luisa::vector<std::byte>> tasks;
        luisa::spin_mutex map_mtx;
        luisa::unordered_map<uint64_t, RC<RPCRetValueBase>> return_futures;
        uint64_t handle_count{};
        luisa::vector<uint64_t> handles;
        // push
        luisa::spin_mutex push_mtx;
        luisa::vector<luisa::BinaryBlob> commits;
        luisa::vector<luisa::BinaryBlob> return_values;
    };
    ThreadData *_threads;
    uint64_t _threads_count;
    // server
    luisa::vector<std::byte> send_data;
    // ipc
    luisa::unique_ptr<ipc::IMessageReceiver> receiver;
    luisa::unique_ptr<ipc::IMessageSender> sender;
    ipc::AsyncTypedMessagePop receiver_poper;
    static auto get_return_id(uint thread_id) { return thread_id * 2 + 1; }
    static auto get_arg_id(uint thread_id) { return thread_id * 2; }
};
}// namespace rbc