#pragma once
struct Args {
	float4 ColorBalance;
	float4 ColorFilter;
	float4 HueSatCon;

	float4 ChannelMixerRed;
	float4 ChannelMixerGreen;
	float4 ChannelMixerBlue;

	float4 Lift;
	float4 InvGamma;
	float4 Gain;

	// 0: unreal, 1: unity, 2. filmic
	int tone_mode;

	// unreal_ACES
	float unreal_tone_slope;
	float unreal_tone_toe;
	float unreal_tone_black_clip;
	float unreal_tone_shoulder;
	float unreal_tone_white_clip;

	// filmic ACES
	float filmic_tone_shoulder_strength;
	float filmic_tone_linear_strength;
	float filmic_tone_linear_angle;
	float filmic_tone_toe_strength;
	float filmic_tone_toe_numerator;
	float filmic_tone_toe_denominator;
};