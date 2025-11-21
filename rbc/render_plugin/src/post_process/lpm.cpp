#include <rbc_render/post_process/lpm.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <rbc_graphics/scene_manager.h>
namespace rbc {
namespace ffx {
template<typename T>
static T reciprocal(T a) {
	return T(1.f) / a;
}
struct LpmConstants {
	std::array<uint4, 10> ctl;
	uint shoulder; // Use optional extra shoulderContrast tuning (set to false if shoulderContrast is 1.0).
	uint con;	   // Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
	uint soft;	   // Use soft gamut mapping.
	uint con2;	   // Use last RGB conversion matrix.
	uint clip;	   // Use clipping in last conversion matrix.
	uint scaleOnly;// Scale only for last conversion matrix (used for 709 HDR to scRGB).
	std::array<uint, 2> pad;
};
static void LpmMatInv3x3(float3& ox, float3& oy, float3& oz, float3 ix, float3 iy, float3 iz) {
	float i = reciprocal(ix[0] * (iy[1] * iz[2] - iz[1] * iy[2]) - ix[1] * (iy[0] * iz[2] - iy[2] * iz[0]) + ix[2] * (iy[0] * iz[1] - iy[1] * iz[0]));
	ox[0] = (iy[1] * iz[2] - iz[1] * iy[2]) * i;
	ox[1] = (ix[2] * iz[1] - ix[1] * iz[2]) * i;
	ox[2] = (ix[1] * iy[2] - ix[2] * iy[1]) * i;
	oy[0] = (iy[2] * iz[0] - iy[0] * iz[2]) * i;
	oy[1] = (ix[0] * iz[2] - ix[2] * iz[0]) * i;
	oy[2] = (iy[0] * ix[2] - ix[0] * iy[2]) * i;
	oz[0] = (iy[0] * iz[1] - iz[0] * iy[1]) * i;
	oz[1] = (iz[0] * ix[1] - ix[0] * iz[1]) * i;
	oz[2] = (ix[0] * iy[1] - iy[0] * ix[1]) * i;
}

// Transpose.
static void LpmMatTrn3x3(float3& ox, float3& oy, float3& oz, float3 ix, float3 iy, float3 iz) {
	ox[0] = ix[0];
	ox[1] = iy[0];
	ox[2] = iz[0];
	oy[0] = ix[1];
	oy[1] = iy[1];
	oy[2] = iz[1];
	oz[0] = ix[2];
	oz[1] = iy[2];
	oz[2] = iz[2];
}

static void LpmMatMul3x3(
	float3& ox, float3& oy, float3& oz, float3 ax, float3 ay, float3 az, float3 bx, float3 by, float3 bz) {
	float3 bx2;
	float3 by2;
	float3 bz2;

	LpmMatTrn3x3(bx2, by2, bz2, bx, by, bz);
	ox[0] = dot(ax, bx2);
	ox[1] = dot(ax, by2);
	ox[2] = dot(ax, bz2);
	oy[0] = dot(ay, bx2);
	oy[1] = dot(ay, by2);
	oy[2] = dot(ay, bz2);
	oz[0] = dot(az, bx2);
	oz[1] = dot(az, by2);
	oz[2] = dot(az, bz2);
}
static float LpmFs2ScrgbScalar(float minLuma, float maxLuma) {
	// Queried display properties.
	return ((maxLuma - minLuma) + minLuma) * (float(1.0) / float(80.0));
}
// D65 xy coordinates.
static const float2 lpmColD65 = {float(0.3127), float(0.3290)};

// Rec709 xy coordinates, (D65 white point).
static const float2 lpmCol709R = {float(0.64), float(0.33)};
static const float2 lpmCol709G = {float(0.30), float(0.60)};
static const float2 lpmCol709B = {float(0.15), float(0.06)};

// DCI-P3 xy coordinates, (D65 white point).
static const float2 lpmColP3R = {float(0.680), float(0.320)};
static const float2 lpmColP3G = {float(0.265), float(0.690)};
static const float2 lpmColP3B = {float(0.150), float(0.060)};

// Rec2020 xy coordinates, (D65 white point).
static const float2 lpmCol2020R = {float(0.708), float(0.292)};
static const float2 lpmCol2020G = {float(0.170), float(0.797)};
static const float2 lpmCol2020B = {float(0.131), float(0.046)};

#define LPM_CONFIG_FS2RAW_709 false, false, true, true, false
#define LPM_COLORS_FS2RAW_709 \
	lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, fs2R, fs2G, fs2B, fs2W, float(1.0)
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAWPQ_709 false, false, true, true, false
#define LPM_COLORS_FS2RAWPQ_709 \
	lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// FreeSync2 min-spec is larger than sRGB, so using 709 primaries all the way through as an optimization.
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2SCRGB_709 false, false, false, false, true
#define LPM_COLORS_FS2SCRGB_709 \
	lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, fs2S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10RAW_709 false, false, true, true, false
#define LPM_COLORS_HDR10RAW_709 \
	lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10SCRGB_709 false, false, false, false, true
#define LPM_COLORS_HDR10SCRGB_709 \
	lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_709_709 false, false, false, false, false
#define LPM_COLORS_709_709 \
	lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, float(1.0)
//==============================================================================================================================
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAW_P3 true, true, false, false, false
#define LPM_COLORS_FS2RAW_P3 lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, fs2R, fs2G, fs2B, fs2W, fs2R, fs2G, fs2B, fs2W, float(1.0)
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAWPQ_P3 true, true, true, false, false
#define LPM_COLORS_FS2RAWPQ_P3 lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, fs2R, fs2G, fs2B, fs2W, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// FreeSync2 gamut can be smaller than P3.
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2SCRGB_P3 true, true, true, false, false
#define LPM_COLORS_FS2SCRGB_P3 lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, fs2R, fs2G, fs2B, fs2W, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, fs2S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10RAW_P3 false, false, true, true, false
#define LPM_COLORS_HDR10RAW_P3 \
	lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10SCRGB_P3 false, false, true, false, false
#define LPM_COLORS_HDR10SCRGB_P3 \
	lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_709_P3 true, true, false, false, false
#define LPM_COLORS_709_P3 \
	lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, float(1.0)
//==============================================================================================================================
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAW_2020 true, true, false, false, false
#define LPM_COLORS_FS2RAW_2020 lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, fs2R, fs2G, fs2B, fs2W, fs2R, fs2G, fs2B, fs2W, float(1.0)
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAWPQ_2020 true, true, true, false, false
#define LPM_COLORS_FS2RAWPQ_2020 lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, fs2R, fs2G, fs2B, fs2W, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2SCRGB_2020 true, true, true, false, false
#define LPM_COLORS_FS2SCRGB_2020 lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, fs2R, fs2G, fs2B, fs2W, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, fs2S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10RAW_2020 false, false, false, false, true
#define LPM_COLORS_HDR10RAW_2020 \
	lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10SCRGB_2020 false, false, true, false, false
#define LPM_COLORS_HDR10SCRGB_2020 \
	lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_709_2020 true, true, false, false, false
#define LPM_COLORS_709_2020                                                                                                                         \
	lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, \
		float(1.0)

// Computes z from xy, returns xyz.
static void LpmColXyToZ(float3& d, float2 s) {
	d[0] = s[0];
	d[1] = s[1];
	d[2] = float(1.0) - (s[0] + s[1]);
}

static void opAMulOneF3(float3& d, float3 a, float b) {
	d = a * b;
	return;
}
static void opAMulF3(float3& d, float3 a, float3 b) {
	d = a * b;
	return;
}
static void opAAddOneF3(float3& d, float3 a, float b) {
	d = a + b;
	return;
}
static float LpmHdr10RawScalar(float peakNits) {
	return peakNits * (float(1.0) / float(10000.0));
}
static float LpmHdr10ScrgbScalar(float peakNits) {
	return peakNits * (float(1.0) / float(10000.0)) * (float(10000.0) / float(80.0));
}

// Returns conversion matrix, rgbw inputs are xy chroma coordinates.
static void LpmColRgbToXyz(float3& ox, float3& oy, float3& oz, float2 r, float2 g, float2 b, float2 w) {
	// Expand from xy to xyz.
	float3 rz;
	float3 gz;
	float3 bz;
	LpmColXyToZ(rz, r);
	LpmColXyToZ(gz, g);
	LpmColXyToZ(bz, b);

	float3 r3;
	float3 g3;
	float3 b3;
	LpmMatTrn3x3(r3, g3, b3, rz, gz, bz);

	// Convert white xyz to XYZ.
	float3 w3;
	LpmColXyToZ(w3, w);
	opAMulOneF3(w3, w3, reciprocal(w[1]));

	// Compute xyz to XYZ scalars for primaries.
	float3 rv;
	float3 gv;
	float3 bv;
	LpmMatInv3x3(rv, gv, bv, r3, g3, b3);

	float3 s;
	s[0] = dot(rv, w3);
	s[1] = dot(gv, w3);
	s[2] = dot(bv, w3);

	// Scale.
	opAMulF3(ox, r3, s);
	opAMulF3(oy, g3, s);
	opAMulF3(oz, b3, s);
}
static void CalculateLpmConsts(uint* ctl,
							   // Path control.
							   bool shoulder,// Use optional extra shoulderContrast tuning (set to false if shoulderContrast is 1.0).

							   // Prefab start, "LPM_CONFIG_".
							   bool con,	  // Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
							   bool soft,	  // Use soft gamut mapping.
							   bool con2,	  // Use last RGB conversion matrix.
							   bool clip,	  // Use clipping in last conversion matrix.
							   bool scaleOnly,// Scale only for last conversion matrix (used for 709 HDR to scRGB).

							   // Gamut control, "LPM_COLORS_".
							   float2 xyRedW,
							   float2 xyGreenW,
							   float2 xyBlueW,
							   float2 xyWhiteW,// Chroma coordinates for working color space.
							   float2 xyRedO,
							   float2 xyGreenO,
							   float2 xyBlueO,
							   float2 xyWhiteO,// For the output color space.
							   float2 xyRedC,
							   float2 xyGreenC,
							   float2 xyBlueC,
							   float2 xyWhiteC,
							   float scaleC,// For the output container color space (if con2).

							   // Prefab end.
							   float softGap,// Range of 0 to a little over zero, controls how much feather region in out-of-gamut mapping, 0=clip.

							   // Tonemapping control.
							   float hdrMax,		  // Maximum input value.
							   float exposure,		  // Number of stops between 'hdrMax' and 18% mid-level on input.
							   float contrast,		  // Input range {0.0 (no extra contrast) to 1.0 (maximum contrast)}.
							   float shoulderContrast,// Shoulder shaping, 1.0 = no change (fast path).
							   float3 saturation,	  // A per channel adjustment, use <0 decrease, 0=no change, >0 increase.
							   float3 crosstalk)	  // One channel must be 1.0, the rest can be <= 1.0 but not zero.
{
	auto LpmSetupOut = [&](uint i, uint4 v) {
		for (int j = 0; j < 4; ++j) {
			ctl[i * 4 + j] = v[j];
		}
	};
	// Contrast needs to be 1.0 based for no contrast.
	contrast += float(1.0);

	// Saturation is based on contrast.
	opAAddOneF3(saturation, saturation, contrast);

	// The 'softGap' must actually be above zero.
	softGap = std::max(softGap, float(1.0 / 1024.0));

	float midIn = hdrMax * float(0.18) * exp2(-exposure);
	float midOut = float(0.18);

	float2 toneScaleBias;
	float cs = contrast * shoulderContrast;
	float z0 = -pow(midIn, contrast);
	float z1 = pow(hdrMax, cs) * pow(midIn, contrast);
	float z2 = pow(hdrMax, contrast) * pow(midIn, cs) * midOut;
	float z3 = pow(hdrMax, cs) * midOut;
	float z4 = pow(midIn, cs) * midOut;
	toneScaleBias[0] = -((z0 + (midOut * (z1 - z2)) * reciprocal(z3 - z4)) * reciprocal(z4));

	float w0 = pow(hdrMax, cs) * pow(midIn, contrast);
	float w1 = pow(hdrMax, contrast) * pow(midIn, cs) * midOut;
	float w2 = pow(hdrMax, cs) * midOut;
	float w3 = pow(midIn, cs) * midOut;
	toneScaleBias[1] = (w0 - w1) * reciprocal(w2 - w3);

	float3 lumaW;
	float3 rgbToXyzXW;
	float3 rgbToXyzYW;
	float3 rgbToXyzZW;
	LpmColRgbToXyz(rgbToXyzXW, rgbToXyzYW, rgbToXyzZW, xyRedW, xyGreenW, xyBlueW, xyWhiteW);

	// Use the Y vector of the matrix for the associated luma coef.
	// For safety, make sure the vector sums to 1.0.
	opAMulOneF3(lumaW, rgbToXyzYW, reciprocal(rgbToXyzYW[0] + rgbToXyzYW[1] + rgbToXyzYW[2]));

	// The 'lumaT' for crosstalk mapping is always based on the output color space, unless soft conversion is not used.
	float3 lumaT;
	float3 rgbToXyzXO;
	float3 rgbToXyzYO;
	float3 rgbToXyzZO;
	LpmColRgbToXyz(rgbToXyzXO, rgbToXyzYO, rgbToXyzZO, xyRedO, xyGreenO, xyBlueO, xyWhiteO);

	lumaT = soft ? rgbToXyzYO : rgbToXyzYW;

	opAMulOneF3(lumaT, lumaT, reciprocal(lumaT[0] + lumaT[1] + lumaT[2]));
	float3 rcpLumaT = 1.f / lumaT;

	float2 softGap2 = {0.0, 0.0};
	if (soft) {
		softGap2[0] = softGap;
		softGap2[1] = (float(1.0) - softGap) * reciprocal(softGap * float(0.693147180559));
	}

	// First conversion is always working to output.
	float3 conR = {0.0, 0.0, 0.0};
	float3 conG = {0.0, 0.0, 0.0};
	float3 conB = {0.0, 0.0, 0.0};

	if (con) {
		float3 xyzToRgbRO;
		float3 xyzToRgbGO;
		float3 xyzToRgbBO;
		LpmMatInv3x3(xyzToRgbRO, xyzToRgbGO, xyzToRgbBO, rgbToXyzXO, rgbToXyzYO, rgbToXyzZO);
		LpmMatMul3x3(conR, conG, conB, xyzToRgbRO, xyzToRgbGO, xyzToRgbBO, rgbToXyzXW, rgbToXyzYW, rgbToXyzZW);
	}

	// The last conversion is always output to container.
	float3 con2R = {0.0, 0.0, 0.0};
	float3 con2G = {0.0, 0.0, 0.0};
	float3 con2B = {0.0, 0.0, 0.0};

	if (con2) {
		float3 rgbToXyzXC;
		float3 rgbToXyzYC;
		float3 rgbToXyzZC;
		LpmColRgbToXyz(rgbToXyzXC, rgbToXyzYC, rgbToXyzZC, xyRedC, xyGreenC, xyBlueC, xyWhiteC);

		float3 xyzToRgbRC;
		float3 xyzToRgbGC;
		float3 xyzToRgbBC;
		LpmMatInv3x3(xyzToRgbRC, xyzToRgbGC, xyzToRgbBC, rgbToXyzXC, rgbToXyzYC, rgbToXyzZC);
		LpmMatMul3x3(con2R, con2G, con2B, xyzToRgbRC, xyzToRgbGC, xyzToRgbBC, rgbToXyzXO, rgbToXyzYO, rgbToXyzZO);
		opAMulOneF3(con2R, con2R, scaleC);
		opAMulOneF3(con2G, con2G, scaleC);
		opAMulOneF3(con2B, con2B, scaleC);
	}

	if (scaleOnly)
		con2R[0] = scaleC;
	uint4 map0;
	map0[0] = reinterpret_cast<uint&>(saturation[0]);
	map0[1] = reinterpret_cast<uint&>(saturation[1]);
	map0[2] = reinterpret_cast<uint&>(saturation[2]);
	map0[3] = reinterpret_cast<uint&>(contrast);
	LpmSetupOut(0, map0);

	uint4 map1;
	map1[0] = reinterpret_cast<uint&>(toneScaleBias[0]);
	map1[1] = reinterpret_cast<uint&>(toneScaleBias[1]);
	map1[2] = reinterpret_cast<uint&>(lumaT[0]);
	map1[3] = reinterpret_cast<uint&>(lumaT[1]);
	LpmSetupOut(1, map1);

	uint4 map2;
	map2[0] = reinterpret_cast<uint&>(lumaT[2]);
	map2[1] = reinterpret_cast<uint&>(crosstalk[0]);
	map2[2] = reinterpret_cast<uint&>(crosstalk[1]);
	map2[3] = reinterpret_cast<uint&>(crosstalk[2]);
	LpmSetupOut(2, map2);

	uint4 map3;
	map3[0] = reinterpret_cast<uint&>(rcpLumaT[0]);
	map3[1] = reinterpret_cast<uint&>(rcpLumaT[1]);
	map3[2] = reinterpret_cast<uint&>(rcpLumaT[2]);
	map3[3] = reinterpret_cast<uint&>(con2R[0]);
	LpmSetupOut(3, map3);

	uint4 map4;
	map4[0] = reinterpret_cast<uint&>(con2R[1]);
	map4[1] = reinterpret_cast<uint&>(con2R[2]);
	map4[2] = reinterpret_cast<uint&>(con2G[0]);
	map4[3] = reinterpret_cast<uint&>(con2G[1]);
	LpmSetupOut(4, map4);

	uint4 map5;
	map5[0] = reinterpret_cast<uint&>(con2G[2]);
	map5[1] = reinterpret_cast<uint&>(con2B[0]);
	map5[2] = reinterpret_cast<uint&>(con2B[1]);
	map5[3] = reinterpret_cast<uint&>(con2B[2]);
	LpmSetupOut(5, map5);

	uint4 map6;
	map6[0] = reinterpret_cast<uint&>(shoulderContrast);
	map6[1] = reinterpret_cast<uint&>(lumaW[0]);
	map6[2] = reinterpret_cast<uint&>(lumaW[1]);
	map6[3] = reinterpret_cast<uint&>(lumaW[2]);
	LpmSetupOut(6, map6);

	uint4 map7;
	map7[0] = reinterpret_cast<uint&>(softGap2[0]);
	map7[1] = reinterpret_cast<uint&>(softGap2[1]);
	map7[2] = reinterpret_cast<uint&>(conR[0]);
	map7[3] = reinterpret_cast<uint&>(conR[1]);
	LpmSetupOut(7, map7);

	uint4 map8;
	map8[0] = reinterpret_cast<uint&>(conR[2]);
	map8[1] = reinterpret_cast<uint&>(conG[0]);
	map8[2] = reinterpret_cast<uint&>(conG[1]);
	map8[3] = reinterpret_cast<uint&>(conG[2]);
	LpmSetupOut(8, map8);

	uint4 map9;
	map9[0] = reinterpret_cast<uint&>(conB[0]);
	map9[1] = reinterpret_cast<uint&>(conB[1]);
	map9[2] = reinterpret_cast<uint&>(conB[2]);
	map9[3] = 0;
	LpmSetupOut(9, map9);
}
static void PopulateLpmConsts(bool incon,
							  bool insoft,
							  bool incon2,
							  bool inclip,
							  bool inscaleOnly,
							  uint32_t& outcon,
							  uint32_t& outsoft,
							  uint32_t& outcon2,
							  uint32_t& outclip,
							  uint32_t& outscaleOnly) {
	outcon = incon;
	outsoft = insoft;
	outcon2 = incon2;
	outclip = inclip;
	outscaleOnly = inscaleOnly;
}

static LpmConstants prepare_args(LpmDispatchParameters const& params) {
	float2 fs2R;
	float2 fs2G;
	float2 fs2B;
	float2 fs2W;
	float2 displayMinMaxLuminance;
	if (params.displayMode != LpmDisplayMode::LDR) {
		// Only used in fs2 modes
		fs2R[0] = params.displayRedPrimary[0];
		fs2R[1] = params.displayRedPrimary[1];
		fs2G[0] = params.displayGreenPrimary[0];
		fs2G[1] = params.displayGreenPrimary[1];
		fs2B[0] = params.displayBluePrimary[0];
		fs2B[1] = params.displayBluePrimary[1];
		fs2W[0] = params.displayWhitePoint[0];
		fs2W[1] = params.displayWhitePoint[1];

		// Used in all HDR modes
		displayMinMaxLuminance[0] = params.displayMinLuminance;
		displayMinMaxLuminance[1] = params.displayMaxLuminance;
	}

	float3 saturation;
	saturation[0] = params.saturation[0];
	saturation[1] = params.saturation[1];
	saturation[2] = params.saturation[2];

	float3 crosstalk;
	crosstalk[0] = params.crosstalk[0];
	crosstalk[1] = params.crosstalk[1];
	crosstalk[2] = params.crosstalk[2];
	LpmConstants lpmConsts{};
	float hdr10S{};
	float fs2S{};
	switch (params.colorSpace) {
		case LpmColorSpace::REC709: {
			switch (params.displayMode) {
				case LpmDisplayMode::LDR: {
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_709_709,
									   LPM_COLORS_709_709,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_709_709, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
				case LpmDisplayMode::FSHDR_2084: {
					hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_FS2RAWPQ_709,
									   LPM_COLORS_FS2RAWPQ_709,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_FS2RAWPQ_709, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
				case LpmDisplayMode::FSHDR_SCRGB: {
					fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_FS2SCRGB_709,
									   LPM_COLORS_FS2SCRGB_709,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_FS2SCRGB_709, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
				case LpmDisplayMode::HDR10_2084: {
					hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_HDR10RAW_709,
									   LPM_COLORS_HDR10RAW_709,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_HDR10RAW_709, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
				case LpmDisplayMode::HDR10_SCRGB: {
					hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_HDR10SCRGB_709,
									   LPM_COLORS_HDR10SCRGB_709,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_HDR10SCRGB_709, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
			}
		} break;
		case LpmColorSpace::P3: {
			switch (params.displayMode) {
				case LpmDisplayMode::LDR: {
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_709_P3,
									   LPM_COLORS_709_P3,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_709_P3, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
				case LpmDisplayMode::FSHDR_2084: {
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_FS2RAWPQ_P3,
									   LPM_COLORS_FS2RAWPQ_P3,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_FS2RAWPQ_P3, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
				case LpmDisplayMode::FSHDR_SCRGB: {
					fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_FS2SCRGB_P3,
									   LPM_COLORS_FS2SCRGB_P3,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_FS2SCRGB_P3, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
				case LpmDisplayMode::HDR10_2084: {
					hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_HDR10RAW_P3,
									   LPM_COLORS_HDR10RAW_P3,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_HDR10RAW_P3, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
				case LpmDisplayMode::HDR10_SCRGB: {
					hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_HDR10SCRGB_P3,
									   LPM_COLORS_HDR10SCRGB_P3,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_HDR10SCRGB_P3, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
			}
		} break;
		case LpmColorSpace::REC2020: {
			switch (params.displayMode) {
				case LpmDisplayMode::LDR: {
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_709_2020,
									   LPM_COLORS_709_2020,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_709_2020, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
				case LpmDisplayMode::FSHDR_2084: {
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_FS2RAWPQ_2020,
									   LPM_COLORS_FS2RAWPQ_2020,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_FS2RAWPQ_2020, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
				case LpmDisplayMode::FSHDR_SCRGB: {
					fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_FS2SCRGB_2020,
									   LPM_COLORS_FS2SCRGB_2020,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_FS2SCRGB_2020, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
				case LpmDisplayMode::HDR10_2084: {
					hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_HDR10RAW_2020,
									   LPM_COLORS_HDR10RAW_2020,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_HDR10RAW_2020, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
				case LpmDisplayMode::HDR10_SCRGB: {
					hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
					CalculateLpmConsts(reinterpret_cast<uint*>(lpmConsts.ctl.data()), params.shoulder,
									   LPM_CONFIG_HDR10SCRGB_2020,
									   LPM_COLORS_HDR10SCRGB_2020,
									   params.softGap,
									   params.hdrMax,
									   params.lpmExposure,
									   params.contrast,
									   params.shoulderContrast,
									   saturation,
									   crosstalk);
					PopulateLpmConsts(LPM_CONFIG_HDR10SCRGB_2020, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
				} break;
			}
		} break;
		default:
			break;
	}
	return lpmConsts;
}
}// namespace ffx

post_uber_pass::LpmArgs LPM::compute(LpmDispatchParameters const& d) {
	auto consts = ffx::prepare_args(d);
	post_uber_pass::LpmArgs a;
	std::memcpy(a.ctl.data(), consts.ctl.data(), consts.ctl.size() * sizeof(uint4));
	a.flags = 0;
	a.flags |= (consts.scaleOnly != 0) ? 1 : 0;
	a.flags <<= 1;
	a.flags |= (consts.clip != 0) ? 1 : 0;
	a.flags <<= 1;
	a.flags |= (consts.con2 != 0) ? 1 : 0;
	a.flags <<= 1;
	a.flags |= (consts.soft != 0) ? 1 : 0;
	a.flags <<= 1;
	a.flags |= (consts.con != 0) ? 1 : 0;
	a.flags <<= 1;
	a.flags |= (consts.shoulder != 0) ? 1 : 0;
	return a;
}
}// namespace rbc

#undef LPM_CONFIG_FS2RAW_709
#undef LPM_COLORS_FS2RAW_709
#undef LPM_CONFIG_FS2RAWPQ_709
#undef LPM_COLORS_FS2RAWPQ_709
#undef LPM_CONFIG_FS2SCRGB_709
#undef LPM_COLORS_FS2SCRGB_709
#undef LPM_CONFIG_HDR10RAW_709
#undef LPM_COLORS_HDR10RAW_709
#undef LPM_CONFIG_HDR10SCRGB_709
#undef LPM_COLORS_HDR10SCRGB_709
#undef LPM_CONFIG_709_709
#undef LPM_COLORS_709_709
#undef LPM_CONFIG_FS2RAW_P3
#undef LPM_COLORS_FS2RAW_P3
#undef LPM_CONFIG_FS2RAWPQ_P3
#undef LPM_COLORS_FS2RAWPQ_P3
#undef LPM_CONFIG_FS2SCRGB_P3
#undef LPM_COLORS_FS2SCRGB_P3
#undef LPM_CONFIG_HDR10RAW_P3
#undef LPM_COLORS_HDR10RAW_P3
#undef LPM_CONFIG_HDR10SCRGB_P3
#undef LPM_COLORS_HDR10SCRGB_P3
#undef LPM_CONFIG_709_P3
#undef LPM_COLORS_709_P3
#undef LPM_CONFIG_FS2RAW_2020
#undef LPM_COLORS_FS2RAW_2020
#undef LPM_CONFIG_FS2RAWPQ_2020
#undef LPM_COLORS_FS2RAWPQ_2020
#undef LPM_CONFIG_FS2SCRGB_2020
#undef LPM_COLORS_FS2SCRGB_2020
#undef LPM_CONFIG_HDR10RAW_2020
#undef LPM_COLORS_HDR10RAW_2020
#undef LPM_CONFIG_HDR10SCRGB_2020
#undef LPM_COLORS_HDR10SCRGB_2020
#undef LPM_CONFIG_709_2020
#undef LPM_COLORS_709_2020