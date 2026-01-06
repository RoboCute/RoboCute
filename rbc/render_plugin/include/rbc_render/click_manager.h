#pragma once
#include <luisa/core/mathematics.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/rtx/triangle.h>
#include <luisa/runtime/buffer.h>
#include <rbc_core/type_info.h>

namespace rbc {
struct ClickRequire {
    luisa::float2 screen_uv;
};
struct RayCastResult {
    luisa::float2 triangle_bary;
    uint inst_id{~0u};
    uint prim_id{~0u};
    uint mat_code;
    uint submesh_index{};
};
struct GizmosRequire {
    luisa::string name;
    luisa::float4x4 transform;
    luisa::compute::BufferView<luisa::float3> pos_buffer;
    luisa::compute::BufferView<luisa::float3> color_buffer;
    luisa::compute::BufferView<luisa::compute::Triangle> triangle_buffer;
    luisa::uint2 clicked_pos;
    luisa::float3 from_color;
    luisa::float3 to_color;
};
struct GizmosResult {
    luisa::float3 local_pos;
    uint32_t primitive_id;
};
struct EditingPass;
struct ClickManager {
    friend struct PipelineContext;
    friend struct EditingPass;
    inline void add_require(
        luisa::string &&key,
        ClickRequire const &require) {
        std::lock_guard lck{_mtx};
        _requires.emplace_back(std::move(key), require);
    }
    inline void add_gizmos_requires(
        GizmosRequire &&require) {
        std::lock_guard lck{_mtx};
        _gizmos_requires.emplace_back(std::move(require));
    }
    inline void add_frame_selection(
        luisa::string &&key,
        luisa::float2 min_projection,
        luisa::float2 max_projection,
        bool draw_rectangle) {
        std::lock_guard lck{_mtx};
        _frame_selection_requires.emplace_back(
            FrameSelectionRequires{
                .name = std::move(key),
                .min_projection = min_projection,
                .max_projection = max_projection,
                .draw_rectangle = draw_rectangle});
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
    inline luisa::optional<GizmosResult> query_gizmos_result(luisa::string_view key) {
        std::lock_guard lck{_mtx};
        if (auto kvp = _gizmos_clicked_result.find(key)) {
            auto disp = vstd::scope_exit([&]() {
                _gizmos_clicked_result.remove(kvp);
            });
            if (kvp.value().primitive_id == ~0u) {
                return {};
            }
            return kvp.value();
        }
        return {};
    }
    inline luisa::vector<uint> query_frame_selection(luisa::string_view key) {
        std::lock_guard lck{_mtx};
        if (auto kvp = _frame_selection_results.find(key)) {
            auto disp = vstd::scope_exit([&]() {
                _frame_selection_results.remove(kvp);
            });
            return {std::move(kvp.value())};
        }
        return {};
    }

    inline void set_contour_objects(luisa::vector<uint> &&obj_ids) {
        std::lock_guard lck{_mtx};
        _contour_objects = std::move(obj_ids);
    }
    inline void add_contour_objects(uint obj_id) {
        std::lock_guard lck{_mtx};
        _contour_objects.emplace_back(obj_id);
    }
    ClickManager() = default;
    ClickManager(ClickManager &&) = delete;
    ClickManager(ClickManager const &) = delete;
    ~ClickManager() = default;
private:
    void clear_requires();
    void clear();
    luisa::vector<uint> _contour_objects;
    struct FrameSelectionRequires {
        luisa::string name;
        luisa::float2 min_projection;
        luisa::float2 max_projection;
        bool draw_rectangle;
    };
    luisa::vector<std::pair<luisa::string, ClickRequire>> _requires;
    luisa::vector<GizmosRequire> _gizmos_requires;
    luisa::vector<FrameSelectionRequires> _frame_selection_requires;
    vstd::HashMap<luisa::string, RayCastResult> _results;
    vstd::HashMap<luisa::string, luisa::vector<uint>> _frame_selection_results;
    vstd::HashMap<luisa::string, GizmosResult> _gizmos_clicked_result;
    luisa::spin_mutex _mtx;
};
}// namespace rbc
RBC_RTTI(rbc::ClickManager)