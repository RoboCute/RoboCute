#include <rbc_core/runtime_static.h>

namespace rbc {
static RuntimeStaticBase *_runtime_static_header{};
RuntimeStaticBase::RuntimeStaticBase() {
    p_next = _runtime_static_header;
    _runtime_static_header = this;
}
RuntimeStaticBase::~RuntimeStaticBase() {
}
void RuntimeStaticBase::init_all() {
    for (auto p = _runtime_static_header; p; p = p->p_next) {
        p->init();
    }
}
void RuntimeStaticBase::dispose_all() {
    for (auto p = _runtime_static_header; p; p = p->p_next) {
        p->destroy();
    }
}
}// namespace rbc