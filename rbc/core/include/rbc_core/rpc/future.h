#pragma once
#include <rbc_core/serde.h>
#include <rbc_core/rc.h>
namespace rbc {
struct RPCCommandList;
template<concepts::DeSerializableType<JsonDeSerializer> T>
struct RPCFuture {
    friend struct RPCCommandList;
    ~RPCFuture() {}
    RPCFuture(RPCFuture const &) = default;
    RPCFuture(RPCFuture &&) = default;
private:
    struct ValueType : RCBase {
        vstd::Storage<T> v;
        std::atomic_bool finished{false};
        ValueType() = default;
        ~ValueType() {
            if (finished)
                std::destroy_at(reinterpret_cast<T *>(v.c));
        }
    };
    RC<ValueType> _value;
    RPCFuture() {
        _value = RC<ValueType>::New();
    }
public:
    T const *try_get() const {
        if (!_value->finished) {
            return nullptr;
        }
        return reinterpret_cast<T const *>(_value->v.c);
    }

    vstd::optional<T> try_take() {
        if (!_value->finished) {
            return {};
        }
        return {std::move(reinterpret_cast<T *>(_value->v.c))};
    }

    void wait() const {
        while (!_value->finished.load()) {
            std::this_thread::yield();
        }
    }

    T const &get() const {
        wait();
        return *reinterpret_cast<T const *>(_value->v.c);
    }

    T take() {
        wait();
        return std::move(reinterpret_cast<T *>(_value->v.c));
    }
};
}// namespace rbc