#pragma once
#include <luisa/std.hpp>
#include <sampling/procedural_types.hpp>
#include <geometry/types.hpp>
#include <luisa/resources/buffer_heap_extern.hpp>
#include <luisa/resources/volume_heap_extern.hpp>
#include <utils/heap_indices.hpp>
#include <std/ex/type_list.hpp>
#ifndef DEFINED_G_ACCEL
namespace luisa::shader {
extern Accel& g_accel;
}// namespace luisa::shader
#endif
namespace luisa::shader {
struct ProceduralGeometry {
	float3 normal;
};
}// namespace luisa::shader
namespace shadertoy {
using namespace luisa::shader;
// Ray Tracing - Primitives. Created by Reinder Nijhoff 2019
// The MIT License
// @reindernijhoff
//
// https://www.shadertoy.com/view/tl23Rm
//
// I wanted to create a reference shader similar to "Raymarching - Primitives"
// (https://www.shadertoy.com/view/Xds3zN), but with ray-primitive intersection
// routines instead of sdf routines.
//
// As usual, I ended up mostly just copy-pasting code from Íñigo Quílez:
//
// https://iquilezles.org/articles/intersectors
//
// Please let me know if there are other routines that I should add to this shader.
//
// Sphere:          https://www.shadertoy.com/view/4d2XWV
// Box:             https://www.shadertoy.com/view/ld23DV
// Capped Cylinder: https://www.shadertoy.com/view/4lcSRn
// Torus:           https://www.shadertoy.com/view/4sBGDy
// Capsule:         https://www.shadertoy.com/view/Xt3SzX
// Capped Cone:     https://www.shadertoy.com/view/llcfRf
// Ellipsoid:       https://www.shadertoy.com/view/MlsSzn
// Rounded Cone:    https://www.shadertoy.com/view/MlKfzm
// Triangle:        https://www.shadertoy.com/view/MlGcDz
// Sphere4:         https://www.shadertoy.com/view/3tj3DW
// Goursat:         https://www.shadertoy.com/view/3lj3DW
// Rounded Box:     https://www.shadertoy.com/view/WlSXRW
//
// Disk:            https://www.shadertoy.com/view/lsfGDB
//
template<concepts::float_family T>
auto sign_float(T v) {
	using MaskType = copy_dim<uint, T>::type;
	auto low = select(T(1), T(0), (bit_cast<MaskType>(v) & MaskType(0x7fffffffu)) == MaskType(0));
	auto high = select(T(2), T(0), (bit_cast<MaskType>(v) & MaskType(0x80000000u)) == MaskType(0));
	return low - high;
}
template<concepts::float_family T>
auto inversesqrt(T v) {
	return T(1) / sqrt(v);
}

#define PROCEDURAL_TRACE_MAX_DIST 1e10f
#define PROCEDURAL_TRACE_CHECK_DIST 1e8f
float dot2(float3 v) { return dot(v, v); }

// Plane
float iPlane(float3 ro, float3 rd, float2 distBound, float3& normal,
			 float3 planeNormal, float planeDist) {
	float a = dot(rd, planeNormal);
	float d = -(dot(ro, planeNormal) + planeDist) / a;
	if (a > 0. || d < distBound.x || d > distBound.y) {
		return PROCEDURAL_TRACE_MAX_DIST;
	} else {
		normal = planeNormal;
		return d;
	}
}

// Sphere:          https://www.shadertoy.com/view/4d2XWV
float iSphere(float3 ro, float3 rd, float2 distBound, float3& normal,
			  float sphereRadius) {
	float b = dot(ro, rd);
	float c = dot(ro, ro) - sphereRadius * sphereRadius;
	float h = b * b - c;
	if (h < 0.) {
		return PROCEDURAL_TRACE_MAX_DIST;
	} else {
		h = sqrt(h);
		float d1 = -b - h;
		float d2 = -b + h;
		if (d1 >= distBound.x && d1 <= distBound.y) {
			normal = normalize(ro + rd * d1);
			return d1;
		} else if (d2 >= distBound.x && d2 <= distBound.y) {
			normal = normalize(ro + rd * d2);
			return d2;
		} else {
			return PROCEDURAL_TRACE_MAX_DIST;
		}
	}
}

// Box:             https://www.shadertoy.com/view/ld23DV
float iBox(float3 ro, float3 rd, float2 distBound, float3& normal,
		   float3 boxSize) {
	float3 m = sign_float(rd) / max(abs(rd), float3(1e-8f));
	float3 n = m * ro;
	float3 k = abs(m) * boxSize;

	float3 t1 = -n - k;
	float3 t2 = -n + k;

	float tN = max(max(t1.x, t1.y), t1.z);
	float tF = min(min(t2.x, t2.y), t2.z);

	if (tN > tF || tF <= 0.) {
		return PROCEDURAL_TRACE_MAX_DIST;
	} else {
		if (tN >= distBound.x && tN <= distBound.y) {
			normal = -sign_float(rd) * step(t1.yzx, t1.xyz) * step(t1.zxy, t1.xyz);
			return tN;
		} else if (tF >= distBound.x && tF <= distBound.y) {
			normal = -sign_float(rd) * step(t1.yzx, t1.xyz) * step(t1.zxy, t1.xyz);
			return tF;
		} else {
			return PROCEDURAL_TRACE_MAX_DIST;
		}
	}
}
// Box:             https://www.shadertoy.com/view/ld23DV
// near dist and far dist
float2 iBoxSimple(float3 ro, float3 rd, float3 boxSize, float3& normal) {
	float3 m = sign_float(rd) / max(abs(rd), float3(1e-8f));
	float3 n = m * ro;
	float3 k = abs(m) * boxSize;

	float3 t1 = -n - k;
	float3 t2 = -n + k;

	float tN = max(max(t1.x, t1.y), t1.z);
	float tF = min(min(t2.x, t2.y), t2.z);

	if (tN > tF || tF <= 0.) {
		return float2(PROCEDURAL_TRACE_MAX_DIST);
	} else {
		normal = -sign_float(rd) * step(t1.yzx, t1.xyz) * step(t1.zxy, t1.xyz);
		return float2(tN, tF);
	}
}

// Capped Cylinder: https://www.shadertoy.com/view/4lcSRn
float iCylinder(float3 ro, float3 rd, float2 distBound, float3& normal,
				float3 pa, float3 pb, float ra) {
	float3 ca = pb - pa;
	float3 oc = ro - pa;

	float caca = dot(ca, ca);
	float card = dot(ca, rd);
	float caoc = dot(ca, oc);

	float a = caca - card * card;
	float b = caca * dot(oc, rd) - caoc * card;
	float c = caca * dot(oc, oc) - caoc * caoc - ra * ra * caca;
	float h = b * b - a * c;

	if (h < 0.) return PROCEDURAL_TRACE_MAX_DIST;

	h = sqrt(h);
	float d = (-b - h) / a;

	float y = caoc + d * card;
	if (y > 0. && y < caca && d >= distBound.x && d <= distBound.y) {
		normal = (oc + d * rd - ca * y / caca) / ra;
		return d;
	}

	d = ((y < 0. ? 0. : caca) - caoc) / card;

	if (abs(b + a * d) < h && d >= distBound.x && d <= distBound.y) {
		normal = normalize(ca * sign_float(y) / caca);
		return d;
	} else {
		return PROCEDURAL_TRACE_MAX_DIST;
	}
}

// Torus:           https://www.shadertoy.com/view/4sBGDy
float iTorus(float3 ro, float3 rd, float2 distBound, float3& normal,
			 float2 torus) {
	// bounding sphere
	float3 tmpnormal;
	if (iSphere(ro, rd, distBound, tmpnormal, torus.y + torus.x) > distBound.y) {
		return PROCEDURAL_TRACE_MAX_DIST;
	}

	float po = 1.0f;

	float Ra2 = torus.x * torus.x;
	float ra2 = torus.y * torus.y;

	float m = dot(ro, ro);
	float n = dot(ro, rd);

#if 1
	float k = (m + Ra2 - ra2) / 2.0f;
	float k3 = n;
	float k2 = n * n - Ra2 * dot(rd.xy, rd.xy) + k;
	float k1 = n * k - Ra2 * dot(rd.xy, ro.xy);
	float k0 = k * k - Ra2 * dot(ro.xy, ro.xy);
#else
	float k = (m - Ra2 - ra2) / 2.0f;
	float k3 = n;
	float k2 = n * n + Ra2 * rd.z * rd.z + k;
	float k1 = k * n + Ra2 * ro.z * rd.z;
	float k0 = k * k + Ra2 * ro.z * ro.z - Ra2 * ra2;
#endif

#if 1
	// prevent |c1| from being too close to zero
	if (abs(k3 * (k3 * k3 - k2) + k1) < 0.01) {
		po = -1.0f;
		float tmp = k1;
		k1 = k3;
		k3 = tmp;
		k0 = 1.0f / k0;
		k1 = k1 * k0;
		k2 = k2 * k0;
		k3 = k3 * k0;
	}
#endif

	// reduced cubic
	float c2 = k2 * 2.0f - 3.0f * k3 * k3;
	float c1 = k3 * (k3 * k3 - k2) + k1;
	float c0 = k3 * (k3 * (c2 + 2.0f * k2) - 8.0f * k1) + 4.0f * k0;

	c2 /= 3.0f;
	c1 *= 2.0f;
	c0 /= 3.0f;

	float Q = c2 * c2 + c0;
	float R = c2 * c2 * c2 - 3.0f * c2 * c0 + c1 * c1;

	float h = R * R - Q * Q * Q;
	float t = PROCEDURAL_TRACE_MAX_DIST;

	if (h >= 0.0f) {
		// 2 intersections
		h = sqrt(h);

		float v = sign_float(R + h) * pow(abs(R + h), 1.0f / 3.0f);// cube root
		float u = sign_float(R - h) * pow(abs(R - h), 1.0f / 3.0f);// cube root

		float2 s = float2((v + u) + 4.0f * c2, (v - u) * sqrt(3.0f));

		float y = sqrt(0.5 * (length(s) + s.x));
		float x = 0.5 * s.y / y;
		float r = 2.0f * c1 / (x * x + y * y);

		float t1 = x - r - k3;
		t1 = (po < 0.0f) ? 2.0f / t1 : t1;
		float t2 = -x - r - k3;
		t2 = (po < 0.0f) ? 2.0f / t2 : t2;

		if (t1 >= distBound.x) t = t1;
		if (t2 >= distBound.x) t = min(t, t2);
	} else {
		// 4 intersections
		float sQ = sqrt(Q);
		float w = sQ * cos(acos(-R / (sQ * Q)) / 3.0f);

		float d2 = -(w + c2);
		if (d2 < 0.0f) return PROCEDURAL_TRACE_MAX_DIST;
		float d1 = sqrt(d2);

		float h1 = sqrt(w - 2.0f * c2 + c1 / d1);
		float h2 = sqrt(w - 2.0f * c2 - c1 / d1);
		float t1 = -d1 - h1 - k3;
		t1 = (po < 0.0f) ? 2.0f / t1 : t1;
		float t2 = -d1 + h1 - k3;
		t2 = (po < 0.0f) ? 2.0f / t2 : t2;
		float t3 = d1 - h2 - k3;
		t3 = (po < 0.0f) ? 2.0f / t3 : t3;
		float t4 = d1 + h2 - k3;
		t4 = (po < 0.0f) ? 2.0f / t4 : t4;

		if (t1 >= distBound.x) t = t1;
		if (t2 >= distBound.x) t = min(t, t2);
		if (t3 >= distBound.x) t = min(t, t3);
		if (t4 >= distBound.x) t = min(t, t4);
	}

	if (t >= distBound.x && t <= distBound.y) {
		float3 pos = ro + rd * t;
		normal = normalize(pos * (dot(pos, pos) - torus.y * torus.y - torus.x * torus.x * float3(1, 1, -1)));
		return t;
	} else {
		return PROCEDURAL_TRACE_MAX_DIST;
	}
}

// Capsule:         https://www.shadertoy.com/view/Xt3SzX
float iCapsule(float3 ro, float3 rd, float2 distBound, float3& normal,
			   float3 pa, float3 pb, float r) {
	float3 ba = pb - pa;
	float3 oa = ro - pa;

	float baba = dot(ba, ba);
	float bard = dot(ba, rd);
	float baoa = dot(ba, oa);
	float rdoa = dot(rd, oa);
	float oaoa = dot(oa, oa);

	float a = baba - bard * bard;
	float b = baba * rdoa - baoa * bard;
	float c = baba * oaoa - baoa * baoa - r * r * baba;
	float h = b * b - a * c;
	if (h >= 0.) {
		float t = (-b - sqrt(h)) / a;
		float d = PROCEDURAL_TRACE_MAX_DIST;

		float y = baoa + t * bard;

		// body
		if (y > 0.f && y < baba) {
			d = t;
		} else {
			// caps
			float3 oc = (y <= 0.f) ? oa : ro - pb;
			b = dot(rd, oc);
			c = dot(oc, oc) - r * r;
			h = b * b - c;
			if (h > 0.0f) {
				d = -b - sqrt(h);
			}
		}
		if (d >= distBound.x && d <= distBound.y) {
			pa = ro + rd * d - pa;
			float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0f, 1.0f);
			normal = (pa - h * ba) / r;
			return d;
		}
	}
	return PROCEDURAL_TRACE_MAX_DIST;
}

// Capped Cone:     https://www.shadertoy.com/view/llcfRf
float iCone(float3 ro, float3 rd, float2 distBound, float3& normal,
			float3 pa, float3 pb, float ra, float rb) {
	float3 ba = pb - pa;
	float3 oa = ro - pa;
	float3 ob = ro - pb;

	float m0 = dot(ba, ba);
	float m1 = dot(oa, ba);
	float m2 = dot(ob, ba);
	float m3 = dot(rd, ba);

	//caps
	if (m1 < 0.) {
		if (dot2(oa * m3 - rd * m1) < (ra * ra * m3 * m3)) {
			float d = -m1 / m3;
			if (d >= distBound.x && d <= distBound.y) {
				normal = -ba * inversesqrt(m0);
				return d;
			}
		}
	} else if (m2 > 0.) {
		if (dot2(ob * m3 - rd * m2) < (rb * rb * m3 * m3)) {
			float d = -m2 / m3;
			if (d >= distBound.x && d <= distBound.y) {
				normal = ba * inversesqrt(m0);
				return d;
			}
		}
	}

	// body
	float m4 = dot(rd, oa);
	float m5 = dot(oa, oa);
	float rr = ra - rb;
	float hy = m0 + rr * rr;

	float k2 = m0 * m0 - m3 * m3 * hy;
	float k1 = m0 * m0 * m4 - m1 * m3 * hy + m0 * ra * (rr * m3 * 1.0f);
	float k0 = m0 * m0 * m5 - m1 * m1 * hy + m0 * ra * (rr * m1 * 2.0f - m0 * ra);

	float h = k1 * k1 - k2 * k0;
	if (h < 0.) return PROCEDURAL_TRACE_MAX_DIST;

	float t = (-k1 - sqrt(h)) / k2;

	float y = m1 + t * m3;
	if (y > 0.f && y < m0 && t >= distBound.x && t <= distBound.y) {
		normal = normalize(m0 * (m0 * (oa + t * rd) + rr * ba * ra) - ba * hy * y);
		return t;
	} else {
		return PROCEDURAL_TRACE_MAX_DIST;
	}
}

// Ellipsoid:       https://www.shadertoy.com/view/MlsSzn
float iEllipsoid(float3 ro, float3 rd, float2 distBound, float3& normal,
				 float3 rad) {
	float3 ocn = ro / rad;
	float3 rdn = rd / rad;

	float a = dot(rdn, rdn);
	float b = dot(ocn, rdn);
	float c = dot(ocn, ocn);
	float h = b * b - a * (c - 1.f);

	if (h < 0.) {
		return PROCEDURAL_TRACE_MAX_DIST;
	}

	float d = (-b - sqrt(h)) / a;

	if (d < distBound.x || d > distBound.y) {
		return PROCEDURAL_TRACE_MAX_DIST;
	} else {
		normal = normalize((ro + d * rd) / rad);
		return d;
	}
}

// Rounded Cone:    https://www.shadertoy.com/view/MlKfzm
float iRoundedCone(float3 ro, float3 rd, float2 distBound, float3& normal,
				   float3 pa, float3 pb, float ra, float rb) {
	float3 ba = pb - pa;
	float3 oa = ro - pa;
	float3 ob = ro - pb;
	float rr = ra - rb;
	float m0 = dot(ba, ba);
	float m1 = dot(ba, oa);
	float m2 = dot(ba, rd);
	float m3 = dot(rd, oa);
	float m5 = dot(oa, oa);
	float m6 = dot(ob, rd);
	float m7 = dot(ob, ob);

	float d2 = m0 - rr * rr;

	float k2 = d2 - m2 * m2;
	float k1 = d2 * m3 - m1 * m2 + m2 * rr * ra;
	float k0 = d2 * m5 - m1 * m1 + m1 * rr * ra * 2. - m0 * ra * ra;

	float h = k1 * k1 - k0 * k2;
	if (h < 0.0f) {
		return PROCEDURAL_TRACE_MAX_DIST;
	}

	float t = (-sqrt(h) - k1) / k2;

	float y = m1 - ra * rr + t * m2;
	if (y > 0.0f && y < d2) {
		if (t >= distBound.x && t <= distBound.y) {
			normal = normalize(d2 * (oa + t * rd) - ba * y);
			return t;
		} else {
			return PROCEDURAL_TRACE_MAX_DIST;
		}
	} else {
		float h1 = m3 * m3 - m5 + ra * ra;
		float h2 = m6 * m6 - m7 + rb * rb;

		if (max(h1, h2) < 0.0f) {
			return PROCEDURAL_TRACE_MAX_DIST;
		}

		float3 n = float3(0);
		float r = PROCEDURAL_TRACE_MAX_DIST;

		if (h1 > 0.) {
			r = -m3 - sqrt(h1);
			n = (oa + r * rd) / ra;
		}
		if (h2 > 0.) {
			t = -m6 - sqrt(h2);
			if (t < r) {
				n = (ob + t * rd) / rb;
				r = t;
			}
		}
		if (r >= distBound.x && r <= distBound.y) {
			normal = n;
			return r;
		} else {
			return PROCEDURAL_TRACE_MAX_DIST;
		}
	}
}

// Triangle:        https://www.shadertoy.com/view/MlGcDz
float iTriangle(float3 ro, float3 rd, float2 distBound, float3& normal,
				float3 v0, float3 v1, float3 v2) {
	float3 v1v0 = v1 - v0;
	float3 v2v0 = v2 - v0;
	float3 rov0 = ro - v0;

	float3 n = cross(v1v0, v2v0);
	float3 q = cross(rov0, rd);
	float d = 1.0f / dot(rd, n);
	float u = d * dot(-q, v2v0);
	float v = d * dot(q, v1v0);
	float t = d * dot(-n, rov0);

	if (u < 0. || v < 0. || (u + v) > 1. || t < distBound.x || t > distBound.y) {
		return PROCEDURAL_TRACE_MAX_DIST;
	} else {
		normal = normalize(-n);
		return t;
	}
}

// Sphere4:         https://www.shadertoy.com/view/3tj3DW
float iSphere4(float3 ro, float3 rd, float2 distBound, float3& normal,
			   float ra) {
	// -----------------------------
	// solve quartic equation
	// -----------------------------

	float r2 = ra * ra;

	float3 d2 = rd * rd;
	float3 d3 = d2 * rd;
	float3 o2 = ro * ro;
	float3 o3 = o2 * ro;

	float ka = 1.0f / dot(d2, d2);

	float k0 = ka * dot(ro, d3);
	float k1 = ka * dot(o2, d2);
	float k2 = ka * dot(o3, rd);
	float k3 = ka * (dot(o2, o2) - r2 * r2);

	// -----------------------------
	// solve cubic
	// -----------------------------

	float c0 = k1 - k0 * k0;
	float c1 = k2 + 2.0f * k0 * (k0 * k0 - (3.0f / 2.0f) * k1);
	float c2 = k3 - 3.0f * k0 * (k0 * (k0 * k0 - 2.0f * k1) + (4.0f / 3.0f) * k2);

	float p = c0 * c0 * 3.0f + c2;
	float q = c0 * c0 * c0 - c0 * c2 + c1 * c1;
	float h = q * q - p * p * p * (1.0f / 27.0f);

	// -----------------------------
	// skip the case of 3 real solutions for the cubic, which involves
	// 4 complex solutions for the quartic, since we know this objcet is
	// convex
	// -----------------------------
	if (h < 0.0f) {
		return PROCEDURAL_TRACE_MAX_DIST;
	}

	// one real solution, two complex (conjugated)
	h = sqrt(h);

	float s = sign_float(q + h) * pow(abs(q + h), 1.0f / 3.0f);// cuberoot
	float t = sign_float(q - h) * pow(abs(q - h), 1.0f / 3.0f);// cuberoot

	float2 v = float2((s + t) + c0 * 4.0f, (s - t) * sqrt(3.0f)) * 0.5f;

	// -----------------------------
	// the quartic will have two real solutions and two complex solutions.
	// we only want the real ones
	// -----------------------------

	float r = length(v);
	float d = -abs(v.y) / sqrt(r + v.x) - c1 / r - k0;

	if (d >= distBound.x && d <= distBound.y) {
		float3 pos = ro + rd * d;
		normal = normalize(pos * pos * pos);
		return d;
	} else {
		return PROCEDURAL_TRACE_MAX_DIST;
	}
}

// Goursat:         https://www.shadertoy.com/view/3lj3DW
float cuberoot(float x) { return sign_float(x) * pow(abs(x), 1.0f / 3.0f); }

float iGoursat(float3 ro, float3 rd, float2 distBound, float3& normal,
			   float ra, float rb) {
	// hole: x4 + y4 + z4 - (r2^2)·(x2 + y2 + z2) + r1^4 = 0;
	float ra2 = ra * ra;
	float rb2 = rb * rb;

	float3 rd2 = rd * rd;
	float3 rd3 = rd2 * rd;
	float3 ro2 = ro * ro;
	float3 ro3 = ro2 * ro;

	float ka = 1.0f / dot(rd2, rd2);

	float k3 = ka * (dot(ro, rd3));
	float k2 = ka * (dot(ro2, rd2) - rb2 / 6.0f);
	float k1 = ka * (dot(ro3, rd) - rb2 * dot(rd, ro) / 2.0f);
	float k0 = ka * (dot(ro2, ro2) + ra2 * ra2 - rb2 * dot(ro, ro));

	float c2 = k2 - k3 * (k3);
	float c1 = k1 + k3 * (2.0f * k3 * k3 - 3.0f * k2);
	float c0 = k0 + k3 * (k3 * (c2 + k2) * 3.0f - 4.0f * k1);

	c0 /= 3.0f;

	float Q = c2 * c2 + c0;
	float R = c2 * c2 * c2 - 3.0f * c0 * c2 + c1 * c1;
	float h = R * R - Q * Q * Q;

	// 2 intersections
	if (h > 0.0f) {
		h = sqrt(h);

		float s = cuberoot(R + h);
		float u = cuberoot(R - h);

		float x = s + u + 4.0f * c2;
		float y = s - u;

		float k2 = x * x + y * y * 3.0f;

		float k = sqrt(k2);

		float d = -0.5 * abs(y) * sqrt(6.0f / (k + x)) - 2.0f * c1 * (k + x) / (k2 + x * k) - k3;

		if (d >= distBound.x && d <= distBound.y) {
			float3 pos = ro + rd * d;
			normal = normalize(4.0f * pos * pos * pos - 2.0f * pos * rb * rb);
			return d;
		} else {
			return PROCEDURAL_TRACE_MAX_DIST;
		}
	} else {
		// 4 intersections
		float sQ = sqrt(Q);
		float z = c2 - 2.0f * sQ * cos(acos(-R / (sQ * Q)) / 3.0f);

		float d1 = z - 3.0f * c2;
		float d2 = z * z - 3.0f * c0;

		if (abs(d1) < 1.0e-4) {
			if (d2 < 0.0f) return PROCEDURAL_TRACE_MAX_DIST;
			d2 = sqrt(d2);
		} else {
			if (d1 < 0.0f) return PROCEDURAL_TRACE_MAX_DIST;
			d1 = sqrt(d1 / 2.0f);
			d2 = c1 / d1;
		}

		//----------------------------------

		float h1 = sqrt(d1 * d1 - z + d2);
		float h2 = sqrt(d1 * d1 - z - d2);
		float t1 = -d1 - h1 - k3;
		float t2 = -d1 + h1 - k3;
		float t3 = d1 - h2 - k3;
		float t4 = d1 + h2 - k3;

		if (t2 < 0.0f && t4 < 0.0f) return PROCEDURAL_TRACE_MAX_DIST;

		float result = 1e20;
		if (t1 > 0.0f)
			result = t1;
		else if (t2 > 0.0f)
			result = t2;
		if (t3 > 0.0f)
			result = min(result, t3);
		else if (t4 > 0.0f)
			result = min(result, t4);

		if (result >= distBound.x && result <= distBound.y) {
			float3 pos = ro + rd * result;
			normal = normalize(4.0f * pos * pos * pos - 2.0f * pos * rb * rb);
			return result;
		} else {
			return PROCEDURAL_TRACE_MAX_DIST;
		}
	}
}

// Rounded Box:     https://www.shadertoy.com/view/WlSXRW
float iRoundedBox(float3 ro, float3 rd, float2 distBound, float3& normal,
				  float3 size, float rad) {
	// bounding box
	float3 m = 1.0f / rd;
	float3 n = m * ro;
	float3 k = abs(m) * (size + rad);
	float3 t1 = -n - k;
	float3 t2 = -n + k;
	float tN = max(max(t1.x, t1.y), t1.z);
	float tF = min(min(t2.x, t2.y), t2.z);
	if (tN > tF || tF < 0.0f) {
		return PROCEDURAL_TRACE_MAX_DIST;
	}
	float t = (tN >= distBound.x && tN <= distBound.y) ? tN :
			  (tF >= distBound.x && tF <= distBound.y) ? tF :
														 PROCEDURAL_TRACE_MAX_DIST;

	// convert to first octant
	float3 pos = ro + t * rd;
	float3 s = sign_float(pos);
	float3 ros = ro * s;
	float3 rds = rd * s;
	pos *= s;

	// faces
	pos -= size;
	pos = max(pos.xyz, pos.yzx);
	if (min(min(pos.x, pos.y), pos.z) < 0.0f) {
		if (t >= distBound.x && t <= distBound.y) {
			float3 p = ro + rd * t;
			normal = sign_float(p) * normalize(max(abs(p) - size, float3(0.0f)));
			return t;
		}
	}

	// some precomputation
	float3 oc = ros - size;
	float3 dd = rds * rds;
	float3 oo = oc * oc;
	float3 od = oc * rds;
	float ra2 = rad * rad;

	t = PROCEDURAL_TRACE_MAX_DIST;

	// corner
	{
		float b = od.x + od.y + od.z;
		float c = oo.x + oo.y + oo.z - ra2;
		float h = b * b - c;
		if (h > 0.0f) t = -b - sqrt(h);
	}

	// edge X
	{
		float a = dd.y + dd.z;
		float b = od.y + od.z;
		float c = oo.y + oo.z - ra2;
		float h = b * b - a * c;
		if (h > 0.0f) {
			h = (-b - sqrt(h)) / a;
			if (h >= distBound.x && h < t && abs(ros.x + rds.x * h) < size.x) t = h;
		}
	}
	// edge Y
	{
		float a = dd.z + dd.x;
		float b = od.z + od.x;
		float c = oo.z + oo.x - ra2;
		float h = b * b - a * c;
		if (h > 0.0f) {
			h = (-b - sqrt(h)) / a;
			if (h >= distBound.x && h < t && abs(ros.y + rds.y * h) < size.y) t = h;
		}
	}
	// edge Z
	{
		float a = dd.x + dd.y;
		float b = od.x + od.y;
		float c = oo.x + oo.y - ra2;
		float h = b * b - a * c;
		if (h > 0.0f) {
			h = (-b - sqrt(h)) / a;
			if (h >= distBound.x && h < t && abs(ros.z + rds.z * h) < size.z) t = h;
		}
	}

	if (t >= distBound.x && t <= distBound.y) {
		float3 p = ro + rd * t;
		normal = sign_float(p) * normalize(max(abs(p) - size, float3(1e-8f)));
		return t;
	} else {
		return PROCEDURAL_TRACE_MAX_DIST;
	};
}
}// namespace shadertoy

namespace sampling {
using namespace luisa::shader;
bool _sample_proccedural(
	Ray ray,
	uint user_id,
	auto hit,
	auto& rng,
	float& hit_dist,
	ProceduralGeometry& geometry,
	geometry::HeightMap height_map) {
	// TODO
	return false;
}

bool _sample_proccedural(
	Ray ray,
	uint user_id,
	auto hit,
	auto& rng,
	float& hit_dist,
	ProceduralGeometry& geometry,
	geometry::VoxelSurface voxel_map) {

	float4x4 inst_matrix = g_accel.instance_transform(hit.inst);
	float3 inst_pos = inst_matrix[3].xyz;

	auto inst_local_to_world = float3x3(
		inst_matrix[0].xyz,
		inst_matrix[1].xyz,
		inst_matrix[2].xyz);
	auto inst_world_to_local = inverse(inst_local_to_world);
	float3 ro = ray.origin();
	float3 rd = ray.dir();
	ro -= inst_pos;
	float3 local_ro = inst_world_to_local * ro;
	float3 local_rd = normalize(inst_world_to_local * rd);
	auto aabb = g_buffer_heap.buffer_read<AABB>(voxel_map.aabb_buffer_heap_idx, voxel_map.aabb_buffer_offset + hit.prim);
	float3 box_min(aabb.packed_min);
	float3 box_max(aabb.packed_max);
	float3 box_center = lerp(box_min, box_max, 0.5f);
	float3 box_size = abs(box_max - box_center);
	float3 local_normal;
	auto local_hit_dist = shadertoy::iBox(local_ro - box_center, local_rd, float2(0, PROCEDURAL_TRACE_MAX_DIST), local_normal, box_size);
	if (local_hit_dist > PROCEDURAL_TRACE_CHECK_DIST) {
		return false;
	}
	float3 local_hit_point = local_hit_dist * local_rd + local_ro;
	auto new_hit_dist = distance(inst_local_to_world * local_hit_point, ro);
	if (new_hit_dist >= hit_dist) return false;
	hit_dist = new_hit_dist;
	geometry.normal = normalize(inst_local_to_world * local_normal);
	return true;
}

bool _sample_proccedural(
	Ray ray,
	uint user_id,
	auto hit,
	auto& rng,
	float& hit_dist,
	ProceduralGeometry& geometry,
	geometry::SDFMap sdf_map) {

	auto sdf_sample = [&](float3 pos) {
		return g_volume_heap.volume_sample(sdf_map.volume_idx, pos * sdf_map.uvw_scale + sdf_map.uvw_offset, Filter::LINEAR_POINT, Address::EDGE).x;
	};

	float4x4 inst_matrix = g_accel.instance_transform(hit.inst);
	float3 inst_pos = inst_matrix[3].xyz;

	auto inst_local_to_world = float3x3(
		inst_matrix[0].xyz,
		inst_matrix[1].xyz,
		inst_matrix[2].xyz);
	auto inst_world_to_local = inverse(inst_local_to_world);
	float3 ro = ray.origin();
	float3 rd = ray.dir();
	ro -= inst_pos;
	float3 local_ro = inst_world_to_local * ro;
	float3 local_rd = normalize(inst_world_to_local * rd);
	float3 box_normal;
	auto local_hit_dist = shadertoy::iBoxSimple(local_ro, local_rd, float3(0.5f), box_normal);
	if (all(local_hit_dist > PROCEDURAL_TRACE_CHECK_DIST)) {
		return false;
	}
	float dist = 0;
	auto sdf_normal = [&](float3 p) noexcept {
		if(dist < 1e-5f) {
			return box_normal;
		}
		static constexpr float d = 1e-3f;
		float3 n = 0.f;
		float sdf_center = sdf_sample(p);
		for (uint i = 0; i < 3; i++) {
			float3 inc = p;
			inc[i] += d;
			n[i] = (1.0f / d) * (sdf_sample(inc) - sdf_center);
		}
		return normalize(n);
	};

	float max_dist = abs(local_hit_dist.y - local_hit_dist.x);
	float3 p = local_ro + local_rd * max(min(local_hit_dist.x, local_hit_dist.y), 0.f);
	for (uint i = 0; i < sdf_map.sample_count; ++i) {
		float s = sdf_sample(p + dist * local_rd);
		if (s <= 1e-6f) {
			break;
		};
		dist += s;
		if (dist > max_dist) {
			return false;
		}
	}
	float3 local_hit_point = p + dist * local_rd;
	auto new_hit_dist = distance(inst_local_to_world * local_hit_point, ro);
	// if(all(dispatch_id() == dispatch_size() / 2u)) {
	// 	device_log("{} {}", new_hit_dist, local_hit_dist.x);
	// }
	float3 local_normal = sdf_normal(local_hit_point);
	if (new_hit_dist >= hit_dist) return false;
	hit_dist = new_hit_dist;
	geometry.normal = normalize(inst_local_to_world * local_normal);
	return true;
}
using PolymorphicGeometry = stdex::type_list<
	geometry::HeightMap,
	geometry::VoxelSurface,
	geometry::SDFMap>;

bool sample_procedural(
	Ray ray,
	auto hit,
	auto& rng,
	float& hit_dist,
	ProceduralGeometry& geometry) {
	auto user_id = g_accel.instance_user_id(hit.inst);
	geometry::ProceduralType procedural_type = g_buffer_heap.buffer_read<geometry::ProceduralType>(heap_indices::procedural_type_buffer_idx, user_id);
	bool sampled = false;
	return PolymorphicGeometry::visit(procedural_type.type, [&]<typename ins>() {
		using type = ins::type;
		auto prim = g_buffer_heap.template byte_buffer_read<type>(heap_indices::buffer_allocator_heap_index, procedural_type.meta_byte_offset);
		return _sample_proccedural(ray, user_id, hit, rng, hit_dist, geometry, prim);
	});
}
}// namespace sampling