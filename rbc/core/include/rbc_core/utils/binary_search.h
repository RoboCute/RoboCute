#pragma once
#include <luisa/core/stl/memory.h>

namespace rbc
{
template <typename F, typename V, typename Func>
requires(std::is_invocable_r_v<int, Func, F const&, V const&>)
inline size_t binary_search(luisa::span<F> sp, V const& value, Func&& compare_func)
{
    auto size = sp.size();
    int64_t high = size - 1;
    int64_t low = 0;
    int64_t index = 0;
    while (low <= high)
    {
        auto mid = low + (high - low) / 2;
        auto r = compare_func(sp[mid], value);
        switch (r)
        {
        case 0:
            return mid;
        case -1:
            low = mid + 1;
            index = low;
            break;
        case 1:
            high = mid - 1;
            index = mid;
            break;
        }
    }
    return index;
}
} // namespace rbc