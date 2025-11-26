#pragma once
#include "generated/generated.hpp"
#include <rbc_graphics/render_device.h>
#include <rbc_ipc/message_manager.h>
namespace rbc {
struct RPCHook {
    SharedWindow shared_window;
    luisa::unique_ptr<ipc::IMessageReceiver> receiver;
    luisa::unique_ptr<ipc::IMessageSender> sender;

    std::mutex mtx;
    luisa::BinaryBlob execute_blob;
    luisa::BinaryBlob return_blob;

    std::thread thd;
    std::atomic_bool enabled{true};
    RPCHook(RenderDevice &render_device, Stream &present_stream);
    ~RPCHook();
    bool update();
    void shutdown_remote();
private:
    void _thread();
};
}// namespace rbc