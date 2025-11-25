#include "rbc_ipc/message_manager.h"
#include "libipc/ipc.h"

namespace rbc {
using ipc_channel = ipc::chan<ipc::relat::single, ipc::relat::single, ipc::trans::unicast>;

struct IPCMessageSender : rbc::IMessageSender {
    std::atomic_bool first_time{true};
    ipc_channel cc;
    std::mutex mtx;
    IPCMessageSender(char const *topic, size_t max_size)
        : cc(topic, ipc::sender) {
    }
    bool push(const luisa::span<const std::byte> data) override {
        std::lock_guard lck(mtx);
        if (first_time) {
            cc.wait_for_recv(1);
            first_time = false;
        }
        return cc.send(data.data(), data.size());
    }
};

struct IPCMessageReceiver : rbc::IMessageReceiver {
    ipc_channel cc;
    IPCMessageReceiver(char const *topic, size_t max_size)
        : cc(topic, ipc::receiver) {
    }
    bool try_pop(luisa::vector<std::byte> &data) override {
        auto buf = cc.try_recv();
        if (buf.empty())
            return false;
        data.clear();
        data.resize_uninitialized(buf.size());
        memcpy(data.data(), buf.data(), buf.size());
        return true;
    }
};
luisa::unique_ptr<IMessageSender> IMessageSender::create(char const *topic, size_t max_size) {
    return luisa::unique_ptr<IMessageSender>{new IPCMessageSender{topic, max_size}};
}
luisa::unique_ptr<IMessageReceiver> IMessageReceiver::create(char const *topic, size_t max_size) {
    return luisa::unique_ptr<IMessageReceiver>{new IPCMessageReceiver{topic, max_size}};
}
}// namespace rbc