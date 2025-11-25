#include "rbc_ipc/message_manager.h"
#include <luisa/core/logging.h>

namespace rbc::ipc {
struct TypedHeader {
    uint32_t custom_id : 8;
    uint32_t size : 24;
};
IMessageSender *create_sender_ipc_impl(char const *topic, size_t max_size);
IMessageReceiver *create_receiver_ipc_impl(char const *topic, size_t max_size);
luisa::unique_ptr<IMessageSender> IMessageSender::create(char const *topic, EMessageQueueBackend backend, size_t max_size) {
    switch (backend) {
        case EMessageQueueBackend::IPC:
            return luisa::unique_ptr<IMessageSender>{create_sender_ipc_impl(topic, max_size)};
        default:
            LUISA_ERROR("unsupported backend.");
    }
    vstd::unreachable();
    return {};
}
luisa::unique_ptr<IMessageReceiver> IMessageReceiver::create(char const *topic, EMessageQueueBackend backend, size_t max_size) {
    switch (backend) {
        case EMessageQueueBackend::IPC:
            return luisa::unique_ptr<IMessageReceiver>{create_receiver_ipc_impl(topic, max_size)};
        default:
            LUISA_ERROR("unsupported backend.");
    }
    vstd::unreachable();
    return {};
}

uint8_t IMessageReceiver::pop_typed_message(luisa::vector<std::byte> &data) {
    data.clear();
    std::lock_guard lck{_mtx};
    std::byte *buf_data{};
    std::byte *buf_end{};
    auto push_to_cache = [&](size_t size) {
        auto sz = _cache.size();
        _cache.push_back_uninitialized(size);
        std::memcpy(_cache.data() + sz, buf_data, size);
        buf_data += size;
    };
    auto push_to_data = [&](size_t size) {
        auto sz = data.size();
        data.push_back_uninitialized(size);
        std::memcpy(data.data() + sz, buf_data, size);
        buf_data += size;
    };
    TypedHeader header;
    while (true) {
        auto buf = try_pop();
        buf_data = buf.data();
        buf_end = buf_data + buf.size();
        auto required_size = std::min<size_t>((buf_end - buf_data), sizeof(TypedHeader) - _cache.size());
        size_t cache_idx = _cache.size();
        push_to_cache(required_size);
        if (_cache.size() >= sizeof(TypedHeader)) {
            std::memcpy(&header, _cache.data() + cache_idx, sizeof(header));
            push_to_data(std::min<size_t>(buf_end - buf_data, header.size - data.size()));
            // already done, push last to cache
            if (buf_data < buf_end) {
                push_to_cache(buf_end - buf_data);
                return header.custom_id;
            }
            break;
        }
    }
    while (true) {
        auto buf = try_pop();
        buf_data = buf.data();
        buf_end = buf_data + buf.size();
        push_to_data(std::min<size_t>(buf_end - buf_data, header.size - data.size()));
        if (buf_data < buf_end) {
            push_to_cache(buf_end - buf_data);
            break;
        }
        if (data.size() == header.size) {
            break;
        }
    }

    return header.custom_id;
}

}// namespace rbc::ipc