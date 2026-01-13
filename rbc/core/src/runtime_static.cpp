#include <rbc_core/runtime_static.h>
#include <luisa/core/logging.h>

namespace rbc {
static RuntimeStaticBase *_runtime_static_header{};
static bool _runtime_already_loaded{false};
void RuntimeStaticBase::_base_init() {
    if (_runtime_already_loaded) {
        init();
    } else {
        p_next = _runtime_static_header;
        _runtime_static_header = this;
    }
}
void RuntimeStaticBase::init_all() {
    if (_runtime_already_loaded) return;
    _runtime_already_loaded = true;
    for (auto p = _runtime_static_header; p; p = p->p_next) {
        p->init();
    }
}
void RuntimeStaticBase::dispose_all() {
    if (!_runtime_already_loaded) return;
    _runtime_already_loaded = false;
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