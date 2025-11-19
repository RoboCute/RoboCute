#include <rbc_graphics/matrix.h>
using namespace luisa;
constexpr float PI = luisa::pi;
constexpr float INV_2PI = 1.f / (2 * PI);
constexpr float HALF_PI = PI * 0.5f;

void scalar_sin_cos(
	float* p_sin,
	float* p_cos,
	float value) noexcept {

	// Map value to y in [-pi,pi], x = 2*pi*quotient + remainder.
	float quotient = INV_2PI * value;
	if (value >= 0.0f) {
		quotient = static_cast<float>(static_cast<int>(quotient + 0.5f));
	} else {
		quotient = static_cast<float>(static_cast<int>(quotient - 0.5f));
	}
	float y = value - PI * 2 * quotient;

	// Map y to [-pi/2,pi/2] with sin(y) = sin(value).
	float sign;
	if (y > HALF_PI) {
		y = PI - y;
		sign = -1.0f;
	} else if (y < -HALF_PI) {
		y = -PI - y;
		sign = -1.0f;
	} else {
		sign = +1.0f;
	}

	float y2 = y * y;

	// 11-degree minimax approximation
	*p_sin = (((((-2.3889859e-08f * y2 + 2.7525562e-06f) * y2 - 0.00019840874f) * y2 + 0.0083333310f) * y2 - 0.16666667f) * y2 + 1.0f) * y;

	// 10-degree minimax approximation
	float p = ((((-2.6051615e-07f * y2 + 2.4760495e-05f) * y2 - 0.0013888378f) * y2 + 0.041666638f) * y2 - 0.5f) * y2 + 1.0f;
	*p_cos = sign * p;
}

float4x4 perspective_lh(
	float fov_angle_y,
	float aspect_ratio,
	float near_z,
	float far_z) {
	float sin_fov;
	float cos_fov;
	scalar_sin_cos(&sin_fov, &cos_fov, 0.5f * fov_angle_y);

	float height = cos_fov / sin_fov;
	float width = height / aspect_ratio;
	float range = far_z / (far_z - near_z);
	float4x4 m;
	m[0][0] = width;
	m[0][1] = 0.0f;
	m[0][2] = 0.0f;
	m[0][3] = 0.0f;

	m[1][0] = 0.0f;
	m[1][1] = height;
	m[1][2] = 0.0f;
	m[1][3] = 0.0f;

	m[2][0] = 0.0f;
	m[2][1] = 0.0f;
	m[2][2] = range;
	m[2][3] = 1.0f;

	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = -range * near_z;
	m[3][3] = 0.0f;
	return m;
}

float4 combine_cone(float4 cone_0, float4 cone_1) {
	float3 dir = lerp(cone_0.xyz(), cone_1.xyz(), float3(0.5f));
	float dir_len = length(dir);
	if (dir_len < 1e-4) {
		return float4(0, 0, 1, PI);
	}
	dir /= dir_len;
	float angle_0 = acos(dot(dir, cone_0.xyz())) + cone_0.w;
	float angle_1 = acos(dot(dir, cone_1.xyz())) + cone_1.w;
	float angle = min(max(angle_0, angle_1), PI);
	return make_float4(dir, angle);
}