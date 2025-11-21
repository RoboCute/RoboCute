::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char _emission_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char _depth_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char _normal_roughness_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char _mv_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char _spec_albedo_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char _albedo_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char _confidence_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char _out_radiance_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char _specular_dist_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char _specular_albedo_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char _reflection_mv_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char sky_idx_desc[5] = {117,105,110,116,0};
char default_depth_desc[6] = {102,108,111,97,116,0};
char resource_to_rec2020_mat_desc[10] = {109,97,116,114,105,120,60,51,62,0};
char world_2_sky_mat_desc[10] = {109,97,116,114,105,120,60,51,62,0};
char inv_vp_desc[10] = {109,97,116,114,105,120,60,52,62,0};
char cam_pos_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,51,62,0};
char jitter_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,50,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(_emission_desc, 16)),::luisa::compute::Type::from(luisa::string_view(_depth_desc, 16)),::luisa::compute::Type::from(luisa::string_view(_normal_roughness_desc, 16)),::luisa::compute::Type::from(luisa::string_view(_mv_desc, 16)),::luisa::compute::Type::from(luisa::string_view(_spec_albedo_desc, 16)),::luisa::compute::Type::from(luisa::string_view(_albedo_desc, 16)),::luisa::compute::Type::from(luisa::string_view(_confidence_desc, 16)),::luisa::compute::Type::from(luisa::string_view(_out_radiance_desc, 16)),::luisa::compute::Type::from(luisa::string_view(_specular_dist_desc, 16)),::luisa::compute::Type::from(luisa::string_view(_specular_albedo_desc, 16)),::luisa::compute::Type::from(luisa::string_view(_reflection_mv_desc, 16)),::luisa::compute::Type::from(luisa::string_view(heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(sky_idx_desc, 4)),::luisa::compute::Type::from(luisa::string_view(default_depth_desc, 5)),::luisa::compute::Type::from(luisa::string_view(resource_to_rec2020_mat_desc, 9)),::luisa::compute::Type::from(luisa::string_view(world_2_sky_mat_desc, 9)),::luisa::compute::Type::from(luisa::string_view(inv_vp_desc, 9)),::luisa::compute::Type::from(luisa::string_view(cam_pos_desc, 15)),::luisa::compute::Type::from(luisa::string_view(jitter_desc, 15))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> _emission,
::luisa::compute::ImageView<float> _depth,
::luisa::compute::ImageView<float> _normal_roughness,
::luisa::compute::ImageView<float> _mv,
::luisa::compute::ImageView<float> _spec_albedo,
::luisa::compute::ImageView<float> _albedo,
::luisa::compute::ImageView<float> _confidence,
::luisa::compute::ImageView<float> _out_radiance,
::luisa::compute::ImageView<float> _specular_dist,
::luisa::compute::ImageView<float> _specular_albedo,
::luisa::compute::ImageView<float> _reflection_mv,
::luisa::compute::BindlessArray const & heap,
uint32_t sky_idx,
float default_depth,
::luisa::float3x3 resource_to_rec2020_mat,
::luisa::float3x3 world_2_sky_mat,
::luisa::float4x4 inv_vp,
::luisa::Vector<float,3> cam_pos,
::luisa::Vector<float,2> jitter) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 19, _shader->uniform_size());
_cmd_encoder.encode_texture(_emission.handle(), _emission.level());
_cmd_encoder.encode_texture(_depth.handle(), _depth.level());
_cmd_encoder.encode_texture(_normal_roughness.handle(), _normal_roughness.level());
_cmd_encoder.encode_texture(_mv.handle(), _mv.level());
_cmd_encoder.encode_texture(_spec_albedo.handle(), _spec_albedo.level());
_cmd_encoder.encode_texture(_albedo.handle(), _albedo.level());
_cmd_encoder.encode_texture(_confidence.handle(), _confidence.level());
_cmd_encoder.encode_texture(_out_radiance.handle(), _out_radiance.level());
_cmd_encoder.encode_texture(_specular_dist.handle(), _specular_dist.level());
_cmd_encoder.encode_texture(_specular_albedo.handle(), _specular_albedo.level());
_cmd_encoder.encode_texture(_reflection_mv.handle(), _reflection_mv.level());
_cmd_encoder.encode_bindless_array(heap.handle());
_cmd_encoder.encode_uniform(std::addressof(sky_idx), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(default_depth), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(resource_to_rec2020_mat), 48, 16);
_cmd_encoder.encode_uniform(std::addressof(world_2_sky_mat), 48, 16);
_cmd_encoder.encode_uniform(std::addressof(inv_vp), 64, 16);
_cmd_encoder.encode_uniform(std::addressof(cam_pos), 16, 16);
_cmd_encoder.encode_uniform(std::addressof(jitter), 8, 8);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}