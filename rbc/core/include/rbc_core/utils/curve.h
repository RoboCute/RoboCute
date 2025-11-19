#pragma once
#include <luisa/core/mathematics.h>
#include <luisa/vstl/common.h>
#include <rbc_config.h>
namespace rbc
{
using namespace luisa;
class RBC_CORE_API Curve
{
    vstd::vector<float2> _key_nodes;
    float2               _range{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::min()
    };

public:
    [[nodiscard]] auto min_time() const { return _range.x; }
    [[nodiscard]] auto max_time() const { return _range.y; }
    [[nodiscard]] auto key_nodes() const { return luisa::span<float2 const>{ _key_nodes }; }
    Curve();
    Curve(std::initializer_list<float2> key_nodes);
    Curve(vstd::vector<float2>&& key_nodes);
    ~Curve();
    Curve(const Curve&)            = default;
    Curve(Curve&&)                 = default;
    Curve& operator=(Curve const&) = default;
    Curve& operator=(Curve&& rhs)
    {
        this->~Curve();
        new (this) Curve(std::move(rhs));
        return *this;
    }
    void                add_node(float time, float value);
    [[nodiscard]] float sample_node(float time) const;
};

struct Rect {
    float2 min = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
    float2 max = { std::numeric_limits<float>::min(), std::numeric_limits<float>::min() };

    inline void extend(float2 point)
    {
        min = luisa::min(point, min);
        max = luisa::max(point, max);
    }
    inline bool is_valid() const
    {
        return min.x <= max.x && min.y <= max.y;
    }
    inline bool overlaps(const Rect& rhs) const
    {
        return rhs.max.x > min.x &&
               rhs.min.x < rhs.max.x &&
               rhs.max.y > min.y &&
               rhs.min.y < max.y;
    }
};

struct CubicBezier2D {
    float2 p0;
    float2 p1;
    float2 p2;
    float2 p3;

    // ctor & dtor (default)

    // offset
    inline void offset(float2 offset)
    {
        p0 += offset;
        p1 += offset;
        p2 += offset;
        p3 += offset;
    }

    // eval
    inline float2 eval(float time) const
    {
        float u   = 1 - time;
        float tt  = time * time;
        float uu  = u * u;
        float uuu = uu * u;
        float ttt = tt * time;

        float2 result;
        result = uuu * p0;
        result += 3 * uu * time * p1;
        result += 3 * u * tt * p2;
        result += ttt * p3;
        return result;
    }
    inline float2 eval_tangent(float time) const
    {
        float u   = 1 - time;
        float tt  = time * time;
        float uu  = u * u;

        float2 result;
        result = (-3 * uu) * p0;
        result += (3 * uu - 6 * u * time) * p1;
        result += (6 * u * time - 3 * tt) * p2;
        result += (3 * tt) * p3;

        return result;
    }
    inline float2 eval_normal(float time) const
    {
        float2 tangent = eval_tangent(time);
        return float2(-tangent.y, tangent.x);
    }

    // length & bound
    inline float length(int sample_count = 30) const
    {
        float  len        = 0;
        float2 last_point = p0;
        for (int i = 0; i <= sample_count; ++i)
        {
            float2 point = eval((float)i / sample_count);
            len += distance(last_point, point);
            last_point = point;
        }

        return len;
    }
    inline Rect bound_box(int sample_count = 30) const
    {
        Rect result;

        result.extend(p0);
        for (int i = 1; i <= sample_count; ++i)
        {
            float2 point = eval((float)i / sample_count);
            result.extend(point);
        }

        return result;
    }
    inline Rect bound_box_of_control_point() const
    {
        Rect result;

        result.extend(p0);
        result.extend(p1);
        result.extend(p2);
        result.extend(p3);

        return result;
    }

    // cut
    inline void cut_left(float time, CubicBezier2D& out_left)
    {
        out_left.p0 = p0;
        out_left.p1 = float2(p0 + time * (p1 - p0));
        out_left.p2 = float2(p1 + time * (p2 - p1)); // temporary holding spot
        p2          = float2(p2 + time * (p3 - p2));
        p1          = float2(out_left.p2 + time * (p2 - out_left.p2));
        out_left.p2 = (out_left.p1 + time * (out_left.p2 - out_left.p1));
        out_left.p3 = p0 = float2(out_left.p2 + time * (p1 - out_left.p2));
    }
    inline void cut_right(float time, CubicBezier2D& out_right)
    {
        out_right = *this;
        out_right.cut_left(time, *this);
    }
    inline void cut_half(CubicBezier2D& out_left, CubicBezier2D& out_right)
    {
        out_left.p0  = p0;
        out_right.p3 = p3;
        out_left.p1  = (p0 + p1) / 2.0f;
        out_right.p2 = (p2 + p3) / 2.0f;
        out_right.p1 = (p1 + p2) / 2.0f; // temporary holding spot
        out_left.p2  = (out_left.p1 + out_right.p1) / 2.0f;
        out_right.p1 = (out_right.p1 + out_right.p2) / 2.0f; // Real value this time
        out_left.p3 = out_right.p0 = (out_left.p2 + out_right.p1) / 2.0f;
    }
    inline CubicBezier2D cut(float begin, float end)
    {
        CubicBezier2D new_bezier = CubicBezier2D(p0, p1, p2, p3);
        CubicBezier2D tmp;
        new_bezier.cut_left(begin, tmp);
        float remapped_time = end / (1 - begin);
        new_bezier.cut_left(remapped_time, tmp);
        return tmp;
    }

    // intersection
    inline bool intersect_bb_from_ctrl_points(CubicBezier2D& rhs)
    {
        return bound_box_of_control_point().overlaps(rhs.bound_box_of_control_point());
    }

    // nearest, return the nearest time, use two step search
    //  step 1. sample points and find search range
    //  step 2. binary search in the range
    inline float nearest(float2 pos, int sample_count = 40, int binary_count = 20)
    {
        float step = 1.0f / (sample_count - 1);

        float begin = 0;
        float end   = 0;

        // sample search
        {
            float min_dis = std::numeric_limits<float>::max();
            for (int i = 0; i < sample_count; ++i)
            {
                float cur_time = i * step;
                float cur_dis  = _lengthsq(eval(cur_time) - pos);
                if (cur_dis < min_dis)
                {
                    begin   = cur_time;
                    min_dis = cur_dis;
                }
            }

            end   = begin > 1 ? begin : begin + step / 2;
            begin = begin < 0 ? begin : begin - step / 2;
        }

        // binary search
        for (int i = 0; i < binary_count; ++i)
        {
            float mid       = (begin + end) / 2;
            float begin_dis = _lengthsq(eval(begin) - pos);
            float end_dis   = _lengthsq(eval(end) - pos);

            if (begin_dis < end_dis)
            {
                end = mid;
            }
            else
            {
                begin = mid;
            }
        }

        return begin + end;
    }
    inline float nearest_x(float x, int sample_count = 40, int binary_count = 20)
    {
        float step = 1.0f / (sample_count - 1);

        float begin = 0;
        float end   = 0;

        // sample search
        {
            float min_dis = std::numeric_limits<float>::max();
            for (int i = 0; i < sample_count; ++i)
            {
                float cur_time = i * step;
                float cur_dis  = abs(eval(cur_time).x - x);
                if (cur_dis < min_dis)
                {
                    begin   = cur_time;
                    min_dis = cur_dis;
                }
            }

            end   = begin > 1 ? begin : begin + step / 2;
            begin = begin < 0 ? begin : begin - step / 2;
        }

        // binary search
        for (int i = 0; i < binary_count; ++i)
        {
            float mid       = (begin + end) / 2;
            float begin_dis = abs(eval(begin).x - x);
            float end_dis   = abs(eval(end).x - x);

            if (begin_dis < end_dis)
            {
                end = mid;
            }
            else
            {
                begin = mid;
            }
        }

        return begin + end;
    }

    // build bezier
    inline void build_through_point(float2 in_p0, float2 in_p3, float2 point)
    {
        // find quadratic bezier
        float2 p10    = in_p0 - point;
        float2 p12    = in_p3 - point;
        float  p10Len = luisa::length(p10);
        float  p12Len = luisa::length(p12);

        float2 pCenter = point - sqrt(p10Len * p12Len) * (p10 / p10Len + p12 / p12Len) / 2.0f;

        // upgrade to cubic bezier
        p0 = in_p0;
        p1 = in_p0 / 3.0f + pCenter * 2.0f / 3.0f;
        p2 = pCenter * 2.0f / 3.0f + in_p3 / 3.0f;
        p3 = in_p3;
    }

private:
    inline float _lengthsq(float2 v)
    {
        return dot(v, v);
    }
};
struct BezierCurve {
    struct Key {
        // control data
        float2 pos;
        float2 in_tangent;
        float2 out_tangent;
    };
    vstd::vector<Key> keys;

    // factory
};

} // namespace rbc
