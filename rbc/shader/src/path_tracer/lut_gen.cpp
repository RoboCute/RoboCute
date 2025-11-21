// #include <material/openpbr_params.hpp>
// #include <sampling/pcg.hpp>
// #include <bsdfs/base/bsdf.hpp>
// #include <bsdfs/openpbr.hpp>
#include <luisa/std.hpp>
#include <luisa/types.hpp>

using namespace luisa::shader;

[[kernel_3d(4, 4, 4)]] int kernel(
	BindlessImage& image_heap,
	Buffer<float4>& result_buffer,
	uint illum_d65_idx,
	uint cie_xyz_cdfinv,
	uint spectrum_lut3d_idx,
	uint frame_index) {
	// auto id = dispatch_id();
	// auto size = dispatch_size();
	// sampling::PCGSampler sampler(uint3(id.xy, id.z + frame_index));
	// mtl::openpbr::Parameter mat_param;
	// float3 wi = 0.0f;
	// wi.z = max(2e-2f, float(id.x) / float(size.x - 1));
	// wi.y = sqrt(1.0f - wi.z * wi.z);
	// mat_param.specular.roughness = float(id.y) / float(size.y - 1);
	// mat_param.specular.ior = mtl::f0_to_ior(sqr(float(id.z) / float(size.z - 1)));
	// mat_param.transmission.weight = 1.0f;
	// mat_param.geometry.thin_walled = false;
	// mat_param.transmission.color = spectrum::SpectrumColor(1.0f, 1.0f);
	// mat_param.specular.color = spectrum::SpectrumColor(1.0f, 1.0f);

	// SpectrumArg spectrum_arg;
	// spectrum_arg.lambda = spectrum::sample_xyz(image_heap, cie_xyz_cdfinv, fract(sampler.next() + sampler.next() / 255.f));
	// spectrum_arg.hero_index = uint(sampler.next() * 3.0f) % 3;
	// spectrum_arg.illum_d65_idx = illum_d65_idx;
	// spectrum_arg.spectrum_lut3d_idx = spectrum_lut3d_idx;
	// mtl::ClosureData closure_data(mat_param, spectrum_arg.lambda);

	// closure_data.selected_wavelength = spectrum_arg.selected_wavelength;
	// closure_data.hero_wavelength_index = spectrum_arg.hero_index;

	// closure_data.detail = mtl::ShadingDetail::Default;
	// closure_data.rand = sampler.next3f();
	// using MatBSDF = mtl::TransmissionBSDF;
	// MatBSDF bsdf;
	// bsdf.init(wi, mat_param, closure_data);
	// float4 result = 0;
	// {//x
	// 	closure_data.rand = sampler.next3f();
	// 	closure_data.sampling_flags = mtl::BSDFFlags::Reflection;
	// 	auto s = bsdf.sample(wi, closure_data);
	// 	if (s && s.wo.z >= 0) result.x = reduce_sum(s.throughput.val) / (s.pdf * 3.0f);
	// }
	// {//y
	// 	closure_data.rand = sampler.next3f();
	// 	closure_data.sampling_flags = mtl::BSDFFlags::Transmission;
	// 	auto s = bsdf.sample(wi, closure_data);
	// 	if (s && s.wo.z <= 0) result.y = reduce_sum(s.throughput.val) / (s.pdf * 3.0f);
	// }
	// wi.z = -wi.z;
	// {//z
	// 	closure_data.rand = sampler.next3f();
	// 	closure_data.sampling_flags = mtl::BSDFFlags::Reflection;
	// 	auto s = bsdf.sample(wi, closure_data);
	// 	if (s && s.wo.z <= 0) result.z = reduce_sum(s.throughput.val) / (s.pdf * 3.0f);
	// }
	// {//w
	// 	closure_data.rand = sampler.next3f();
	// 	closure_data.sampling_flags = mtl::BSDFFlags::Transmission;
	// 	auto s = bsdf.sample(wi, closure_data);
	// 	if (s && s.wo.z >= 0) result.w = reduce_sum(s.throughput.val) / (s.pdf * 3.0f);
	// }

	// // result.xy = result.zw;
	// // result.zw = float2(0.0f, 1.0f);

	// result = ite(is_finite(result), result, float4(0.0f));
	// auto buffer_id = id.x + id.y * size.x + id.z * size.x * size.y;
	// if (frame_index != 0) {
	// 	auto old_result = result_buffer.read(buffer_id);
	// 	result = lerp(old_result, result, 1.f / (frame_index + 1.f));
	// 	if (frame_index == 16383) {
	// 		if (all(result.xy == 0.0f)) {
	// 			result.xy = float2(1.0f, 0.0f);
	// 		}
	// 		if (all(result.zw == 0.0f)) {
	// 			result.zw = float2(1.0f, 0.0f);
	// 		}
	// 	}
	// }
	// result_buffer.write(
	// 	buffer_id,
	// 	result);
	return 0;
}