#include <rbc_core/coroutine.h>
#include <luisa/core/logging.h>

namespace rbc {
coroutine::coroutine(coroutine &&rhs)
    : _base(rhs._base) {
    rhs._base = nullptr;
}
void coroutine::resume() {
    if (_base.done()) [[unlikely]]
        return;
    auto &prom = _base.promise();
    if (prom._awaitable_func) [[likely]] {
        if (prom._awaitable_func(prom._awaitable_ptr)) {
            prom._awaitable_func = nullptr;
            prom._awaitable_ptr = nullptr;
            _base.resume();
        }
    } else {
        _base.resume();
    }
}
bool coroutine::done() {
    return !_base || _base.done();
}
void coroutine::destroy() {
    if (!_base) [[unlikely]]
        return;
    _base.destroy();
    _base = nullptr;
}
coroutine::~coroutine() {
    if (_base) {
        _base.destroy();
    }
}
}// namespace rbc