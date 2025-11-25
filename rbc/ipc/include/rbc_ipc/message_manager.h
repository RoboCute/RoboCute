#pragma once
#include <rbc_config.h>
#include <luisa/vstl/hash_map.h>
#include <luisa/core/stl/string.h>
#include <luisa/core/stl/memory.h>
#include <luisa/core/stl/vector.h>
#include <luisa/core/binary_io.h>

namespace rbc::ipc {
enum struct EMessageQueueBackend {
    IPC,
    Network
};

struct IMessageSender : vstd::IOperatorNewBase {
    // interfaces
    virtual bool push(const luisa::span<const std::byte> data) = 0;
    virtual ~IMessageSender() = default;

    // utils
    RBC_IPC_API static luisa::unique_ptr<IMessageSender> create(char const *topic, EMessageQueueBackend backend, size_t max_size = 1024 * 1024);
};

struct IMessageReceiver : vstd::IOperatorNewBase {
    // interfaces
    virtual luisa::BinaryBlob try_pop() = 0;
    virtual ~IMessageReceiver() = default;

    // utils
    RBC_IPC_API static luisa::unique_ptr<IMessageReceiver> create(char const *topic, EMessageQueueBackend backend, size_t max_size = 1024 * 1024);
    RBC_IPC_API uint8_t pop_typed_message(luisa::vector<std::byte> &data);
private:
    std::mutex _mtx;
    luisa::vector<std::byte> _cache;
};
}// namespace rbc::ipc