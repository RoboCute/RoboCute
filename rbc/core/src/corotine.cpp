#include <rbc_core/coroutine.h>
#include <luisa/core/logging.h>
namespace rbc::coro {
void throw_coro_exception() {
    LUISA_ERROR("Coroutine throw exception.");
}
}// namespace rbc