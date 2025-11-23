#pragma once
#include <luisa/std.hpp>
#include <luisa/experimental/intrin.hpp>

using namespace luisa::shader;
#define ACEScc_MAX 1.4679964
#define ACEScc_MIDGRAY 0.4135884
#define float_MAX max_float32
#define ODT_SAT_FACTOR 0.93

/** Values of DIM_OUTPUT_DEVICE (matching C++'s EDisplayColorGamut) */
#define TONEMAPPER_GAMUT_REC709_D65 0
#define TONEMAPPER_GAMUT_DCIP3_D65 1
#define TONEMAPPER_GAMUT_Rec2020_D65 2
#define TONEMAPPER_GAMUT_ACES_D60 3
#define TONEMAPPER_GAMUT_ACEScg_D60 4
// TODO: matrix sequence
float3 aces_mul(float3x3 matrix, float3 vec) {
	return transpose(matrix) * vec;
}

template<concepts::arithmetic T>
T Min3(T a, T b, T c) {
	return min(a, min(b, c));
}
template<concepts::arithmetic T>
T Max3(T a, T b, T c) {
	return max(a, max(b, c));
}
template<concepts::float_family T>
T FastSign(T x) {
	return saturate(x * T(float_MAX) + T(0.5)) * T(2.0) - T(1.0);
}
//
// Precomputed matrices (pre-transposed)
// See https://github.com/ampas/aces-dev/blob/master/transforms/ctl/README-MATRIX.md
//
static const float3x3 REC709_2_AP0 = {
	0.4397010, 0.3829780, 0.1773350,
	0.0897923, 0.8134230, 0.0967616,
	0.0175440, 0.1115440, 0.8707040};

static const float3x3 REC2020_2_AP0 = {
	0.679086, 0.157701, 0.163213,
	0.046002, 0.859055, 0.094943,
	-0.000574, 0.028468, 0.972106};

static const float3x3 REC2020_2_AP1 = {
	0.974895, 0.019599, 0.005506,
	0.002180, 0.995535, 0.002285,
	0.004797, 0.024532, 0.970671};

static const float3x3 REC709_2_AP1 = {
	0.61319, 0.33951, 0.04737,
	0.07021, 0.91634, 0.01345,
	0.02062, 0.10957, 0.86961};

static const float3x3 AP0_2_REC709 = {
	2.52169, -1.13413, -0.38756,
	-0.27648, 1.37272, -0.09624,
	-0.01538, -0.15298, 1.16835};

static const float3x3 AP1_2_REC2020 = {
	1.025825, -0.020053, -0.005772,
	-0.002234, 1.004587, -0.002352,
	-0.005013, -0.025290, 1.030303};

static const float3x3 AP0_2_REC2020 = {
	1.490410, -0.266171, -0.224239,
	-0.080167, 1.182167, -0.102000,
	0.003228, -0.034776, 1.031549};

static const float3x3 AP1_2_REC709 = {
	1.70505, -0.62179, -0.08326,
	-0.13026, 1.14080, -0.01055,
	-0.02400, -0.12897, 1.15297};

static const float3x3 AP0_2_XYZ_MAT = {
	0.9525523959, 0.0000000000, 0.0000936786,
	0.3439664498, 0.7281660966, -0.0721325464,
	0.0000000000, 0.0000000000, 1.0088251844};

static const float3x3 XYZ_2_AP0_MAT = {
	1.0498110175, 0.0000000000, -0.0000974845,
	-0.4959030231, 1.3733130458, 0.0982400361,
	0.0000000000, 0.0000000000, 0.9912520182};

static const float3x3 AP0_2_AP1_MAT = {
	1.4514393161, -0.2365107469, -0.2149285693,
	-0.0765537734, 1.1762296998, -0.0996759264,
	0.0083161484, -0.0060324498, 0.9977163014};

static const float3x3 AP1_2_AP0_MAT = {
	0.6954522414, 0.1406786965, 0.1638690622,
	0.0447945634, 0.8596711185, 0.0955343182,
	-0.0055258826, 0.0040252103, 1.0015006723};

static const float3x3 AP1_2_XYZ_MAT = {
	0.6624541811, 0.1340042065, 0.1561876870,
	0.2722287168, 0.6740817658, 0.0536895174,
	-0.0055746495, 0.0040607335, 1.0103391003};

static const float3x3 XYZ_2_AP1_MAT = {
	1.6410233797, -0.3248032942, -0.2364246952,
	-0.6636628587, 1.6153315917, 0.0167563477,
	0.0117218943, -0.0082844420, 0.9883948585};

static const float3x3 REC709_2_XYZ_MAT = {
	0.412391, 0.357584, 0.180481,
	0.212639, 0.715169, 0.072192,
	0.019331, 0.119195, 0.9505322};

static const float3x3 REC2020_2_XYZ_MAT = {
	0.636958, 0.144617, 0.168881,
	0.262700, 0.677998, 0.059302,
	0.000000, 0.028073, 1.060985};

static const float3x3 XYZ_2_REC709_MAT = {
	3.2409699419, -1.5373831776, -0.4986107603,
	-0.9692436363, 1.8759675015, 0.0415550574,
	0.0556300797, -0.2039769589, 1.0569715142};

static const float3x3 XYZ_2_REC2020_MAT = {
	1.7166511880, -0.3556707838, -0.2533662814,
	-0.6666843518, 1.6164812366, 0.0157685458,
	0.0176398574, -0.0427706133, 0.9421031212};

static const float3x3 XYZ_2_DCIP3_MAT = {
	2.7253940305, -1.0180030062, -0.4401631952,
	-0.7951680258, 1.6897320548, 0.0226471906,
	0.0412418914, -0.0876390192, 1.1009293786};

static const float3x3 RRT_SAT_MAT = {
	0.9708890, 0.0269633, 0.00214758,
	0.0108892, 0.9869630, 0.00214758,
	0.0108892, 0.0269633, 0.96214800};

static const float3x3 ODT_SAT_MAT = {
	0.949056, 0.0471857, 0.00375827,
	0.019056, 0.9771860, 0.00375827,
	0.019056, 0.0471857, 0.93375800};

static const float3x3 D60_2_D65_CAT = {
	0.98722400, -0.00611327, 0.0159533,
	-0.00759836, 1.00186000, 0.0053302,
	0.00307257, -0.00509595, 1.0816800};

static const float ExpandGamut = 1.0f;
static const float BlueCorrection = 0.6f;

//
// Unity to ACES
//
// converts Unity raw (REC709 primaries) to
//          ACES2065-1 (AP0 w/ linear encoding)
//
float3 srgb_to_ACES_AP0(float3 x) {
	x = aces_mul(REC709_2_AP0, x);
	return x;
}

float3 rec2020_to_ACES_AP0(float3 x) {
	x = aces_mul(REC2020_2_AP0, x);
	return x;
}
//
// ACES to Unity
//
// converts ACES2065-1 (AP0 w/ linear encoding)
//          Unity raw (REC709 primaries) to
//
float3 ACES_AP0_to_srgb(float3 x) {
	x = aces_mul(AP0_2_REC709, x);
	return x;
}
float3 ACES_AP0_to_rec2020(float3 x) {
	x = aces_mul(AP0_2_REC2020, x);
	return x;
}

//
// ACES Color Space Conversion - ACES to ACEScc
//
// converts ACES2065-1 (AP0 w/ linear encoding) to
//          ACEScc (AP1 w/ logarithmic encoding)
//
// This transform follows the foraces_mulas from section 4.4 in S-2014-003
//
float ACES_to_ACEScc(float x) {
	if (x <= 0.0)
		return -0.35828683;// = (log2(exp2(-15.0) * 0.5) + 9.72) / 17.52
	else if (x < exp2(-15.0))
		return (log2(exp2(-16.0) + x * 0.5) + 9.72) / 17.52;
	else// (x >= exp2(-15.0))
		return (log2(x) + 9.72) / 17.52;
}

float3 ACES_to_ACEScc(float3 x) {
	x = clamp(x, 0.0, float_MAX);

	// x is clamped to [0, float_MAX], skip the <= 0 check
	return float3((x.x < 0.00003051757) ? (log2(0.00001525878 + x.x * 0.5) + 9.72) / 17.52 : (log2(x.x) + 9.72) / 17.52,
				  (x.y < 0.00003051757) ? (log2(0.00001525878 + x.y * 0.5) + 9.72) / 17.52 : (log2(x.y) + 9.72) / 17.52,
				  (x.z < 0.00003051757) ? (log2(0.00001525878 + x.z * 0.5) + 9.72) / 17.52 : (log2(x.z) + 9.72) / 17.52);

	/*
    return float3(
        ACES_to_ACEScc(x.r),
        ACES_to_ACEScc(x.g),
        ACES_to_ACEScc(x.b)
    );
    */
}

//
// ACES Color Space Conversion - ACEScc to ACES
//
// converts ACEScc (AP1 w/ ACESlog encoding) to
//          ACES2065-1 (AP0 w/ linear encoding)
//
// This transform follows the foraces_mulas from section 4.4 in S-2014-003
//
float ACEScc_to_ACES(float x) {
	// TODO: Optimize me
	if (x < -0.3013698630)// (9.72 - 15) / 17.52
		return (exp2(x * 17.52 - 9.72) - exp2(-16.0)) * 2.0;
	else if (x < (log2(float_MAX) + 9.72) / 17.52)
		return exp2(x * 17.52 - 9.72);
	else// (x >= (log2(float_MAX) + 9.72) / 17.52)
		return float_MAX;
}

float3 ACEScc_to_ACES(float3 x) {
	return float3(
		ACEScc_to_ACES(x.r),
		ACEScc_to_ACES(x.g),
		ACEScc_to_ACES(x.b));
}

//
// ACES Color Space Conversion - ACES to ACEScg
//
// converts ACES2065-1 (AP0 w/ linear encoding) to
//          ACEScg (AP1 w/ linear encoding)
//
float3 ACES_to_ACEScg(float3 x) {
	return aces_mul(AP0_2_AP1_MAT, x);
}

//
// ACES Color Space Conversion - ACEScg to ACES
//
// converts ACEScg (AP1 w/ linear encoding) to
//          ACES2065-1 (AP0 w/ linear encoding)
//
float3 ACEScg_to_ACES(float3 x) {
	return aces_mul(AP1_2_AP0_MAT, x);
}
