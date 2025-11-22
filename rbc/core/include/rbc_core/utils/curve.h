#pragma once
#include <luisa/core/mathematics.h>
#include <luisa/vstl/common.h>
#include <rbc_config.h>
namespace rbc {
using namespace luisa;
class RBC_CORE_API Curve {
    vstd::vector<float2> _key_nodes;
    float2 _range{
        1e8f,
        -1e8f};
    void _sync_range();
public:
    [[nodiscard]] auto min_time() const { return _range.x; }
    [[nodiscard]] auto max_time() const { return _range.y; }
    [[nodiscard]] auto key_nodes() const { return luisa::span<float2 const>{_key_nodes}; }
    Curve();
    Curve(std::initializer_list<float2> key_nodes);
    Curve(vstd::vector<float2> &&key_nodes);
    ~Curve();
    Curve(const Curve &) = default;
    Curve(Curve &&) = default;
    Curve &operator=(Curve const &) = default;
    Curve &operator=(Curve &&rhs) {
        std::destroy_at(this);
        std::construct_at(this, std::move(rhs));
        return *this;
    }
    void add_node(float time, float value);
    [[nodiscard]] float sample_node(float time) const;
};

struct Rect {
    float2 min = {1e8f, 1e8f};
    float2 max = {-1e8f, -1e8f};

    inline void extend(float2 point) {
        min = luisa::min(point, min);
        max = luisa::max(point, max);
    }
    inline bool is_valid() const {
        return min.x <= max.x && min.y <= max.y;
    }
    inline bool overlaps(const Rect &rhs) const {
        return rhs.max.x > min.x &&
               rhs.min.x < rhs.max.x &&
               rhs.max.y > min.y &&
               rhs.min.y < max.y;
    }
};

struct RBC_CORE_API CubicBezier2D {
    float2 p0;
    float2 p1;
    float2 p2;
    float2 p3;

    // ctor & dtor (default)

    // offset
    void offset(float2 offset);
    // eval
    float2 eval(float time) const;
    float2 eval_tangent(float time) const;
    float2 eval_normal(float time) const;
    // length & bound
    float length(int sample_count = 30) const;
    Rect bound_box(int sample_count = 30) const;
    Rect bound_box_of_control_point() const;

    // cut
    void cut_left(float time, CubicBezier2D &out_left);
    void cut_right(float time, CubicBezier2D &out_right);
    void cut_half(CubicBezier2D &out_left, CubicBezier2D &out_right) const;
    CubicBezier2D cut(float begin, float end) const;

    // intersection
    bool intersect_bb_from_ctrl_points(CubicBezier2D &rhs) const;
    // nearest, return the nearest time, use two step search
    //  step 1. sample points and find search range
    //  step 2. binary search in the range
    float nearest(float2 pos, int sample_count = 40, int binary_count = 20) const;
    float nearest_x(float x, int sample_count = 40, int binary_count = 20) const;

    // build bezier
    void build_through_point(float2 in_p0, float2 in_p3, float2 point);

private:
    static float _lengthsq(float2 v) {
        return dot(v, v);
    }
};

}// namespace rbc
