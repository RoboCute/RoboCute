#pragma once
#include <luisa/vstl/common.h>
namespace rbc {
struct RayCastRequire {
    float3 ray_origin;
    float3 ray_dir;
    float t_min;
    float t_max;
    uint8_t mask = std::numeric_limits<uint8_t>::max();
};
struct ClickRequire {
    float2 screen_uv;
    uint8_t mask = std::numeric_limits<uint8_t>::max();
};
struct RayCastResult {
    MatCode mat_code;
    uint inst_id;
    uint prim_id;
    uint submesh_id;
    float ray_length;
    float2 triangle_bary;
};

struct ClickManager {
    friend struct PreparePass;
    inline void add_require(
        luisa::string &&key,
        RayCastRequire const &require) {
        _requires.emplace_back(std::move(key), require);
    }
    inline void add_require(
        luisa::string &&key,
        ClickRequire const &require) {
        _requires.emplace_back(std::move(key), require);
    }
    inline luisa::optional<RayCastResult> query_result(luisa::string_view key) {
        // TODO @zhurong: should this be erased after found?
        std::lock_guard lck{_mtx};
        if (auto kvp = _results.find(key)) {
            auto disp = vstd::scope_exit([&]() {
                _results.remove(kvp);
            });
            if (kvp.value().ray_length < 0.0f) {
                return {};
            }
            return kvp.value();
        }
        return {};
    }
    inline void clear_requires() {
        _requires.clear();
    }
    void clear() {
        _requires.clear();
        _results.clear();
    }

private:
    luisa::vector<std::pair<luisa::string, luisa::variant<RayCastRequire, ClickRequire>>> _requires;
    vstd::HashMap<luisa::string, RayCastResult> _results;
    luisa::spin_mutex _mtx;
};
}// namespace rbc
RBC_RTTI(rbc::ClickManager)