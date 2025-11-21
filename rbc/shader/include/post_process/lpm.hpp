#include <luisa/std.hpp>
#include <luisa/printer.hpp>
using namespace luisa::shader;
static bool LpmD(float a, float b) {
	return abs(a - b) < 1.0;
}

static float LpmC(float a, float b) {
	float c = 1.0;// 6-bits or less (the color)
	if (LpmD(a * 127.0, b * 127.0))
		c = 0.875;// 7-bits
	if (LpmD(a * 255.0, b * 255.0))
		c = 0.5;// 8-bits
	if (LpmD(a * 512.0, b * 512.0))
		c = 0.125;// 9-bits
	if (LpmD(a * 1024.0, b * 1024.0))
		c = 0.0;// 10-bits or better (black)
	return c;
}

template<concepts::arithmetic T>
T max3(T a, T b, T c) {
	return max(a, max(b, c));
}

static float3 LpmViewDiff(float3 a, float3 b) {
	return float3(LpmC(a.x, b.x), LpmC(a.y, b.y), LpmC(a.z, b.z));
}
static void LpmMap(float& colorR,
				   float& colorG,
				   float& colorB,		  // Input and output color.
				   float3 lumaW,		  // Luma coef for RGB working space.
				   float3 lumaT,		  // Luma coef for crosstalk mapping (can be working or output color-space depending on usage case).
				   float3 rcpLumaT,		  // 1/lumaT.
				   float3 saturation,	  // Saturation powers.
				   float contrast,		  // Contrast power.
				   bool shoulder,		  // Using shoulder tuning (should be a compile-time immediate).
				   float shoulderContrast,// Shoulder power.
				   float2 toneScaleBias,  // Other tonemapping parameters.
				   float3 crosstalk,	  // Crosstalk scaling for over-exposure color shaping.
				   bool con,			  // Use first RGB conversion matrix (should be a compile-time immediate), if 'soft' then 'con' must be true also.
				   float3 conR,
				   float3 conG,
				   float3 conB,	  // RGB conversion matrix (working to output space conversion).
				   bool soft,	  // Use soft gamut mapping (should be a compile-time immediate).
				   float2 softGap,// {x,(1-x)/(x*0.693147180559)}, where 'x' is gamut mapping soft fall-off amount.
				   bool con2,	  // Use last RGB conversion matrix (should be a compile-time immediate).
				   bool clip,	  // Use clipping on last conversion matrix.
				   bool scaleOnly,// Do scaling only (special case for 709 HDR to scRGB).
				   float3 con2R,
				   float3 con2G,
				   float3 con2B) {
	// Secondary RGB conversion matrix.
	// Grab original RGB ratio (RCP, 3x MUL, MAX3).
	float rcpMax = rcp(max3(colorR, colorG, colorB));
	float ratioR = colorR * rcpMax;
	float ratioG = colorG * rcpMax;
	float ratioB = colorB * rcpMax;

	// Apply saturation, ratio must be max 1.0 for this to work right (3x EXP2, 3x LOG2, 3x MUL).
	ratioR = pow(ratioR, float(saturation.x));
	ratioG = pow(ratioG, float(saturation.y));
	ratioB = pow(ratioB, float(saturation.z));

	// Tonemap luma, note this uses the original color, so saturation is luma preserving.
	// If not using 'con' this uses the output space luma directly to avoid needing extra constants.
	// Note 'soft' should be a compile-time immediate (so no branch) (3x MAD).
	float luma;
	if (soft)
		luma = colorG * float(lumaW.y) + (colorR * float(lumaW.x) + (colorB * float(lumaW.z)));
	else
		luma = colorG * float(lumaT.y) + (colorR * float(lumaT.x) + (colorB * float(lumaT.z)));
	luma = pow(luma, float(contrast));												  // (EXP2, LOG2, MUL).
	float lumaShoulder = ite(shoulder, pow(luma, float(shoulderContrast)), luma);	  // Optional (EXP2, LOG2, MUL).
	luma = luma * rcp(lumaShoulder * float(toneScaleBias.x) + float(toneScaleBias.y));// (MAD, MUL, RCP).
	// If running soft clipping (this should be a compile-time immediate so branch will not exist).
	if (soft) {
		// The 'con' should be a compile-time immediate so branch will not exist.
		// Use of 'con' is implied if soft-falloff is enabled, but using the check here to make finding bugs easy.
		if (con) {
			// Converting ratio instead of color. Change of primaries (9x MAD).
			colorR = ratioR;
			colorG = ratioG;
			colorB = ratioB;
			ratioR = colorR * float(conR.x) + (colorG * float(conR.y) + (colorB * float(conR.z)));
			ratioG = colorG * float(conG.y) + (colorR * float(conG.x) + (colorB * float(conG.z)));
			ratioB = colorB * float(conB.z) + (colorG * float(conB.y) + (colorR * float(conB.x)));

			// Convert ratio to max 1 again (RCP, 3x MUL, MAX3).
			rcpMax = rcp(max3(ratioR, ratioG, ratioB));
			ratioR *= rcpMax;
			ratioG *= rcpMax;
			ratioB *= rcpMax;
		}

		// Absolute gamut mapping converted to soft falloff (maintains max 1 property).
		//  g = gap {0 to g} used for {-inf to 0} input range
		//          {g to 1} used for {0 to 1} input range
		//  x >= 0 := y = x * (1-g) + g
		//  x < 0  := g * 2^(x*h)
		//  Where h=(1-g)/(g*log(2)) --- where log() is the natural log
		// The {g,h} above is passed in as softGap.
		// Soft falloff (3x MIN, 3x MAX, 9x MAD, 3x EXP2).
		ratioR = min(max(float(softGap.x), saturate(ratioR * float(-softGap.x) + ratioR)),
					 saturate(float(softGap.x) * exp2(ratioR * float(softGap.y))));
		ratioG = min(max(float(softGap.x), saturate(ratioG * float(-softGap.x) + ratioG)),
					 saturate(float(softGap.x) * exp2(ratioG * float(softGap.y))));
		ratioB = min(max(float(softGap.x), saturate(ratioB * float(-softGap.x) + ratioB)),
					 saturate(float(softGap.x) * exp2(ratioB * float(softGap.y))));
	}

	// Compute ratio scaler required to hit target luma (4x MAD, 1 RCP).
	float lumaRatio = ratioR * float(lumaT.x) + ratioG * float(lumaT.y) + ratioB * float(lumaT.z);

	// This is limited to not clip.
	float ratioScale = saturate(luma * rcp(lumaRatio));

	// Assume in gamut, compute output color (3x MAD).
	colorR = saturate(ratioR * ratioScale);
	colorG = saturate(ratioG * ratioScale);
	colorB = saturate(ratioB * ratioScale);

	// Capability per channel to increase value (3x MAD).
	// This factors in crosstalk factor to avoid multiplies later.
	//  '(1.0-ratio)*crosstalk' optimized to '-crosstalk*ratio+crosstalk'
	float capR = float(-crosstalk.x) * colorR + float(crosstalk.x);
	float capG = float(-crosstalk.y) * colorG + float(crosstalk.y);
	float capB = float(-crosstalk.z) * colorB + float(crosstalk.z);

	// Compute amount of luma needed to add to non-clipped channels to make up for clipping (3x MAD).
	float lumaAdd = saturate((-colorB) * float(lumaT.z) + ((-colorR) * float(lumaT.x) + ((-colorG) * float(lumaT.y) + luma)));

	// Amount to increase keeping over-exposure ratios constant and possibly exceeding clipping point (4x MAD, 1 RCP).
	float t = lumaAdd * rcp(capG * float(lumaT.y) + (capR * float(lumaT.x) + (capB * float(lumaT.z))));

	// Add amounts to base color but clip (3x MAD).
	colorR = saturate(t * capR + colorR);
	colorG = saturate(t * capG + colorG);
	colorB = saturate(t * capB + colorB);

	// Compute amount of luma needed to add to non-clipped channel to make up for clipping (3x MAD).
	lumaAdd = saturate((-colorB) * float(lumaT.z) + ((-colorR) * float(lumaT.x) + ((-colorG) * float(lumaT.y) + luma)));

	// Add to last channel (3x MAD).
	colorR = saturate(lumaAdd * float(rcpLumaT.x) + colorR);
	colorG = saturate(lumaAdd * float(rcpLumaT.y) + colorG);
	colorB = saturate(lumaAdd * float(rcpLumaT.z) + colorB);

	// The 'con2' should be a compile-time immediate so branch will not exist.
	// Last optional place to convert from smaller to larger gamut (or do clipped conversion).
	// For the non-soft-falloff case, doing this after all other mapping saves intermediate re-scaling ratio to max 1.0.
	// con2R = abs(con2R);
	// con2G = abs(con2G);
	// con2B = abs(con2B);

	if (con2) {
		// Change of primaries (9x MAD).
		ratioR = colorR;
		ratioG = colorG;
		ratioB = colorB;

		if (clip) {
			colorR = saturate(ratioR * float(con2R.x) + (ratioG * float(con2R.y) + (ratioB * float(con2R.z))));
			colorG = saturate(ratioG * float(con2G.y) + (ratioR * float(con2G.x) + (ratioB * float(con2G.z))));
			colorB = saturate(ratioB * float(con2B.z) + (ratioG * float(con2B.y) + (ratioR * float(con2B.x))));
		} else {
			colorR = ratioR * float(con2R.x) + (ratioG * float(con2R.y) + (ratioB * float(con2R.z)));
			colorG = ratioG * float(con2G.y) + (ratioR * float(con2G.x) + (ratioB * float(con2G.z)));
			colorB = ratioB * float(con2B.z) + (ratioG * float(con2B.y) + (ratioR * float(con2B.x)));
		}
	}
	if (scaleOnly) {
		colorR *= float(con2R.x);
		colorG *= float(con2R.x);
		colorB *= float(con2R.x);
	}
	// colorR = max(0.f, colorR);
	// colorG = max(0.f, colorG);
	// colorB = max(0.f, colorB);
}
static void LpmFilter(
	// Input and output color.
	std::array<uint4, 10> ctl,
	float& colorR,
	float& colorG,
	float& colorB,
	// Path control should all be compile-time immediates.
	bool shoulder,// Using shoulder tuning.
	// Prefab "LPM_CONFIG_" start, use the same as used for LpmSetup().
	bool con,	   // Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
	bool soft,	   // Use soft gamut mapping.
	bool con2,	   // Use last RGB conversion matrix.
	bool clip,	   // Use clipping in last conversion matrix.
	bool scaleOnly)// Scale only for last conversion matrix (used for 709 HDR to scRGB).
{
	// Grab control block, what is unused gets dead-code removal.
	uint4 map0 = ctl[0];
	uint4 map1 = ctl[1];
	uint4 map2 = ctl[2];
	uint4 map3 = ctl[3];
	uint4 map4 = ctl[4];
	uint4 map5 = ctl[5];
	uint4 map6 = ctl[6];
	uint4 map7 = ctl[7];
	uint4 map8 = ctl[8];
	uint4 map9 = ctl[9];

	LpmMap(colorR,
		   colorG,
		   colorB,
		   float3(bit_cast<float4>(map6).yzw),						   // lumaW
		   float3(bit_cast<float4>(map1).zw, bit_cast<float4>(map2).x),// lumaT
		   float3(bit_cast<float4>(map3).xyz),						   // rcpLumaT
		   float3(bit_cast<float4>(map0).xyz),						   // saturation
		   bit_cast<float4>(map0).w,								   // contrast
		   shoulder,
		   bit_cast<float4>(map6).x,		  // shoulderContrast
		   float2(bit_cast<float4>(map1).xy), // toneScaleBias
		   float3(bit_cast<float4>(map2).yzw),// crosstalk
		   con,
		   float3(bit_cast<float4>(map7).zw, bit_cast<float4>(map8).x),// conR
		   float3(bit_cast<float4>(map8).yzw),						   // conG
		   float3(bit_cast<float4>(map9).xyz),						   // conB
		   soft,
		   float2(bit_cast<float4>(map7).xy),// softGap
		   con2,
		   clip,
		   scaleOnly,
		   float3(bit_cast<float4>(map3).w, bit_cast<float4>(map4).xy),// con2R
		   float3(bit_cast<float4>(map4).zw, bit_cast<float4>(map5).x),// con2G
		   float3(bit_cast<float4>(map5).yzw));						   // con2B
}