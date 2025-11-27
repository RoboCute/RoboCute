#include <rbc_ipc/message_manager.h>
#include <luisa/core/logging.h>
using namespace rbc;
using namespace luisa;
int main(int argc, char *argv[]) {
    LUISA_INFO("Start");
    luisa::vector<std::byte> receive_data;
    bool send_first = argc > 1;
    if (send_first) {
        auto sender = ipc::IMessageSender::create("rbc_test_ipc", ipc::EMessageQueueBackend::IPC);
        luisa::vector<std::byte> data;
        auto push_str = [&](uint8_t id, luisa::string_view str) {
            ipc::TypedHeader header{
                .custom_id = id,
                .size = (uint32_t)str.size()};
            vstd::push_back_all(data, (std::byte *)&header, sizeof(header));
            vstd::push_back_all(data, (std::byte *)str.data(), str.size());
        };
        auto send = [&](uint start, uint end) {
            while (!sender->push(luisa::span{data}.subspan(start, end - start))) {
                //    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };
        push_str(5, "The first sentence");
        push_str(1, "The second sentence");
        push_str(2, "The third sentence");
        push_str(3, "The last sentence");
        for (int i = 1; i < 5; ++i) {
            auto cut = data.size() * ((float)i / 5.0);
            send(0, cut);
            send(cut, data.size());
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));

    } else {
        auto receiver = ipc::IMessageReceiver::create("rbc_test_ipc", ipc::EMessageQueueBackend::IPC);
        uint8_t id;
        auto async_popper = ipc::AsyncTypedMessagePop{};
        auto print = [&]() {
            async_popper.reset(receiver.get());
            while(async_popper.next_step()) {
            }
            id = async_popper.id();
            auto data = async_popper.get_data();
            LUISA_INFO("ID {} Data {}", id, luisa::string_view{(char const *)data.data(), data.size()});
        };
        for (int i = 1; i < 2; ++i) {
            print();
            print();
            print();
            print();
        }
    }
    //TODO: new ipc test wip
    return 0;
}