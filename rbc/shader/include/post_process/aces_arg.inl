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
};