#pragma once
struct Args {
	float4 distortion_Amount;
	float4 distortion_CenterScale;
	float4 bloom_settings;
	float4 bloom_color;
	float4 bloom_dirt_tile_offset;
	uint2 pixel_offset;
	float chromatic_aberration;
	float gamma;
	float hdr_display_multiplier;
	float hdr_input_multiplier;
	bool saturate_result;
	bool use_hdr10;
};

struct LpmArgs {
	std::array<uint4, 10> ctl;
	// bool shoulder;
	// bool con;
	// bool soft;
	// bool con2;
	// bool clip;
	// bool scaleOnly;
	uint flags;
};