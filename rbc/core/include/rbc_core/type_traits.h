#pragma once
namespace rbc {
template<class _Ty>
static _Ty &lvalue_declval() noexcept {
    static_assert(false, "Calling declval is ill-formed, see N4950 [declval]/2.");
}

}// namespace rbc