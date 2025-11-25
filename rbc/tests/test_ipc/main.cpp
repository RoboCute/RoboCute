#include <rbc_ipc/message_manager.h>
#include <luisa/core/logging.h>
using namespace rbc;
using namespace luisa;
int main(int argc, char *argv[]) {
    luisa::vector<std::byte> receive_data;
    bool send_first = argc > 1;
    const char *a = "rbc_test_ipc_a";
    const char *b = "rbc_test_ipc_b";
    auto sender = IMessageSender::create(send_first ? a : b);
    auto receiver = IMessageReceiver::create(send_first ? b : a);

    if (send_first) {
        LUISA_INFO("Start sending first message");
        luisa::string_view msg = "HAHAHAHAHAHA first message";
        while (!sender->push(
            {reinterpret_cast<std::byte const *>(msg.data()),
             msg.size()})) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    LUISA_INFO("Start receiving first message");
    while (!receiver->try_pop(receive_data)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LUISA_INFO("Received first message: {}",
               luisa::string_view{
                   reinterpret_cast<char const *>(receive_data.data()),
                   receive_data.size()});

    luisa::string_view msg = "HAHAHAHAHAHA second message";
    LUISA_INFO("Start sending second message");
    while (!sender->push(
        {reinterpret_cast<std::byte const *>(msg.data()),
         msg.size()})) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!send_first) {
        LUISA_INFO("Start receiving second message");
        while (!receiver->try_pop(receive_data)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        LUISA_INFO("Received second message: {}",
                   luisa::string_view{
                       reinterpret_cast<char const *>(receive_data.data()),
                       receive_data.size()});
    }
    return 0;
}