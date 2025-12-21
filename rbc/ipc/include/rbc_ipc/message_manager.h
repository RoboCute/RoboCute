#pragma once
#include <rbc_config.h>
#include <luisa/vstl/hash_map.h>
#include <luisa/core/stl/string.h>
#include <luisa/core/stl/memory.h>
#include <luisa/core/stl/vector.h>
#include <luisa/core/binary_io.h>
#include <luisa/core/stl/functional.h>

#include <rbc_core/base.h>

namespace rbc::ipc {
struct TypedHeader {
    uint32_t custom_id : 8;
    uint32_t size : 24;
};
enum struct EMessageQueueBackend {
    IPC,
    Network
};

struct IMessageSender : RBCStruct {
    // interfaces
    virtual uint64_t max_size() const = 0;
    virtual bool push(const luisa::span<const std::byte> data) = 0;
    virtual ~IMessageSender() = default;

    // utils
    RBC_IPC_API bool push(uint8_t custom_id, luisa::span<const std::byte> data);
    RBC_IPC_API static luisa::unique_ptr<IMessageSender> create(char const *topic, EMessageQueueBackend backend, size_t max_size = 1024 * 1024);
};
struct IMessageReceiver;
struct RBC_IPC_API AsyncTypedMessagePop {
    void reset(IMessageReceiver *self);
    bool next_step();
    luisa::vector<std::byte> steal_data() { return std::move(data); }
    luisa::span<std::byte const> get_data() const { return data; }
    uint8_t id() const { return header.custom_id; }
private:
    TypedHeader header{};
    luisa::vector<std::byte> data;
    luisa::BinaryBlob buf;
    IMessageReceiver *self{};
    std::byte *buf_data{};
    std::byte *buf_end{};
    uint step{};
};

struct IMessageReceiver : RBCStruct {
    friend struct AsyncTypedMessagePop;
    // interfaces
    virtual luisa::BinaryBlob try_pop() = 0;
    virtual ~IMessageReceiver() = default;
    virtual uint64_t max_size() const = 0;

    // utils
    RBC_IPC_API static luisa::unique_ptr<IMessageReceiver> create(char const *topic, EMessageQueueBackend backend, size_t max_size = 1024 * 1024);
    RBC_IPC_API uint8_t pop_typed_message(luisa::vector<std::byte> &data);
private:
    std::mutex _mtx;
    luisa::vector<std::byte> _cache;
};
}// namespace rbc::ipc