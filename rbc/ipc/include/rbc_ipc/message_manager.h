#pragma once
#include <luisa/vstl/hash_map.h>
#include <luisa/core/stl/string.h>
#include <luisa/core/stl/memory.h>
#include <luisa/core/stl/vector.h>

namespace rbc {
enum class EMessageQueueBackend {
    IPC
};

struct IMessageSender : vstd::IOperatorNewBase {
    virtual bool push(const luisa::span<const std::byte> data) = 0;
    static luisa::unique_ptr<IMessageSender> create(char const *topic, size_t max_size = 1024 * 1024);
    virtual ~IMessageSender() = default;
};

struct IMessageReceiver : vstd::IOperatorNewBase {
    virtual bool try_pop(luisa::vector<std::byte> &data) = 0;
    static luisa::unique_ptr<IMessageReceiver> create(char const *topic, size_t max_size = 1024 * 1024);
    virtual ~IMessageReceiver() = default;
};
}// namespace rbc