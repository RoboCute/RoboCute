#pragma once
#include <rbc_core/serde.h>
#include <rbc_core/rpc/future.h>
#include <luisa/vstl/functional.h>
#include <luisa/core/stl/vector.h>
#include <luisa/core/stl/functional.h>
namespace rbc {
struct RBC_CORE_API RPCCommandList {
private:
    JsonSerializer arg_ser;

    RC<RPCRetValueBase> return_values;
    uint64_t _func_count{};
    uint64_t _arg_count{};

    void _add_future(RPCRetValueBase *future);
public:
    uint64_t func_count() const { return _func_count; }
    luisa::BinaryBlob commit_commands();
    void add_functioon(char const *name, void *handle);
    static void readback(
        vstd::function<RC<RPCRetValueBase>(uint64_t)> const& get_cmdlist,
        luisa::BinaryBlob &&serialized_return_values);
    static luisa::BinaryBlob server_execute(
        uint64_t call_count,
        luisa::BinaryBlob &&commands);
    RPCCommandList(uint64_t custom_handle);
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

    // static void backend_execute(luisa::move_only_function<luisa::BinaryBlob()>)

    RPCCommandList(RPCCommandList const &) = delete;
    RPCCommandList(RPCCommandList &&) = delete;
};
};// namespace rbc