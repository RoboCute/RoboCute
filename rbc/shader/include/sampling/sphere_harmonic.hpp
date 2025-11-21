#pragma once
#include <luisa/std.hpp>
namespace sampling {
using namespace luisa::shader;
using SH1Table = std::array<float, 4>;
using SH1ColorTable = std::array<float3, 4>;
using SH2Table = std::array<float, 9>;
using SH2ColorTable = std::array<float3, 9>;
using SH2TexValue = std::array<float4, 7>;
inline SH1Table get_sh1_table(float3 dir) {
	SH1Table t;
	t[0] = 0.2820947917f;
	t[1] = 0.4886025119f * dir.y;
	t[2] = 0.4886025119f * dir.z;
	t[3] = 0.4886025119f * dir.x;
	return t;
}
inline SH2Table get_sh2_table(float3 dir) {
	SH2Table t;
	t[0] = 0.2820947917f;
	t[1] = 0.4886025119f * dir.y;
	t[2] = 0.4886025119f * dir.z;
	t[3] = 0.4886025119f * dir.x;
	t[4] = 1.0925484306f * dir.x * dir.y;
	t[5] = 1.0925484306f * dir.y * dir.z;
	t[6] = 0.3153915652f * (3 * dir.z * dir.z - 1);
	t[7] = 1.0925484306f * dir.x * dir.z;
	t[8] = 0.5462742153f * (dir.x * dir.x - dir.y * dir.y);
	return t;
}
inline SH2TexValue write_sh_to_tex(SH2ColorTable sh_color) {
	SH2TexValue tex_value;
	tex_value[0] = float4(sh_color[0], sh_color[1].x);
	tex_value[1] = float4(sh_color[1].yz, sh_color[2].xy);
	tex_value[2] = float4(sh_color[2].z, sh_color[3]);
	tex_value[3] = float4(sh_color[4], sh_color[5].x);
	tex_value[4] = float4(sh_color[5].yz, sh_color[6].xy);
	tex_value[5] = float4(sh_color[6].z, sh_color[7]);
	tex_value[6] = float4(sh_color[8], 0.f);
	return tex_value;
}
inline SH2ColorTable read_sh_from_tx(SH2TexValue values) {
	SH2ColorTable sh_color;
	sh_color[0] = values[0].xyz;
	sh_color[1] = float3(values[0].w, values[1].xy);
	sh_color[2] = float3(values[1].zw, values[2].x);
	sh_color[3] = float3(values[2].yzw);
	sh_color[4] = values[3].xyz;
	sh_color[5] = float3(values[3].w, values[4].xy);
	sh_color[6] = float3(values[4].zw, values[5].x);
	sh_color[7] = float3(values[5].yzw);
	sh_color[8] = float3(values[6].xyz);
	return sh_color;
}
}// namespace sampling