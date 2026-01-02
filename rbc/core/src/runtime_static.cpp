#include <rbc_core/runtime_static.h>

namespace rbc {
static RuntimeStaticBase *_runtime_static_header{};
static bool _runtime_already_loaded{false};
RuntimeStaticBase::RuntimeStaticBase() {
    if (_runtime_already_loaded) [[unlikely]] {
        LUISA_ERROR("RuntimeStatic<> class can not be defined as non-static variable or defiend in plugins.");
    }
    p_next = _runtime_static_header;
    _runtime_static_header = this;
}
RuntimeStaticBase::~RuntimeStaticBase() {
}
void RuntimeStaticBase::init_all() {
    _runtime_already_loaded = true;
    for (auto p = _runtime_static_header; p; p = p->p_next) {
        p->init();
    }
}
void RuntimeStaticBase::dispose_all() {
    for (auto p = _runtime_static_header; p; p = p->p_next) {
        p->destroy();
    }
}
void RuntimeStaticBase::check_ptr(bool ptr) {
    if (!ptr) {
        LUISA_ERROR("Static object already disposed.");
    }
}
}// namespace rbc