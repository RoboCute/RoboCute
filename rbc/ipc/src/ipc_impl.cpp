#include "rbc_ipc/message_manager.h"
// TODO: may disable cpp-ipc on sone platform
#define USE_CPP_IPC
#ifdef USE_CPP_IPC
#include "libipc/ipc.h"

namespace rbc::ipc {
using ipc_channel = ::ipc::chan<::ipc::relat::single, ::ipc::relat::single, ::ipc::trans::unicast>;

struct IPCMessageSender : IMessageSender {
    std::atomic_bool first_time{true};
    ipc_channel cc;
    std::mutex mtx;
    size_t _max_size;
    IPCMessageSender(char const *topic, size_t max_size)
        : cc(topic, ::ipc::sender), _max_size(max_size) {
    }
    uint64_t max_size() const override { return _max_size; }
    bool push(const luisa::span<const std::byte> data) override {
        std::lock_guard lck(mtx);
        if (first_time) {
            cc.wait_for_recv(1);
            first_time = false;
        }
        return cc.send(data.data(), data.size());
    }
};

struct IPCMessageReceiver : IMessageReceiver {
    ipc_channel cc;
    size_t _max_size;
    IPCMessageReceiver(char const *topic, size_t max_size)
        : cc(topic, ::ipc::receiver), _max_size(max_size) {
    }
    uint64_t max_size() const override { return _max_size; }
    luisa::BinaryBlob try_pop() override {
        auto buf = cc.try_recv();
        if (buf.empty())
            return {};
        return {
            reinterpret_cast<std::byte *>(buf.data()),
            buf.size(),
            [buf = std::move(buf)](void *ptr) {}};
    }
};
IMessageSender *create_sender_ipc_impl(char const *topic, size_t max_size) {
    return new IPCMessageSender{topic, max_size};
}
IMessageReceiver *create_receiver_ipc_impl(char const *topic, size_t max_size) {
    return new IPCMessageReceiver{topic, max_size};
}
}// namespace rbc::ipc
#else
namespace rbc::ipc {
IMessageSender *create_sender_ipc_impl(char const *topic, size_t max_size) {
    return nullptr;
}
IMessageReceiver *create_receiver_ipc_impl(char const *topic, size_t max_size) {
    return nullptr;
}
}// namespace rbc::ipc
#endif