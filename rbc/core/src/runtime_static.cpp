#include <rbc_core/runtime_static.h>
#include <luisa/core/logging.h>

namespace rbc {
static RuntimeStaticBase *_runtime_static_header{};
static bool _runtime_already_loaded{false};
void RuntimeStaticBase::_base_init() {
    if (_runtime_already_loaded) {
        _init();
    } else {
        _p_next = _runtime_static_header;
        _runtime_static_header = this;
    }
}
void RuntimeStaticBase::init_all() {
    if (_runtime_already_loaded) return;
    _runtime_already_loaded = true;
    for (auto p = _runtime_static_header; p; p = p->_p_next) {
        p->_init();
    }
}
void RuntimeStaticBase::dispose_all() {
    if (!_runtime_already_loaded) return;
    _runtime_already_loaded = false;
    for (auto p = _runtime_static_header; p; p = p->_p_next) {
        p->_destroy();
    }
}
void RuntimeStaticBase::_check_ptr(bool ptr) {
    if (!ptr) {
        LUISA_ERROR("Static object already disposed.");
    }
}
}// namespace rbc