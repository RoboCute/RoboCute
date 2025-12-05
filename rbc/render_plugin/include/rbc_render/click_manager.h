#pragma once
#include <luisa/vstl/common.h>
namespace rbc {
struct ClickRequire {
    float2 screen_uv;
};
struct RayCastResult {
    float2 triangle_bary;
    uint inst_id{~0u};
    uint prim_id{~0u};
    uint mat_code;
    uint submesh_index{};
};

struct ClickManager {
    friend struct RasterPass;
    inline void add_require(
        luisa::string &&key,
        ClickRequire const &require) {
        _requires.emplace_back(std::move(key), require);
    }
    inline luisa::optional<RayCastResult> query_result(luisa::string_view key) {
        std::lock_guard lck{_mtx};
        if (auto kvp = _results.find(key)) {
            auto disp = vstd::scope_exit([&]() {
                _results.remove(kvp);
            });
            if (kvp.value().inst_id == ~0u) {
                return {};
            }
            return kvp.value();
        }
        return {};
    }
    inline void clear_requires() {
        std::lock_guard lck{_mtx};
        _requires.clear();
    }
    void clear() {
        std::lock_guard lck{_mtx};
        _requires.clear();
        _results.clear();
    }

private:
    luisa::vector<std::pair<luisa::string, ClickRequire>> _requires;
    vstd::HashMap<luisa::string, RayCastResult> _results;
    luisa::spin_mutex _mtx;
};
}// namespace rbc
RBC_RTTI(rbc::ClickManager)