#include <rbc_ipc/message_manager.h>
#include <rbc_ipc/rpc_server.h>
#include <luisa/core/logging.h>
#include "generated/server.hpp"
#include "generated/client.hpp"
#include <iostream>
using namespace rbc;
using namespace luisa;
std::atomic_bool enabled{true};
void Chat::exit() {
    enabled = false;
}
luisa::string Chat::chat(luisa::string value) {
    LUISA_INFO("Sending message {}", value);
    return luisa::format("Received message {} with size {}", value, value.size());
}

int main(int argc, char *argv[]) {
    LUISA_INFO("Start, input strings");
    luisa::vector<std::byte> receive_data;
    bool is_server = argc > 1;
    auto server_name = "rbc_test_ipc_s";
    auto client_name = "rbc_test_ipc";
    auto sender = ipc::IMessageSender::create(is_server ? server_name : client_name, ipc::EMessageQueueBackend::IPC);
    auto receiver = ipc::IMessageReceiver::create(is_server ? client_name : server_name, ipc::EMessageQueueBackend::IPC);
    RPCServer service(
        std::move(receiver),
        std::move(sender),
        1);
    std::thread thd{[&] {
        auto d = vstd::scope_exit([&] {
            exit(0);
        });
        while (enabled) {
            service.tick_pop();
            if (!service.tick_push()) {
                LUISA_INFO("push failed.");
                enabled = false;
                return;
            }
            service.execute_remote(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        service.tick_push();
    }};
    Chat chat;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    while (enabled) {
        luisa::string s;
        std::cin >> s;
        auto cmdlist = service.create_cmdlist(0);
        if (s == "exit") {
            {
                ChatClient::exit(cmdlist, &chat);
                service.commit(std::move(cmdlist));
            }
            enabled = false;
            break;
        } else {
            auto future = ChatClient::chat(cmdlist, &chat, s);
            service.commit(std::move(cmdlist));
            auto str = future.take();
            LUISA_INFO("{}", str);
        }
    }
    thd.join();
    // wait last ipc finished
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 0;
}