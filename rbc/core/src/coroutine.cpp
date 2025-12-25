#include <rbc_core/coroutine.h>
#include <luisa/core/logging.h>

namespace rbc::coro {
coroutine::coroutine(coroutine &&rhs)
    : _base(rhs._base),
      _own(rhs._own) {
    rhs._own = false;
    rhs._base = {};
}
void coroutine::resume() {
    LUISA_DEBUG_ASSERT(_own, "Coroutine already disposed.");
    if (_base.done()) [[unlikely]]
        return;
    auto &aw = _base.promise()._awaitable;
    if (aw) [[likely]] {
        if (aw->await_ready()) {
            aw = nullptr;
            _base.resume();
        }
    } else {
        _base.resume();
    }
}
bool coroutine::done() {
    LUISA_DEBUG_ASSERT(_own, "Coroutine already disposed.");
    return _base.done();
}
void coroutine::destroy() {
    if (!_own) [[unlikely]]
        return;
    _base.destroy();
    _own = false;
    _base = {};
}
coroutine::~coroutine() {
    if (_own) {
        _base.destroy();
    }
}
}// namespace rbc::coro