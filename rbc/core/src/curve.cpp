#include <rbc_core/utils/curve.h>
#include <rbc_core/utils/binary_search.h>

namespace rbc {
namespace curve_detail {
size_t _get_idx(Curve const &curve, float time) {
    return rbc::binary_search(curve.key_nodes(), time, [](float2 a, float b) {
        if (a.x < b) return -1;
        if (a.x > b) return 1;
        return 0;
    });
}
};// namespace curve_detail
void Curve::add_node(float time, float value) {
    _range.x = std::min(time, _range.x);
    _range.y = std::max(time, _range.y);
    if (_key_nodes.empty()) {
        _key_nodes.emplace_back(time, value);
        return;
    }
    auto idx = curve_detail::_get_idx(*this, time);
    _key_nodes.emplace(_key_nodes.begin() + idx, time, value);
}
float Curve::sample_node(float time) const {
    if (_key_nodes.empty()) return 0;
    if (_key_nodes.size() == 1) return _key_nodes[0].y;
    int64_t next_idx = curve_detail::_get_idx(*this, time);
    int64_t idx = next_idx - 1;
    next_idx = std::clamp<int64_t>(next_idx, 0, _key_nodes.size() - 1);
    idx = std::clamp<int64_t>(idx, 0, _key_nodes.size() - 1);
    if (next_idx == idx) {
        return _key_nodes[idx].y;
    }
    auto lerp_val = (_key_nodes[next_idx].x - time) / std::max<float>(_key_nodes[next_idx].x - _key_nodes[idx].x, 1e-5f);
    return lerp(_key_nodes[next_idx].y, _key_nodes[idx].y, lerp_val);
}
Curve::Curve() {}
Curve::~Curve() {}
void Curve::_sync_range() {
    for (auto &i : _key_nodes) {
        _range.x = std::min<double>(i.x, _range.x);
        _range.y = std::max<double>(i.x, _range.y);
    }
}
Curve::Curve(std::initializer_list<float2> key_nodes) : _key_nodes(key_nodes) {
    _sync_range();
}
Curve::Curve(vstd::vector<float2> &&key_nodes) : _key_nodes(std::move(key_nodes)) {
    _sync_range();
}
void CubicBezier2D::offset(float2 offset) {
    p0 += offset;
    p1 += offset;
    p2 += offset;
    p3 += offset;
}

// eval
float2 CubicBezier2D::eval(float time) const {
    float u = 1 - time;
    float tt = time * time;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * time;

    float2 result;
    result = uuu * p0;
    result += 3 * uu * time * p1;
    result += 3 * u * tt * p2;
    result += ttt * p3;
    return result;
}
float2 CubicBezier2D::eval_tangent(float time) const {
    float u = 1 - time;
    float tt = time * time;
    float uu = u * u;

    float2 result;
    result = (-3 * uu) * p0;
    result += (3 * uu - 6 * u * time) * p1;
    result += (6 * u * time - 3 * tt) * p2;
    result += (3 * tt) * p3;

    return result;
}
float2 CubicBezier2D::eval_normal(float time) const {
    float2 tangent = eval_tangent(time);
    return float2(-tangent.y, tangent.x);
}

// length & bound
float CubicBezier2D::length(int sample_count) const {
    float len = 0;
    float2 last_point = p0;
    for (int i = 0; i <= sample_count; ++i) {
        float2 point = eval((float)i / sample_count);
        len += distance(last_point, point);
        last_point = point;
    }

    return len;
}
Rect CubicBezier2D::bound_box(int sample_count) const {
    Rect result;

    result.extend(p0);
    for (int i = 1; i <= sample_count; ++i) {
        float2 point = eval((float)i / sample_count);
        result.extend(point);
    }

    return result;
}
Rect CubicBezier2D::bound_box_of_control_point() const {
    Rect result;

    result.extend(p0);
    result.extend(p1);
    result.extend(p2);
    result.extend(p3);

    return result;
}

// cut
void CubicBezier2D::cut_left(float time, CubicBezier2D &out_left) {
    out_left.p0 = p0;
    out_left.p1 = float2(p0 + time * (p1 - p0));
    out_left.p2 = float2(p1 + time * (p2 - p1));// temporary holding spot
    p2 = float2(p2 + time * (p3 - p2));
    p1 = float2(out_left.p2 + time * (p2 - out_left.p2));
    out_left.p2 = (out_left.p1 + time * (out_left.p2 - out_left.p1));
    out_left.p3 = p0 = float2(out_left.p2 + time * (p1 - out_left.p2));
}
void CubicBezier2D::cut_right(float time, CubicBezier2D &out_right) {
    out_right = *this;
    out_right.cut_left(time, *this);
}
void CubicBezier2D::cut_half(CubicBezier2D &out_left, CubicBezier2D &out_right) const {
    out_left.p0 = p0;
    out_right.p3 = p3;
    out_left.p1 = (p0 + p1) / 2.0f;
    out_right.p2 = (p2 + p3) / 2.0f;
    out_right.p1 = (p1 + p2) / 2.0f;// temporary holding spot
    out_left.p2 = (out_left.p1 + out_right.p1) / 2.0f;
    out_right.p1 = (out_right.p1 + out_right.p2) / 2.0f;// Real value this time
    out_left.p3 = out_right.p0 = (out_left.p2 + out_right.p1) / 2.0f;
}
CubicBezier2D CubicBezier2D::cut(float begin, float end) const {
    CubicBezier2D new_bezier = CubicBezier2D(p0, p1, p2, p3);
    CubicBezier2D tmp;
    new_bezier.cut_left(begin, tmp);
    float remapped_time = end / (1 - begin);
    new_bezier.cut_left(remapped_time, tmp);
    return tmp;
}

// intersection
bool CubicBezier2D::intersect_bb_from_ctrl_points(CubicBezier2D &rhs) const {
    return bound_box_of_control_point().overlaps(rhs.bound_box_of_control_point());
}

// nearest, return the nearest time, use two step search
//  step 1. sample points and find search range
//  step 2. binary search in the range
float CubicBezier2D::nearest(float2 pos, int sample_count, int binary_count) const {
    float step = 1.0f / (sample_count - 1);

    float begin = 0;
    float end = 0;

    // sample search
    {
        float min_dis = std::numeric_limits<float>::max();
        for (int i = 0; i < sample_count; ++i) {
            float cur_time = i * step;
            float cur_dis = _lengthsq(eval(cur_time) - pos);
            if (cur_dis < min_dis) {
                begin = cur_time;
                min_dis = cur_dis;
            }
        }

        end = begin > 1 ? begin : begin + step / 2;
        begin = begin < 0 ? begin : begin - step / 2;
    }

    // binary search
    for (int i = 0; i < binary_count; ++i) {
        float mid = (begin + end) / 2;
        float begin_dis = _lengthsq(eval(begin) - pos);
        float end_dis = _lengthsq(eval(end) - pos);

        if (begin_dis < end_dis) {
            end = mid;
        } else {
            begin = mid;
        }
    }

    return begin + end;
}
float CubicBezier2D::nearest_x(float x, int sample_count, int binary_count) const {
    float step = 1.0f / (sample_count - 1);

    float begin = 0;
    float end = 0;

    // sample search
    {
        float min_dis = std::numeric_limits<float>::max();
        for (int i = 0; i < sample_count; ++i) {
            float cur_time = i * step;
            float cur_dis = abs(eval(cur_time).x - x);
            if (cur_dis < min_dis) {
                begin = cur_time;
                min_dis = cur_dis;
            }
        }

        end = begin > 1 ? begin : begin + step / 2;
        begin = begin < 0 ? begin : begin - step / 2;
    }

    // binary search
    for (int i = 0; i < binary_count; ++i) {
        float mid = (begin + end) / 2;
        float begin_dis = abs(eval(begin).x - x);
        float end_dis = abs(eval(end).x - x);

        if (begin_dis < end_dis) {
            end = mid;
        } else {
            begin = mid;
        }
    }

    return begin + end;
}

// build bezier
void CubicBezier2D::build_through_point(float2 in_p0, float2 in_p3, float2 point) {
    // find quadratic bezier
    float2 p10 = in_p0 - point;
    float2 p12 = in_p3 - point;
    float p10Len = luisa::length(p10);
    float p12Len = luisa::length(p12);

    float2 pCenter = point - sqrt(p10Len * p12Len) * (p10 / p10Len + p12 / p12Len) / 2.0f;

    // upgrade to cubic bezier
    p0 = in_p0;
    p1 = in_p0 / 3.0f + pCenter * 2.0f / 3.0f;
    p2 = pCenter * 2.0f / 3.0f + in_p3 / 3.0f;
    p3 = in_p3;
}
}// namespace rbc