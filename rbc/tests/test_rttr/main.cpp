#include <rbc_core/type_info.h>
#include <rbc_core/runtime_static.h>

using namespace rbc;
using namespace luisa;

int main() {
    RuntimeStaticBase::init_all();
    auto dispose_runtime_static = vstd::scope_exit([] {
        RuntimeStaticBase::dispose_all();
    });
}