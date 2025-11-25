#include <rbc_ipc/message_manager.h>
#include <luisa/core/logging.h>
using namespace rbc;
using namespace luisa;
int main(int argc, char *argv[]) {
    luisa::vector<std::byte> receive_data;
    bool send_first = argc > 1;
    const char *a = "rbc_test_ipc_a";
    const char *b = "rbc_test_ipc_b";
    auto sender = ipc::IMessageSender::create(send_first ? a : b, ipc::EMessageQueueBackend::IPC);
    auto receiver = ipc::IMessageReceiver::create(send_first ? b : a, ipc::EMessageQueueBackend::IPC);
    //TODO: new ipc test wip
    return 0;
}