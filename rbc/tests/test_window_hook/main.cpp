#include <rbc_core/rpc/command_list.h>
#include <rbc_ipc/message_manager.h>
#include <luisa/gui/window.h>
#include <luisa/core/logging.h>
#include "generated/client.hpp"
using namespace luisa;
using namespace luisa::compute;
using namespace rbc;
int main() {
    // LUISA_INFO("Hook");
    auto sender = ipc::IMessageSender::create(
        "test_window_hook",
        ipc::EMessageQueueBackend::IPC);
    auto receiver = ipc::IMessageReceiver::create(
        "test_graphics",
        ipc::EMessageQueueBackend::IPC);
    // get handle
    uint32_t entry_code = 114514;
    uint32_t shutdown_code = 1919810;
    while (!sender->push(0, {reinterpret_cast<std::byte *>(&entry_code), sizeof(entry_code)})) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    luisa::vector<std::byte> data;
    uint64_t handle{};
    while (true) {
        auto data = receiver->try_pop();
        if (data.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        LUISA_ASSERT(data.size() == sizeof(handle));
        std::memcpy(&handle, data.data(), data.size());
        break;
    }
    LUISA_INFO("Handle received {}", handle);
    const uint2 resolution{1024};
    Window window("Hooked window", resolution);
    RPCCommandList cmdlist;
    LUISA_INFO("Sending window handle {}", window.native_handle());
    SharedWindowClient::open_window(cmdlist, (void *)handle, window.native_handle());
    auto open_commands = cmdlist.commit_commands();

    while (!sender->push(1, open_commands)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    LUISA_INFO("Pushed");

    while (true) {
        auto data = receiver->try_pop();
        if (data.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        break;
    }
    LUISA_INFO("Window opened.");
    ipc::AsyncTypedMessagePop poper;
    poper.reset(receiver.get());
    bool shutdown{false};
    while (!window.should_close() && !shutdown) {
        window.poll_events();
        // TODO: may need frame-sync
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!poper.next_step()) {
            auto id = poper.id();
            auto data = poper.get_data();
            poper.reset(receiver.get());
            if (data.size() != sizeof(uint32_t) || (*(uint32_t *)data.data()) != shutdown_code) {
                LUISA_ERROR("Password error.");
            }
            LUISA_INFO("Shotdown by remote.");
            shutdown = true;
        }
    }

    // exit
    if (!shutdown)
        sender->push(255, {reinterpret_cast<std::byte *>(&shutdown_code), sizeof(shutdown_code)});
}