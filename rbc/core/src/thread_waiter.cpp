#include <rbc_core/utils/thread_waiter.h>
#include <luisa/core/logging.h>
namespace rbc {
void ThreadWaiter::_waiting_sign(luisa::string_view name) {
    LUISA_INFO("Still waiting for {}", name);
}
}// namespace rbc