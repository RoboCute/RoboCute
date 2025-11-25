#pragma once
#include <rbc_core/serde.h>
#include <rbc_core/rpc/future.h>
#include <luisa/core/stl/vector.h>
#include <luisa/core/stl/functional.h>
namespace rbc {
struct RBC_CORE_API RPCCommandList {
private:
    JsonSerializer arg_ser;
    struct RetValue {
        RC<RCBase> storage;
        void *storage_ptr;
        vstd::func_ptr_t<void(RC<RCBase> &)> mark_finished;
        vstd::func_ptr_t<void(RC<RCBase> &, JsonDeSerializer *ret_deser)> deser_func;
    };
    luisa::vector<RetValue> ret_values;
    uint64_t _func_count{};
    uint64_t _arg_count{};

public:
    uint64_t func_count() const { return _func_count; }
    luisa::BinaryBlob commit_commands();
    void add_functioon(char const *name, void *handle);
    void locally_execute();
    void readback(luisa::BinaryBlob &&serialized_return_values);
    static luisa::BinaryBlob server_execute(
        uint64_t call_count,
        luisa::BinaryBlob &&commands);
    RPCCommandList();
    ~RPCCommandList();

    template<concepts::SerializableType<JsonSerializer> T>
    void add_arg(T const &t) {
        _arg_count += 1;
        arg_ser._store(t);
    }
    template<concepts::DeSerializableType<JsonDeSerializer> T>
    RPCFuture<T> return_value() {
        RPCFuture<T> future;
        ret_values.emplace_back(RetValue{
            future._value.get(),
            future._value->v.c,
            +[](RC<RCBase> &rc) {
                using ElemType = typename RPCFuture<T>::ValueType;
                auto value_ptr = static_cast<ElemType *>(rc.get());
                value_ptr->finished.store(true);
            },
            +[](RC<RCBase> &rc, JsonDeSerializer *ret_deser) {
                using ElemType = typename RPCFuture<T>::ValueType;
                auto value_ptr = static_cast<ElemType *>(rc.get());
                auto ptr = reinterpret_cast<T *>(value_ptr->v.c);
                std::construct_at(std::launder(ptr));
                ret_deser->_load(*ptr);
                value_ptr->finished.store(true);
            }});
        return future;
    }

    // static void backend_execute(luisa::move_only_function<luisa::BinaryBlob()>)

    RPCCommandList(RPCCommandList const &) = delete;
    RPCCommandList(RPCCommandList &&) = delete;
};
};// namespace rbc