#pragma once
#include <luisa/std.hpp>
namespace compression {
inline uint encode_rgbe(float3 v) {
	float3 va = max(float3(0.), v);
	float max_abs = max(va.x, max(va.y, va.z));
	if (max_abs == 0) {
		return 0;
	}
	float exponent = floor(log2(max_abs));
	uint result = uint(clamp(exponent + 20, 0, 31)) << 27;
	float scale = exp2(-exponent) * 256.0f;
	uint3 vu = min(uint3(511), uint3(round(va * scale)));
	result |= vu.x;
	result |= vu.y << 9;
	result |= vu.z << 18;
	return result;
}
inline float3 decode_rgbe(uint x) {
	int exponent = int(x >> 27) - 20;
	float scale = exp2(float(exponent)) / 256.f;
	float3 v = float3(float(x & 0x1ff), float((x >> 9) & 0x1ff), float((x >> 18) & 0x1ff)) * scale;
	return v;
}

inline uint2 encode_highquality_rgbe(float3 hdr) {
	auto scale = max(max(hdr.x, hdr.y), hdr.z);
	auto e = uint(clamp(log2(scale) * 2048.f + 32768, 0., 65535.5f));
	hdr = (65536.f * hdr) / scale;
	auto hdr_uint = uint3(clamp(hdr, 0.f, 65535.5f));
	hdr_uint <<= uint3(16, 0, 16);
	return uint2(hdr_uint.x + hdr_uint.y, hdr_uint.z + e);
}

inline float3 decode_highquality_rgbe(uint2 rgbe) {
	auto rgb = (float3((uint3(rgbe.xx, rgbe.y) >> uint3(16, 0, 16)) & uint(65535)) + 0.5f) / 65536.f;
	auto e = float(rgbe.y & uint(65535));
	auto pow_e = exp2((e - 32768.f) / 2048.f);
	return rgb * pow_e;
}

inline uint encode_normal(float3 xyz) {
	auto encoded_xyz = uint3(clamp((xyz * 0.5f + 0.5f) * float3(2047,
																2047, 1023),
								   0.0f, float3(2047.5f, 2047.5f, 1023.5f)));
	encoded_xyz <<= uint3(21, 10, 0);
	return encoded_xyz.x + encoded_xyz.y + encoded_xyz.z;
}

inline float3 decode_normal(uint v) {
	auto norm = (uint3(v) >> uint3(21, 10, 0)) & uint3(2047, 2047, 1023);
	return (float3(norm) / float3(2047, 2047, 1023)) * 2.0f - 1.0f;
}

inline uint encode_rgb(float3 col) {
	auto encoded_xyz = uint3(clamp(col * float3(2047, 2047, 1023),
								   0.0f, float3(2047.5f, 2047.5f, 1023.5f)));
	encoded_xyz <<= uint3(21, 10, 0);
	return encoded_xyz.x + encoded_xyz.y + encoded_xyz.z;
}

inline float3 decode_rgb(uint v) {
	auto norm = (uint3(v) >> uint3(21, 10, 0)) & uint3(2047, 2047, 1023);
	return float3(norm) / float3(2047, 2047, 1023);
}

inline uint encode_rgba(float4 col) {
	auto encoded_xyz = uint4(clamp(col * float4(255), 0.0, float4(255.5)));
	encoded_xyz <<= uint4(24, 16, 8, 0);
	return encoded_xyz.x + encoded_xyz.y + encoded_xyz.z + encoded_xyz.w;
}

inline float4 decode_rgba(uint v) {
	uint4 norm = (uint4(v) >> uint4(24, 16, 8, 0)) & uint4(255);
	return float4(norm) / float4(255);
}
inline uint encode_xyz_15_15_2(uint x, uint y, uint z) {
	x = x & 0x7FFFu;
	y = y & 0x7FFFu;
	z = z & 0x3u;
	return (z << 30u) | (y << 15u) | x;
}
inline void decode_xyz_15_15_2(uint encoded, uint& x, uint& y, uint& z) {
	x = encoded & 0x7FFFu;
	y = (encoded >> 15u) & 0x7FFFu;
	z = (encoded >> 30u) & 0x3u;
}
}// namespace compression