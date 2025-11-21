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

//
// Reference Rendering Transform (RRT)
//
//   Input is ACES
//   Output is OCES
//
float rgb_2_saturation(float3 rgb) {
	const float TINY = 1e-4;
	float mi = Min3(rgb.r, rgb.g, rgb.b);
	float ma = Max3(rgb.r, rgb.g, rgb.b);
	return (max(ma, TINY) - max(mi, TINY)) / max(ma, 1e-2);
}

float rgb_2_yc(float3 rgb) {
	const float ycRadiusWeight = 1.75;

	// Converts RGB to a luminance proxy, here called YC
	// YC is ~ Y + K * Chroma
	// Constant YC is a cone-shaped surface in RGB space, with the tip on the
	// neutral axis, towards white.
	// YC is normalized: RGB 1 1 1 maps to YC = 1
	//
	// ycRadiusWeight defaults to 1.75, although can be overridden in function
	// call to rgb_2_yc
	// ycRadiusWeight = 1 -> YC for pure cyan, magenta, yellow == YC for neutral
	// of same value
	// ycRadiusWeight = 2 -> YC for pure red, green, blue  == YC for  neutral of
	// same value.

	float r = rgb.x;
	float g = rgb.y;
	float b = rgb.z;
	float chroma = sqrt(b * (b - g) + g * (g - r) + r * (r - b));
	return (b + g + r + ycRadiusWeight * chroma) / 3.0;
}

float rgb_2_hue(float3 rgb) {
	// Returns a geometric hue angle in degrees (0-360) based on RGB values.
	// For neutral colors, hue is undefined and the function will return a quiet NaN value.
	float hue;
	if (rgb.x == rgb.y && rgb.y == rgb.z)
		hue = 0.0;// RGB triplets where RGB are equal have an undefined hue
	else
		hue = (180.0 / pi) * atan2(sqrt(3.0) * (rgb.y - rgb.z), 2.0 * rgb.x - rgb.y - rgb.z);

	if (hue < 0.0) hue = hue + 360.0;

	return hue;
}

float center_hue(float hue, float centerH) {
	float hueCentered = hue - centerH;
	if (hueCentered < -180.0)
		hueCentered = hueCentered + 360.0;
	else if (hueCentered > 180.0)
		hueCentered = hueCentered - 360.0;
	return hueCentered;
}

float sigmoid_shaper(float x) {
	// Sigmoid function in the range 0 to 1 spanning -2 to +2.

	float t = max(1 - abs(0.5 * x), 0);
	float y = 1 + FastSign(x) * (1 - t * t);
	return 0.5 * y;
}

float glow_fwd(float ycIn, float glowGainIn, float glowMid) {
	float glowGainOut;

	if (ycIn <= 2.0 / 3.0 * glowMid)
		glowGainOut = glowGainIn;
	else if (ycIn >= 2.0 * glowMid)
		glowGainOut = 0.0;
	else
		glowGainOut = glowGainIn * (glowMid / ycIn - 1.0 / 2.0);

	return glowGainOut;
}
static const float3x3 M = {
	0.5, -1.0, 0.5,
	-1.0, 1.0, 0.0,
	0.5, 0.5, 0.0};

float segmented_spline_c5_fwd(float x) {
	static const std::array<float, 6> coefsLow = {-4.0000000000, -4.0000000000, -3.1573765773, -0.4852499958, 1.8477324706, 1.8477324706};// coefs for B-spline between minPoint and midPoint (units of log luminance)
	static const std::array<float, 6> coefsHigh = {-0.7185482425, 2.0810307172, 3.6681241237, 4.0000000000, 4.0000000000, 4.0000000000};  // coefs for B-spline between midPoint and maxPoint (units of log luminance)
	const float2 minPoint = float2(0.18 * exp2(-15.0), 0.0001);																			  // {luminance, luminance} linear extension below this
	const float2 midPoint = float2(0.18, 0.48);																							  // {luminance, luminance}
	const float2 maxPoint = float2(0.18 * exp2(18.0), 10000.0);																			  // {luminance, luminance} linear extension above this
	const float slopeLow = 0.0;																											  // log-log slope of low linear extension
	const float slopeHigh = 0.0;																										  // log-log slope of high linear extension

	const int N_KNOTS_LOW = 4;
	const int N_KNOTS_HIGH = 4;

	// Check for negatives or zero before taking the log. If negative or zero,
	// set to ACESMIN.1
	float xCheck = x;
	if (xCheck <= 0.0) xCheck = 0.00006103515;// = exp2(-14.0);

	float logx = log10(xCheck);
	float logy;

	if (logx <= log10(minPoint.x)) {
		logy = logx * slopeLow + (log10(minPoint.y) - slopeLow * log10(minPoint.x));
	} else if ((logx > log10(minPoint.x)) && (logx < log10(midPoint.x))) {
		float knot_coord = (N_KNOTS_LOW - 1) * (logx - log10(minPoint.x)) / (log10(midPoint.x) - log10(minPoint.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = float3(coefsLow[j], coefsLow[j + 1], coefsLow[j + 2]);
		float3 monomials = float3(t * t, t, 1.0);
		logy = dot(monomials, aces_mul(M, cf));
	} else if ((logx >= log10(midPoint.x)) && (logx < log10(maxPoint.x))) {
		float knot_coord = (N_KNOTS_HIGH - 1) * (logx - log10(midPoint.x)) / (log10(maxPoint.x) - log10(midPoint.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = float3(coefsHigh[j], coefsHigh[j + 1], coefsHigh[j + 2]);
		float3 monomials = float3(t * t, t, 1.0);
		logy = dot(monomials, aces_mul(M, cf));
	} else {//if (logIn >= log10(maxPoint.x)) {
		logy = logx * slopeHigh + (log10(maxPoint.y) - slopeHigh * log10(maxPoint.x));
	}

	return pow(10.0, logy);
}

float segmented_spline_c9_fwd(float x) {
	const std::array<float, 10> coefsLow = {-1.6989700043, -1.6989700043, -1.4779000000, -1.2291000000, -0.8648000000, -0.4480000000, 0.0051800000, 0.4511080334, 0.9113744414, 0.9113744414};// coefs for B-spline between minPoint and midPoint (units of log luminance)
	const std::array<float, 10> coefsHigh = {0.5154386965, 0.8470437783, 1.1358000000, 1.3802000000, 1.5197000000, 1.5985000000, 1.6467000000, 1.6746091357, 1.6878733390, 1.6878733390};	  // coefs for B-spline between midPoint and maxPoint (units of log luminance)
	const float2 minPoint = float2(segmented_spline_c5_fwd(0.18 * exp2(-6.5)), 0.02);																										  // {luminance, luminance} linear extension below this
	const float2 midPoint = float2(segmented_spline_c5_fwd(0.18), 4.8);																														  // {luminance, luminance}
	const float2 maxPoint = float2(segmented_spline_c5_fwd(0.18 * exp2(6.5)), 48.0);																										  // {luminance, luminance} linear extension above this
	const float slopeLow = 0.0;																																								  // log-log slope of low linear extension
	const float slopeHigh = 0.04;																																							  // log-log slope of high linear extension

	const int N_KNOTS_LOW = 8;
	const int N_KNOTS_HIGH = 8;

	// Check for negatives or zero before taking the log. If negative or zero,
	// set to OCESMIN.
	float xCheck = x;
	if (xCheck <= 0.0) xCheck = 1e-4;

	float logx = log10(xCheck);
	float logy;

	if (logx <= log10(minPoint.x)) {
		logy = logx * slopeLow + (log10(minPoint.y) - slopeLow * log10(minPoint.x));
	} else if ((logx > log10(minPoint.x)) && (logx < log10(midPoint.x))) {
		float knot_coord = (N_KNOTS_LOW - 1) * (logx - log10(minPoint.x)) / (log10(midPoint.x) - log10(minPoint.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = float3(coefsLow[j], coefsLow[j + 1], coefsLow[j + 2]);
		float3 monomials = float3(t * t, t, 1.0);
		logy = dot(monomials, aces_mul(M, cf));
	} else if ((logx >= log10(midPoint.x)) && (logx < log10(maxPoint.x))) {
		float knot_coord = (N_KNOTS_HIGH - 1) * (logx - log10(midPoint.x)) / (log10(maxPoint.x) - log10(midPoint.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = float3(coefsHigh[j], coefsHigh[j + 1], coefsHigh[j + 2]);
		float3 monomials = float3(t * t, t, 1.0);
		logy = dot(monomials, aces_mul(M, cf));
	} else {//if (logIn >= log10(maxPoint.x)) {
		logy = logx * slopeHigh + (log10(maxPoint.y) - slopeHigh * log10(maxPoint.x));
	}

	return pow(10.0, logy);
}

#define RRT_GLOW_GAIN 0.05f
#define RRT_GLOW_MID 0.08f

#define RRT_RED_SCALE 0.82f
#define RRT_RED_PIVOT 0.03f
#define RRT_RED_HUE 0.0f
#define RRT_RED_WIDTH 135.0f

#define RRT_SAT_FACTOR 0.96f

float3 RRT(float3 aces) {
	// --- Glow module --- //
	float saturation = rgb_2_saturation(aces);
	float ycIn = rgb_2_yc(aces);
	float s = sigmoid_shaper((saturation - 0.4) / 0.2);
	float addedGlow = 1.0 + glow_fwd(ycIn, RRT_GLOW_GAIN * s, RRT_GLOW_MID);
	aces *= addedGlow;

	// --- Red modifier --- //
	float hue = rgb_2_hue(aces);
	float centeredHue = center_hue(hue, RRT_RED_HUE);
	float hueWeight;
	{
		//hueWeight = cubic_basis_shaper(centeredHue, RRT_RED_WIDTH);
		hueWeight = smoothstep(0.0, 1.0, 1.0 - abs(2.0 * centeredHue / RRT_RED_WIDTH));
		hueWeight *= hueWeight;
	}

	aces.r += hueWeight * saturation * (RRT_RED_PIVOT - aces.r) * (1.0 - RRT_RED_SCALE);

	// --- ACES to RGB rendering space --- //
	aces = clamp(aces, 0.0, float_MAX);// avoids saturated negative colors from becoming positive in the matrix
	float3 rgbPre = aces_mul(AP0_2_AP1_MAT, aces);
	rgbPre = clamp(rgbPre, 0, float_MAX);

	// --- Global desaturation --- //
	//rgbPre = aces_mul(RRT_SAT_MAT, rgbPre);
	rgbPre = lerp(float3(aces_mul(AP1_2_XYZ_MAT, rgbPre).y), rgbPre, float3(RRT_SAT_FACTOR));

	// --- Apply the tonescale independently in rendering-space RGB --- //
	float3 rgbPost;
	rgbPost.x = segmented_spline_c5_fwd(rgbPre.x);
	rgbPost.y = segmented_spline_c5_fwd(rgbPre.y);
	rgbPost.z = segmented_spline_c5_fwd(rgbPre.z);

	// --- RGB rendering space to OCES --- //
	float3 rgbOces = aces_mul(AP1_2_AP0_MAT, rgbPost);

	return rgbOces;
}

//
// Output Device Transform
//
float3 Y_2_linCV(float3 Y, float Ymax, float Ymin) {
	return (Y - Ymin) / (Ymax - Ymin);
}

float3 XYZ_2_xyY(float3 XYZ) {
	float divisor = max(dot(XYZ, float3(1.0)), 1e-4);
	return float3(XYZ.xy / divisor, XYZ.y);
}

float3 xyY_2_XYZ(float3 xyY) {
	float m = xyY.z / max(xyY.y, 1e-4);
	float3 XYZ = float3(xyY.xz, (1.0 - xyY.x - xyY.y));
	XYZ.xz *= m;
	return XYZ;
}

float3 UE_xyY_2_XYZ(float3 xyY) {
	float3 XYZ;
	XYZ[0] = xyY[0] * xyY[2] / max(xyY[1], 1e-10);
	XYZ[1] = xyY[2];
	XYZ[2] = (1.0 - xyY[0] - xyY[1]) * xyY[2] / max(xyY[1], 1e-10);

	return XYZ;
}

#define DIM_SURROUND_GAMMA 0.9811

float3 darkSurround_to_dimSurround(float3 linearCV) {
	float3 XYZ = aces_mul(AP1_2_XYZ_MAT, linearCV);

	float3 xyY = XYZ_2_xyY(XYZ);
	xyY.z = clamp(xyY.z, 0.0, float_MAX);
	xyY.z = pow(xyY.z, DIM_SURROUND_GAMMA);
	XYZ = xyY_2_XYZ(xyY);

	return aces_mul(XYZ_2_AP1_MAT, XYZ);
}

float moncurve_r(float y, float gamma, float offs) {
	// Reverse monitor curve
	float x;
	const float yb = pow(offs * gamma / ((gamma - 1.0) * (1.0 + offs)), gamma);
	const float rs = pow((gamma - 1.0) / offs, gamma - 1.0) * pow((1.0 + offs) / gamma, gamma);
	if (y >= yb)
		x = (1.0 + offs) * pow(y, 1.0 / gamma) - offs;
	else
		x = y * rs;
	return x;
}

float bt1886_r(float L, float gamma, float Lw, float Lb) {
	// The reference EOTF specified in Rec. ITU-R BT.1886
	// L = a(max[(V+b),0])^g
	float a = pow(pow(Lw, 1.0 / gamma) - pow(Lb, 1.0 / gamma), gamma);
	float b = pow(Lb, 1.0 / gamma) / (pow(Lw, 1.0 / gamma) - pow(Lb, 1.0 / gamma));
	float V = pow(max(L / a, 0.0), 1.0 / gamma) - b;
	return V;
}

float roll_white_fwd(
	float x,	  // color value to adjust (white scaled to around 1.0)
	float new_wht,// white adjustment (e.g. 0.9 for 10% darkening)
	float width	  // adjusted width (e.g. 0.25 for top quarter of the tone scale)
) {
	const float x0 = -1.0;
	const float x1 = x0 + width;
	const float y0 = -new_wht;
	const float y1 = x1;
	const float m1 = (x1 - x0);
	const float a = y0 - y1 + m1;
	const float b = 2.0 * (y1 - y0) - m1;
	const float c = y0;
	const float t = (-x - x0) / (x1 - x0);
	float o = 0.0;
	if (t < 0.0)
		o = -(t * b + c);
	else if (t > 1.0)
		o = x;
	else
		o = -((t * a + b) * t + c);
	return o;
}

float3 linear_to_bt1886(float3 x, float gamma, float Lw, float Lb) {
	// Good enough approximation for now, may consider using the exact foraces_mula instead
	// TODO: Experiment
	return pow(max(x, float3(0)), 1.0 / 2.4);

	// Correct implementation (Reference EOTF specified in Rec. ITU-R BT.1886) :
	// L = a(max[(V+b),0])^g
	float invgamma = 1.0 / gamma;
	float p_Lw = pow(Lw, invgamma);
	float p_Lb = pow(Lb, invgamma);
	float3 a = float3(pow(p_Lw - p_Lb, gamma));
	float3 b = float3(p_Lb / p_Lw - p_Lb);
	float3 V = pow(max(x / a, float3(0)), float3(invgamma)) - b;
	return V;
}

#define CINEMA_WHITE float(48.0)
#define CINEMA_BLACK float(CINEMA_WHITE / 2400.0)
#define ODT_SAT_FACTO 0.93f

// <ACEStransformID>ODT.Academy.RGBmonitor_100nits_dim.a1.0.3</ACEStransformID>
// <ACESuserName>ACES 1.0 Output - REC709</ACESuserName>

//
// Output Device Transform - RGB computer monitor
//

//
// Summary :
//  This transform is intended for mapping OCES onto a desktop computer monitor
//  typical of those used in motion picture visual effects production. These
//  monitors may occasionally be referred to as "REC709" displays, however, the
//  monitor for which this transform is designed does not exactly match the
//  specifications in IEC 61966-2-1:1999.
//
//  The assumed observer adapted white is D65, and the viewing environment is
//  that of a dim surround.
//
//  The monitor specified is intended to be more typical of those found in
//  visual effects production.
//
// Device Primaries :
//  Primaries are those specified in Rec. ITU-R BT.709
//  CIE 1931 chromaticities:  x         y         Y
//              Red:          0.64      0.33
//              Green:        0.3       0.6
//              Blue:         0.15      0.06
//              White:        0.3127    0.329     100 cd/m^2
//
// Display EOTF :
//  The reference electro-optical transfer function specified in
//  IEC 61966-2-1:1999.
//
// Signal Range:
//    This transform outputs full range code values.
//
// Assumed observer adapted white point:
//         CIE 1931 chromaticities:    x            y
//                                     0.3127       0.329
//
// Viewing Environment:
//   This ODT has a compensation for viewing environment variables more typical
//   of those associated with video mastering.
//
float3 ODT_RGBmonitor_100nits_dim(float3 oces) {
	// OCES to RGB rendering space
	float3 rgbPre = aces_mul(AP0_2_AP1_MAT, oces);

	// Apply the tonescale independently in rendering-space RGB
	float3 rgbPost;
	rgbPost.x = segmented_spline_c9_fwd(rgbPre.x);
	rgbPost.y = segmented_spline_c9_fwd(rgbPre.y);
	rgbPost.z = segmented_spline_c9_fwd(rgbPre.z);

	// Scale luminance to linear code value
	float3 linearCV = Y_2_linCV(rgbPost, CINEMA_WHITE, CINEMA_BLACK);

	// Apply gamma adjustment to compensate for dim surround
	linearCV = darkSurround_to_dimSurround(linearCV);

	// Apply desaturation to compensate for luminance difference
	//linearCV = aces_mul(ODT_SAT_MAT, linearCV);
	linearCV = lerp(float3(aces_mul(AP1_2_XYZ_MAT, linearCV).y), linearCV, float3(ODT_SAT_FACTOR));

	linearCV = aces_mul(AP1_2_REC709, linearCV);

	// Handle out-of-gamut values
	// Clip values < 0 or > 1 (i.e. projecting outside the display primaries)
	linearCV = saturate(linearCV);

	// TODO: Revisit when it is possible to deactivate Unity default framebuffer encoding
	// with REC709 opto-electrical transfer function (OETF).
	/*
    // Encode linear code values with transfer function
    float3 outputCV;
    // moncurve_r with gamma of 2.4 and offset of 0.055 matches the EOTF found in IEC 61966-2-1:1999 (REC709)
    const float DISPGAMMA = 2.4;
    const float OFFSET = 0.055;
    outputCV.x = moncurve_r(linearCV.x, DISPGAMMA, OFFSET);
    outputCV.y = moncurve_r(linearCV.y, DISPGAMMA, OFFSET);
    outputCV.z = moncurve_r(linearCV.z, DISPGAMMA, OFFSET);

    outputCV = linear_to_REC709(linearCV);
    */

	// Unity already draws to a REC709 target
	return linearCV;
}

// <ACEStransformID>ODT.Academy.RGBmonitor_D60sim_100nits_dim.a1.0.3</ACEStransformID>
// <ACESuserName>ACES 1.0 Output - REC709 (D60 sim.)</ACESuserName>

//
// Output Device Transform - RGB computer monitor (D60 siaces_mulation)
//

//
// Summary :
//  This transform is intended for mapping OCES onto a desktop computer monitor
//  typical of those used in motion picture visual effects production. These
//  monitors may occasionally be referred to as "REC709" displays, however, the
//  monitor for which this transform is designed does not exactly match the
//  specifications in IEC 61966-2-1:1999.
//
//  The assumed observer adapted white is D60, and the viewing environment is
//  that of a dim surround.
//
//  The monitor specified is intended to be more typical of those found in
//  visual effects production.
//
// Device Primaries :
//  Primaries are those specified in Rec. ITU-R BT.709
//  CIE 1931 chromaticities:  x         y         Y
//              Red:          0.64      0.33
//              Green:        0.3       0.6
//              Blue:         0.15      0.06
//              White:        0.3127    0.329     100 cd/m^2
//
// Display EOTF :
//  The reference electro-optical transfer function specified in
//  IEC 61966-2-1:1999.
//
// Signal Range:
//    This transform outputs full range code values.
//
// Assumed observer adapted white point:
//         CIE 1931 chromaticities:    x            y
//                                     0.32168      0.33767
//
// Viewing Environment:
//   This ODT has a compensation for viewing environment variables more typical
//   of those associated with video mastering.
//
float3 ODT_RGBmonitor_D60sim_100nits_dim(float3 oces) {
	// OCES to RGB rendering space
	float3 rgbPre = aces_mul(AP0_2_AP1_MAT, oces);

	// Apply the tonescale independently in rendering-space RGB
	float3 rgbPost;
	rgbPost.x = segmented_spline_c9_fwd(rgbPre.x);
	rgbPost.y = segmented_spline_c9_fwd(rgbPre.y);
	rgbPost.z = segmented_spline_c9_fwd(rgbPre.z);

	// Scale luminance to linear code value
	float3 linearCV = Y_2_linCV(rgbPost, CINEMA_WHITE, CINEMA_BLACK);

	// --- Compensate for different white point being darker  --- //
	// This adjustment is to correct an issue that exists in ODTs where the device
	// is calibrated to a white chromaticity other than D60. In order to siaces_mulate
	// D60 on such devices, unequal code values are sent to the display to achieve
	// neutrals at D60. In order to produce D60 on a device calibrated to the DCI
	// white point (i.e. equal code values yield CIE x,y chromaticities of 0.314,
	// 0.351) the red channel is higher than green and blue to compensate for the
	// "greenish" DCI white. This is the correct behavior but it means that as
	// highlight increase, the red channel will hit the device maximum first and
	// clip, resulting in a chromaticity shift as the green and blue channels
	// continue to increase.
	// To avoid this clipping error, a slight scale factor is applied to allow the
	// ODTs to siaces_mulate D60 within the D65 calibration white point.

	// Scale and clamp white to avoid casted highlights due to D60 siaces_mulation
	const float SCALE = 0.955;
	linearCV = min(linearCV, float3(1.0f)) * SCALE;

	// Apply gamma adjustment to compensate for dim surround
	linearCV = darkSurround_to_dimSurround(linearCV);

	// Apply desaturation to compensate for luminance difference
	//linearCV = aces_mul(ODT_SAT_MAT, linearCV);
	linearCV = lerp(float3(aces_mul(AP1_2_XYZ_MAT, linearCV).y), linearCV, float3(ODT_SAT_FACTOR));

	// Convert to display primary encoding
	// Rendering space RGB to XYZ
	float3 XYZ = aces_mul(AP1_2_XYZ_MAT, linearCV);

	// CIE XYZ to display primaries
	linearCV = aces_mul(XYZ_2_REC709_MAT, XYZ);

	// Handle out-of-gamut values
	// Clip values < 0 or > 1 (i.e. projecting outside the display primaries)
	linearCV = saturate(linearCV);

	// TODO: Revisit when it is possible to deactivate Unity default framebuffer encoding
	// with REC709 opto-electrical transfer function (OETF).
	/*
    // Encode linear code values with transfer function
    float3 outputCV;
    // moncurve_r with gamma of 2.4 and offset of 0.055 matches the EOTF found in IEC 61966-2-1:1999 (REC709)
    const float DISPGAMMA = 2.4;
    const float OFFSET = 0.055;
    outputCV.x = moncurve_r(linearCV.x, DISPGAMMA, OFFSET);
    outputCV.y = moncurve_r(linearCV.y, DISPGAMMA, OFFSET);
    outputCV.z = moncurve_r(linearCV.z, DISPGAMMA, OFFSET);

    outputCV = linear_to_REC709(linearCV);
    */

	// Unity already draws to a REC709 target
	return linearCV;
}

// <ACEStransformID>ODT.Academy.Rec709_100nits_dim.a1.0.3</ACEStransformID>
// <ACESuserName>ACES 1.0 Output - Rec.709</ACESuserName>

//
// Output Device Transform - Rec709
//

//
// Summary :
//  This transform is intended for mapping OCES onto a Rec.709 broadcast monitor
//  that is calibrated to a D65 white point at 100 cd/m^2. The assumed observer
//  adapted white is D65, and the viewing environment is a dim surround.
//
//  A possible use case for this transform would be HDTV/video mastering.
//
// Device Primaries :
//  Primaries are those specified in Rec. ITU-R BT.709
//  CIE 1931 chromaticities:  x         y         Y
//              Red:          0.64      0.33
//              Green:        0.3       0.6
//              Blue:         0.15      0.06
//              White:        0.3127    0.329     100 cd/m^2
//
// Display EOTF :
//  The reference electro-optical transfer function specified in
//  Rec. ITU-R BT.1886.
//
// Signal Range:
//    By default, this transform outputs full range code values. If instead a
//    SMPTE "legal" signal is desired, there is a runtime flag to output
//    SMPTE legal signal. In ctlrender, this can be achieved by appending
//    '-param1 legalRange 1' after the '-ctl odt.ctl' string.
//
// Assumed observer adapted white point:
//         CIE 1931 chromaticities:    x            y
//                                     0.3127       0.329
//
// Viewing Environment:
//   This ODT has a compensation for viewing environment variables more typical
//   of those associated with video mastering.
//
float3 ODT_Rec709_100nits_dim(float3 oces) {
	// OCES to RGB rendering space
	float3 rgbPre = aces_mul(AP0_2_AP1_MAT, oces);

	// Apply the tonescale independently in rendering-space RGB
	float3 rgbPost;
	rgbPost.x = segmented_spline_c9_fwd(rgbPre.x);
	rgbPost.y = segmented_spline_c9_fwd(rgbPre.y);
	rgbPost.z = segmented_spline_c9_fwd(rgbPre.z);

	// Scale luminance to linear code value
	float3 linearCV = Y_2_linCV(rgbPost, CINEMA_WHITE, CINEMA_BLACK);

	// Apply gamma adjustment to compensate for dim surround
	linearCV = darkSurround_to_dimSurround(linearCV);

	// Apply desaturation to compensate for luminance difference
	//linearCV = aces_mul(ODT_SAT_MAT, linearCV);
	linearCV = lerp(float3(aces_mul(AP1_2_XYZ_MAT, linearCV).y), linearCV, float3(ODT_SAT_FACTOR));

	linearCV = aces_mul(AP1_2_REC709, linearCV);

	// Handle out-of-gamut values
	// Clip values < 0 or > 1 (i.e. projecting outside the display primaries)
	linearCV = saturate(linearCV);

	// Encode linear code values with transfer function
	const float DISPGAMMA = 2.4;
	const float L_W = 1.0;
	const float L_B = 0.0;
	float3 outputCV = linear_to_bt1886(linearCV, DISPGAMMA, L_W, L_B);

	// TODO: Implement support for legal range.

	// NOTE: Unity framebuffer encoding is encoded with REC709 opto-electrical transfer function (OETF)
	// by default which will result in double perceptual encoding, thus for now if one want to use
	// this ODT, he needs to decode its output with REC709 electro-optical transfer function (EOTF) to
	// compensate for Unity default behaviour.

	return outputCV;
}

// <ACEStransformID>ODT.Academy.Rec709_D60sim_100nits_dim.a1.0.3</ACEStransformID>
// <ACESuserName>ACES 1.0 Output - Rec.709 (D60 sim.)</ACESuserName>

//
// Output Device Transform - Rec709 (D60 siaces_mulation)
//

//
// Summary :
//  This transform is intended for mapping OCES onto a Rec.709 broadcast monitor
//  that is calibrated to a D65 white point at 100 cd/m^2. The assumed observer
//  adapted white is D60, and the viewing environment is a dim surround.
//
//  A possible use case for this transform would be cinema "soft-proofing".
//
// Device Primaries :
//  Primaries are those specified in Rec. ITU-R BT.709
//  CIE 1931 chromaticities:  x         y         Y
//              Red:          0.64      0.33
//              Green:        0.3       0.6
//              Blue:         0.15      0.06
//              White:        0.3127    0.329     100 cd/m^2
//
// Display EOTF :
//  The reference electro-optical transfer function specified in
//  Rec. ITU-R BT.1886.
//
// Signal Range:
//    By default, this transform outputs full range code values. If instead a
//    SMPTE "legal" signal is desired, there is a runtime flag to output
//    SMPTE legal signal. In ctlrender, this can be achieved by appending
//    '-param1 legalRange 1' after the '-ctl odt.ctl' string.
//
// Assumed observer adapted white point:
//         CIE 1931 chromaticities:    x            y
//                                     0.32168      0.33767
//
// Viewing Environment:
//   This ODT has a compensation for viewing environment variables more typical
//   of those associated with video mastering.
//
float3 ODT_Rec709_D60sim_100nits_dim(float3 oces) {
	// OCES to RGB rendering space
	float3 rgbPre = aces_mul(AP0_2_AP1_MAT, oces);

	// Apply the tonescale independently in rendering-space RGB
	float3 rgbPost;
	rgbPost.x = segmented_spline_c9_fwd(rgbPre.x);
	rgbPost.y = segmented_spline_c9_fwd(rgbPre.y);
	rgbPost.z = segmented_spline_c9_fwd(rgbPre.z);

	// Scale luminance to linear code value
	float3 linearCV = Y_2_linCV(rgbPost, CINEMA_WHITE, CINEMA_BLACK);

	// --- Compensate for different white point being darker  --- //
	// This adjustment is to correct an issue that exists in ODTs where the device
	// is calibrated to a white chromaticity other than D60. In order to siaces_mulate
	// D60 on such devices, unequal code values must be sent to the display to achieve
	// the chromaticities of D60. More specifically, in order to produce D60 on a device
	// calibrated to a D65 white point (i.e. equal code values yield CIE x,y
	// chromaticities of 0.3127, 0.329) the red channel must be slightly higher than
	// that of green and blue in order to compensate for the relatively more "blue-ish"
	// D65 white. This unequalness of color channels is the correct behavior but it
	// means that as neutral highlights increase, the red channel will hit the
	// device maximum first and clip, resulting in a small chromaticity shift as the
	// green and blue channels continue to increase to their maximums.
	// To avoid this clipping error, a slight scale factor is applied to allow the
	// ODTs to siaces_mulate D60 within the D65 calibration white point.

	// Scale and clamp white to avoid casted highlights due to D60 siaces_mulation
	const float SCALE = 0.955;
	linearCV = min(linearCV, float3(1.0)) * SCALE;

	// Apply gamma adjustment to compensate for dim surround
	linearCV = darkSurround_to_dimSurround(linearCV);

	// Apply desaturation to compensate for luminance difference
	//linearCV = aces_mul(ODT_SAT_MAT, linearCV);
	linearCV = lerp(float3(aces_mul(AP1_2_XYZ_MAT, linearCV).y), linearCV, float3(ODT_SAT_FACTOR));

	// Convert to display primary encoding
	// Rendering space RGB to XYZ
	float3 XYZ = aces_mul(AP1_2_XYZ_MAT, linearCV);

	// CIE XYZ to display primaries
	linearCV = aces_mul(XYZ_2_REC709_MAT, XYZ);

	// Handle out-of-gamut values
	// Clip values < 0 or > 1 (i.e. projecting outside the display primaries)
	linearCV = saturate(linearCV);

	// Encode linear code values with transfer function
	const float DISPGAMMA = 2.4;
	const float L_W = 1.0;
	const float L_B = 0.0;
	float3 outputCV = linear_to_bt1886(linearCV, DISPGAMMA, L_W, L_B);

	// TODO: Implement support for legal range.

	// NOTE: Unity framebuffer encoding is encoded with REC709 opto-electrical transfer function (OETF)
	// by default which will result in double perceptual encoding, thus for now if one want to use
	// this ODT, he needs to decode its output with REC709 electro-optical transfer function (EOTF) to
	// compensate for Unity default behaviour.

	return outputCV;
}

// <ACEStransformID>ODT.Academy.Rec2020_100nits_dim.a1.0.3</ACEStransformID>
// <ACESuserName>ACES 1.0 Output - Rec.2020</ACESuserName>

//
// Output Device Transform - Rec2020
//

//
// Summary :
//  This transform is intended for mapping OCES onto a Rec.2020 broadcast
//  monitor that is calibrated to a D65 white point at 100 cd/m^2. The assumed
//  observer adapted white is D65, and the viewing environment is that of a dim
//  surround.
//
//  A possible use case for this transform would be UHDTV/video mastering.
//
// Device Primaries :
//  Primaries are those specified in Rec. ITU-R BT.2020
//  CIE 1931 chromaticities:  x         y         Y
//              Red:          0.708     0.292
//              Green:        0.17      0.797
//              Blue:         0.131     0.046
//              White:        0.3127    0.329     100 cd/m^2
//
// Display EOTF :
//  The reference electro-optical transfer function specified in
//  Rec. ITU-R BT.1886.
//
// Signal Range:
//    By default, this transform outputs full range code values. If instead a
//    SMPTE "legal" signal is desired, there is a runtime flag to output
//    SMPTE legal signal. In ctlrender, this can be achieved by appending
//    '-param1 legalRange 1' after the '-ctl odt.ctl' string.
//
// Assumed observer adapted white point:
//         CIE 1931 chromaticities:    x            y
//                                     0.3127       0.329
//
// Viewing Environment:
//   This ODT has a compensation for viewing environment variables more typical
//   of those associated with video mastering.
//

float3 ODT_Rec2020_100nits_dim(float3 oces) {
	// OCES to RGB rendering space
	float3 rgbPre = aces_mul(AP0_2_AP1_MAT, oces);

	// Apply the tonescale independently in rendering-space RGB
	float3 rgbPost;
	rgbPost.x = segmented_spline_c9_fwd(rgbPre.x);
	rgbPost.y = segmented_spline_c9_fwd(rgbPre.y);
	rgbPost.z = segmented_spline_c9_fwd(rgbPre.z);

	// Scale luminance to linear code value
	float3 linearCV = Y_2_linCV(rgbPost, CINEMA_WHITE, CINEMA_BLACK);

	// Apply gamma adjustment to compensate for dim surround
	linearCV = darkSurround_to_dimSurround(linearCV);

	// Apply desaturation to compensate for luminance difference
	//linearCV = aces_mul(ODT_SAT_MAT, linearCV);
	linearCV = lerp(float3(aces_mul(AP1_2_XYZ_MAT, linearCV).y), linearCV, float3(ODT_SAT_FACTOR));

	linearCV = aces_mul(AP1_2_REC2020, linearCV);

	// Handle out-of-gamut values
	// Clip values < 0 or > 1 (i.e. projecting outside the display primaries)
	linearCV = saturate(linearCV);

	// Encode linear code values with transfer function
	const float DISPGAMMA = 2.4;
	const float L_W = 1.0;
	const float L_B = 0.0;
	float3 outputCV = linear_to_bt1886(linearCV, DISPGAMMA, L_W, L_B);

	// TODO: Implement support for legal range.

	// NOTE: Unity framebuffer encoding is encoded with REC709 opto-electrical transfer function (OETF)
	// by default which will result in double perceptual encoding, thus for now if one want to use
	// this ODT, he needs to decode its output with REC709 electro-optical transfer function (EOTF) to
	// compensate for Unity default behaviour.

	return outputCV;
}

// <ACEStransformID>ODT.Academy.P3DCI_48nits.a1.0.3</ACEStransformID>
// <ACESuserName>ACES 1.0 Output - P3-DCI</ACESuserName>

//
// Output Device Transform - P3DCI (D60 Siaces_mulation)
//

//
// Summary :
//  This transform is intended for mapping OCES onto a P3 digital cinema
//  projector that is calibrated to a DCI white point at 48 cd/m^2. The assumed
//  observer adapted white is D60, and the viewing environment is that of a dark
//  theater.
//
// Device Primaries :
//  CIE 1931 chromaticities:  x         y         Y
//              Red:          0.68      0.32
//              Green:        0.265     0.69
//              Blue:         0.15      0.06
//              White:        0.314     0.351     48 cd/m^2
//
// Display EOTF :
//  Gamma: 2.6
//
// Assumed observer adapted white point:
//         CIE 1931 chromaticities:    x            y
//                                     0.32168      0.33767
//
// Viewing Environment:
//  Environment specified in SMPTE RP 431-2-2007
//
float3 ODT_P3DCI_48nits(float3 oces) {
	// OCES to RGB rendering space
	float3 rgbPre = aces_mul(AP0_2_AP1_MAT, oces);

	// Apply the tonescale independently in rendering-space RGB
	float3 rgbPost;
	rgbPost.x = segmented_spline_c9_fwd(rgbPre.x);
	rgbPost.y = segmented_spline_c9_fwd(rgbPre.y);
	rgbPost.z = segmented_spline_c9_fwd(rgbPre.z);

	// Scale luminance to linear code value
	float3 linearCV = Y_2_linCV(rgbPost, CINEMA_WHITE, CINEMA_BLACK);

	// --- Compensate for different white point being darker  --- //
	// This adjustment is to correct an issue that exists in ODTs where the device
	// is calibrated to a white chromaticity other than D60. In order to siaces_mulate
	// D60 on such devices, unequal code values are sent to the display to achieve
	// neutrals at D60. In order to produce D60 on a device calibrated to the DCI
	// white point (i.e. equal code values yield CIE x,y chromaticities of 0.314,
	// 0.351) the red channel is higher than green and blue to compensate for the
	// "greenish" DCI white. This is the correct behavior but it means that as
	// highlight increase, the red channel will hit the device maximum first and
	// clip, resulting in a chromaticity shift as the green and blue channels
	// continue to increase.
	// To avoid this clipping error, a slight scale factor is applied to allow the
	// ODTs to siaces_mulate D60 within the D65 calibration white point. However, the
	// magnitude of the scale factor required for the P3DCI ODT was considered too
	// large. Therefore, the scale factor was reduced and the additional required
	// compression was achieved via a reshaping of the highlight rolloff in
	// conjunction with the scale. The shape of this rolloff was determined
	// throught subjective experiments and deemed to best reproduce the
	// "character" of the highlights in the P3D60 ODT.

	// Roll off highlights to avoid need for as much scaling
	const float NEW_WHT = 0.918;
	const float ROLL_WIDTH = 0.5;
	linearCV.x = roll_white_fwd(linearCV.x, NEW_WHT, ROLL_WIDTH);
	linearCV.y = roll_white_fwd(linearCV.y, NEW_WHT, ROLL_WIDTH);
	linearCV.z = roll_white_fwd(linearCV.z, NEW_WHT, ROLL_WIDTH);

	// Scale and clamp white to avoid casted highlights due to D60 siaces_mulation
	const float SCALE = 0.96;
	linearCV = min(linearCV, float3(NEW_WHT)) * SCALE;

	// Convert to display primary encoding
	// Rendering space RGB to XYZ
	float3 XYZ = aces_mul(AP1_2_XYZ_MAT, linearCV);

	// CIE XYZ to display primaries
	linearCV = aces_mul(XYZ_2_DCIP3_MAT, XYZ);

	// Handle out-of-gamut values
	// Clip values < 0 or > 1 (i.e. projecting outside the display primaries)
	linearCV = saturate(linearCV);

	// Encode linear code values with transfer function
	const float DISPGAMMA = 2.6;
	float3 outputCV = pow(linearCV, 1.0 / DISPGAMMA);

	// NOTE: Unity framebuffer encoding is encoded with REC709 opto-electrical transfer function (OETF)
	// by default which will result in double perceptual encoding, thus for now if one want to use
	// this ODT, he needs to decode its output with REC709 electro-optical transfer function (EOTF) to
	// compensate for Unity default behaviour.

	return outputCV;
}

static float3 UnrealToneMapping(
	float3 color_ap0,
	float slope,
	float toe,
	float black_clip,
	float shoulder,
	float white_clip) {

	const float toe_scale = 1 + black_clip - toe;
	const float shoulder_scale = 1 + white_clip - shoulder;
	const float in_match = 0.18f;
	const float out_match = 0.18f;

	float toe_match;
	if (toe > 0.8) {
		toe_match = (1 - toe - out_match) / slope + log10(in_match);
	} else {
		const float bt = (out_match + black_clip) / toe_scale - 1;
		toe_match = log10(in_match) - 0.5 * log((1 + bt) / (1 - bt)) * (toe_scale / slope);
	}

	float straight_match = (1 - toe) / slope - toe_match;
	float shoulder_match = shoulder / slope - straight_match;

	float3 log_color = log10(color_ap0);
	float3 straight_color = slope * (log_color + straight_match);

	float3 toe_color = (-black_clip) + float3(2.0f * toe_scale) / (1.0f + exp((-2.0f * slope / toe_scale) * (log_color - toe_match)));
	float3 shoulder_color = float3(1.0f + white_clip) - float3(2 * shoulder_scale) / (1.0f + exp((2 * slope / shoulder_scale) * (log_color - shoulder_match)));

	toe_color = ite(log_color < toe_match, toe_color, straight_color);
	shoulder_color = ite(log_color > shoulder_match, shoulder_color, straight_color);

	float3 t = saturate((log_color - toe_match) / (shoulder_match - toe_match));
	t = shoulder_match < toe_match ? float3(1.0f) - t : t;
	t = (float3(3.0f) - 2.0f * t) * t * t;
	float3 tone_color = lerp(toe_color, shoulder_color, t);

	// Post desaturate
	tone_color = lerp(float3(aces_mul(AP1_2_XYZ_MAT, float3(tone_color)).y), tone_color, 0.93f);

	// Returning positive AP1 values
	return max(float3(0.0f), tone_color);
}

static float3 UnityACESToneMapping(
	float3 color_acescg) {
	// Luminance fitting of *RRT.a1.0.3 + ODT.Academy.RGBmonitor_100nits_dim.a1.0.3*.
	// https://github.com/colour-science/colour-unity/blob/master/Assets/Colour/Notebooks/CIECAM02_Unity.ipynb
	// RMSE: 0.0012846272106
	const float a = 278.5085;
	const float b = 10.7772;
	const float c = 293.6045;
	const float d = 88.7122;
	const float e = 80.6889;

	float3 x = color_acescg;
	return (x * (a * x + b)) / (x * (c * x + d) + e);
}

static float3 FilmicToneMappingExpr(
	float3 x,
	float shoulder_strength,
	float linear_strength,
	float linear_angle,
	float toe_strength,
	float toe_numerator,
	float toe_denominator) {

	float A = shoulder_strength;// default: 0.22
	float B = linear_strength;	// default: 0.30
	float C = linear_angle;		// default: 0.10
	float D = toe_strength;		// default: 0.20
	float E = toe_numerator;	// default: 0.01
	float F = toe_denominator;	// default: 0.30
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

static float3 FilmicToneMapping(
	float3 color_acescg,
	float shoulder_strength,
	float linear_strength,
	float linear_angle,
	float toe_strength,
	float toe_numerator,
	float toe_denominator) {
	// http://filmicworlds.com/blog/filmic-tonemapping-with-piecewise-power-curves/

	float3 linear_color = FilmicToneMappingExpr(
		color_acescg,
		shoulder_strength,
		linear_strength,
		linear_angle,
		toe_strength,
		toe_numerator,
		toe_denominator);
	float3 linear_white = FilmicToneMappingExpr(
		float3(11.2f),
		shoulder_strength,
		linear_strength,
		linear_angle,
		toe_strength,
		toe_numerator,
		toe_denominator);
	return linear_color / linear_white;
}

float3 AcesTonemap(
	float3 aces,
	int tone_mode,
	// unreal tone
	float unreal_tone_slope,
	float unreal_tone_toe,
	float unreal_tone_black_clip,
	float unreal_tone_shoulder,
	float unreal_tone_white_clip,
	// filmic tone
	float filmic_tone_shoulder_strength,
	float filmic_tone_linear_strength,
	float filmic_tone_linear_angle,
	float filmic_tone_toe_strength,
	float filmic_tone_toe_numerator,
	float filmic_tone_toe_denominator) {

	float saturation = rgb_2_saturation(aces);
	float ycIn = rgb_2_yc(aces);
	float s = sigmoid_shaper((saturation - 0.4) / 0.2);
	float addedGlow = 1.0 + glow_fwd(ycIn, RRT_GLOW_GAIN * s, RRT_GLOW_MID);
	aces *= addedGlow;

	// --- Red modifier --- //
	float hue = rgb_2_hue(aces);
	float centeredHue = center_hue(hue, RRT_RED_HUE);
	float hueWeight;
	{
		//hueWeight = cubic_basis_shaper(centeredHue, RRT_RED_WIDTH);
		hueWeight = smoothstep(0.0, 1.0, 1.0 - abs(2.0 * centeredHue / RRT_RED_WIDTH));
		hueWeight *= hueWeight;
	}

	aces.r += hueWeight * saturation * (RRT_RED_PIVOT - aces.r) * (1.0 - RRT_RED_SCALE);

	// --- ACES to RGB rendering space --- //
	float3 acescg = max(float3(0.0f), ACES_to_ACEScg(aces));

	// --- Global desaturation --- //
	//acescg = mul(RRT_SAT_MAT, acescg);
	acescg = lerp(float3(aces_mul(AP1_2_XYZ_MAT, acescg).y), acescg, float3(RRT_SAT_FACTOR));

	// const float a = 278.5085;
	// const float b = 10.7772;
	// const float c = 293.6045;
	// const float d = 88.7122;
	// const float e = 80.6889;
	// float3 x = acescg;
	// float3 rgbPost = (x * (a * x + b)) / (x * (c * x + d) + e);

	// Unreal ToneMapping???????????????????
	// float ga = 0.8; // slope
	// float t0 = 0.5; // Toe
	// float t1 = 0;	// Black clip
	// float s0 = 0.3; // Shoulder
	// float s1 = 0.04;// White clip
	float3 rgbPost;
	force_case();
	switch (tone_mode) {
		case 0:
			rgbPost = UnrealToneMapping(
				acescg,
				unreal_tone_slope,
				unreal_tone_toe,
				unreal_tone_black_clip,
				unreal_tone_shoulder,
				unreal_tone_white_clip);
			break;
		case 1:
			rgbPost = UnityACESToneMapping(
				acescg);
			break;
		case 2:
			rgbPost = FilmicToneMapping(
				acescg,
				filmic_tone_shoulder_strength,
				filmic_tone_linear_strength,
				filmic_tone_linear_angle,
				filmic_tone_toe_strength,
				filmic_tone_toe_numerator,
				filmic_tone_toe_denominator);
			break;
	}

	// Scale luminance to linear code value
	// float3 linearCV = Y_2_linCV(rgbPost, CINEMA_WHITE, CINEMA_BLACK);
	float3 linearCV = rgbPost;
	if (tone_mode != 0) {
		// The only difference between UE and other tonemapping is that it lacks this

		// Apply gamma adjustment to compensate for dim surround
		linearCV = darkSurround_to_dimSurround(linearCV);
	}
	// Apply desaturation to compensate for luminance difference
	//linearCV = mul(ODT_SAT_MAT, color);
	linearCV = lerp(float3(aces_mul(AP1_2_XYZ_MAT, linearCV).y), linearCV, float3(ODT_SAT_FACTOR));

	return linearCV;
}
