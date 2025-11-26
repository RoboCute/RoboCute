#include "rbc_ipc/message_manager.h"
#include <luisa/core/logging.h>

namespace rbc::ipc {

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

bool IMessageSender::push(uint8_t custom_id, luisa::span<const std::byte> data) {
    TypedHeader header{
        custom_id,
        (uint32_t)data.size()};
    if (!push(luisa::span{
            reinterpret_cast<std::byte const *>(&header),
            sizeof(header)})) return false;
    return push(data);
}

void AsyncTypedMessagePop::reset(IMessageReceiver *self) {
    this->self = self;
    header = {};
    buf = {};
    buf_data = {};
    buf_end = {};
    step = 0;
}

bool AsyncTypedMessagePop::next_step() {
    auto &_cache = self->_cache;
    auto push_to_cache = [&](size_t size) {
        auto sz = _cache.size();
        _cache.push_back_uninitialized(size);
        LUISA_DEBUG_ASSERT(size <= (buf_end - buf_data), "push cache overflow");
        std::memcpy(_cache.data() + sz, buf_data, size);
        buf_data += size;
    };
    auto push_to_data = [&](size_t size) {
        auto sz = data.size();
        data.push_back_uninitialized(size);
        LUISA_DEBUG_ASSERT(size <= (buf_end - buf_data), "push data overflow");
        std::memcpy(data.data() + sz, buf_data, size);
        buf_data += size;
    };

#define GOTO_STEP(i) \
    case i: goto STEP_##i
    switch (step) {
        GOTO_STEP(0);
        GOTO_STEP(1);
        GOTO_STEP(2);
        GOTO_STEP(3);
        case ~0u: return false;
        default: goto FINAL_STEP;
    }
STEP_0:
    // first step:read cache
    data.clear();
    self->_mtx.lock();
    if (_cache.size() >= sizeof(TypedHeader)) {
        buf_data = _cache.data();
        buf_end = buf_data + _cache.size();
        std::memcpy(&header, _cache.data(), sizeof(header));
        buf_data += sizeof(header);
        if ((buf_end - buf_data) > header.size) {
            push_to_data(header.size);
            auto lefted_size = buf_end - buf_data;
            std::memmove(_cache.data(), buf_data, lefted_size);
            _cache.resize_uninitialized(lefted_size);
            step = ~0u;
            goto FINAL_STEP;
        } else if ((buf_end - buf_data) == header.size) {
            push_to_data(header.size);
            _cache.clear();
            step = ~0u;
            goto FINAL_STEP;
        }
        push_to_data(buf_end - buf_data);
        step += 2;
        return true;
    }
    step += 1;
    return true;
STEP_1: {
    buf = self->try_pop();
    buf_data = buf.data();
    buf_end = buf_data + buf.size();
    auto required_size = std::min<size_t>((buf_end - buf_data), sizeof(TypedHeader) - _cache.size());
    size_t cache_idx = _cache.size();
    push_to_cache(required_size);
    if (_cache.size() >= sizeof(TypedHeader)) {
        std::memcpy(&header, _cache.data(), sizeof(header));
        _cache.clear();
        push_to_data(std::min<size_t>(buf_end - buf_data, header.size - data.size()));
        // already done, push last to cache
        if (buf_data < buf_end) {
            push_to_cache(buf_end - buf_data);
            step = ~0u;
            goto FINAL_STEP;
        }
        step += 1;
        return true;
    }
}
    return true;
    // clear cache
STEP_2: {
    _cache.clear();
    if (data.size() >= header.size) {
        step = ~0u;
        goto FINAL_STEP;
    }
    step += 1;
}
    return true;
// read data
STEP_3: {
    buf = self->try_pop();
    buf_data = buf.data();
    buf_end = buf_data + buf.size();
    push_to_data(std::min<size_t>(buf_end - buf_data, header.size - data.size()));
    if (buf_data < buf_end) {
        push_to_cache(buf_end - buf_data);
        step += 1;
        return true;
    }
    if (data.size() == header.size) {
        step += 1;
        return true;
    }
}
    return true;
FINAL_STEP:
    self->_mtx.unlock();
    return false;
}

uint8_t IMessageReceiver::pop_typed_message(luisa::vector<std::byte> &data) {
    data.clear();
    std::lock_guard lck{_mtx};
    std::byte *buf_data{};
    std::byte *buf_end{};
    auto push_to_cache = [&](size_t size) {
        auto sz = _cache.size();
        _cache.push_back_uninitialized(size);
        LUISA_DEBUG_ASSERT(size <= (buf_end - buf_data), "push cache overflow");
        std::memcpy(_cache.data() + sz, buf_data, size);
        buf_data += size;
    };
    auto push_to_data = [&](size_t size) {
        auto sz = data.size();
        data.push_back_uninitialized(size);
        LUISA_DEBUG_ASSERT(size <= (buf_end - buf_data), "push data overflow");
        std::memcpy(data.data() + sz, buf_data, size);
        buf_data += size;
    };
    TypedHeader header;
    if (_cache.size() >= sizeof(TypedHeader)) {
        buf_data = _cache.data();
        buf_end = buf_data + _cache.size();
        std::memcpy(&header, _cache.data(), sizeof(header));
        buf_data += sizeof(header);
        if ((buf_end - buf_data) > header.size) {
            push_to_data(header.size);
            auto lefted_size = buf_end - buf_data;
            std::memmove(_cache.data(), buf_data, lefted_size);
            _cache.resize_uninitialized(lefted_size);
            return header.custom_id;
        } else if ((buf_end - buf_data) == header.size) {
            push_to_data(header.size);
            _cache.clear();
            return header.custom_id;
        }
        push_to_data(buf_end - buf_data);
    } else {
        while (true) {
            auto buf = try_pop();
            buf_data = buf.data();
            buf_end = buf_data + buf.size();
            auto required_size = std::min<size_t>((buf_end - buf_data), sizeof(TypedHeader) - _cache.size());
            size_t cache_idx = _cache.size();
            push_to_cache(required_size);
            if (_cache.size() >= sizeof(TypedHeader)) {
                std::memcpy(&header, _cache.data(), sizeof(header));
                _cache.clear();
                push_to_data(std::min<size_t>(buf_end - buf_data, header.size - data.size()));
                // already done, push last to cache
                if (buf_data < buf_end) {
                    push_to_cache(buf_end - buf_data);
                    return header.custom_id;
                }
                break;
            }
            std::this_thread::yield();
        }
    }
    _cache.clear();
    if (data.size() >= header.size) return header.custom_id;
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
        std::this_thread::yield();
    }
    return header.custom_id;
}

}// namespace rbc::ipc