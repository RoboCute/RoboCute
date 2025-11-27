#pragma once
#include <rbc_core/serde.h>
#include <rbc_ipc/future.h>
#include <luisa/vstl/functional.h>
#include <luisa/core/stl/vector.h>
#include <luisa/core/stl/functional.h>
namespace rbc {
struct RPCServer;
struct RBC_IPC_API RPCCommandList {
    friend struct RPCServer;
private:
    RPCServer *_server;
    JsonSerializer arg_ser;
    RC<RPCRetValueBase> return_values;
    uint _thread_id{};
    uint64_t _custom_id{};
    uint64_t _func_count{};
    uint64_t _arg_count{};

    void _add_future(RPCRetValueBase *future);
    RPCCommandList(RPCServer *server, uint thread_id, uint64_t custom_handle);
public:
    struct Commit {
        luisa::BinaryBlob arguments;
        RC<RPCRetValueBase> ret_values_handle;
    };
    uint64_t func_count() const { return _func_count; }
    Commit commit_commands();
    auto thread_id() const { return _thread_id; }
    void add_functioon(char const *name, void *handle);
    static void readback(
        vstd::function<RC<RPCRetValueBase>(uint64_t)> const &get_retvalue_handle,
        luisa::span<std::byte const> data);
    static luisa::BinaryBlob server_execute(
        uint64_t call_count,
        luisa::span<std::byte const> data);
    ~RPCCommandList();

    template<concepts::SerializableType<JsonSerializer> T>
    void add_arg(T const &t) {
        _arg_count += 1;
        arg_ser._store(t);
    }
    template<concepts::DeSerializableType<JsonDeSerializer> T>
    RPCFuture<T> return_value() {
        RPCFuture<T> future;
        _add_future(future._value.get());
        return future;
    }
    auto id() const { return _custom_id; }
    // static void backend_execute(luisa::move_only_function<luisa::BinaryBlob()>)

    RPCCommandList(RPCCommandList const &) = delete;
    RPCCommandList(RPCCommandList &&) = delete;
};
};// namespace rbc