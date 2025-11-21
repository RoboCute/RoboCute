::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char world_mv_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char viewz_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char screen_mv_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char normal_roughness_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char out_normal_roughness_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char inv_p_desc[10] = {109,97,116,114,105,120,60,52,62,0};
char inv_v_desc[10] = {109,97,116,114,105,120,60,52,62,0};
char last_vp_desc[10] = {109,97,116,114,105,120,60,52,62,0};
char inv_sky_view_desc[10] = {109,97,116,114,105,120,60,52,62,0};
char last_sky_vp_desc[10] = {109,97,116,114,105,120,60,52,62,0};
char near_z_desc[6] = {102,108,111,97,116,0};
char far_z_desc[6] = {102,108,111,97,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(world_mv_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(viewz_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(screen_mv_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(normal_roughness_desc, 16)),::luisa::compute::Type::from(luisa::string_view(out_normal_roughness_desc, 16)),::luisa::compute::Type::from(luisa::string_view(inv_p_desc, 9)),::luisa::compute::Type::from(luisa::string_view(inv_v_desc, 9)),::luisa::compute::Type::from(luisa::string_view(last_vp_desc, 9)),::luisa::compute::Type::from(luisa::string_view(inv_sky_view_desc, 9)),::luisa::compute::Type::from(luisa::string_view(last_sky_vp_desc, 9)),::luisa::compute::Type::from(luisa::string_view(near_z_desc, 5)),::luisa::compute::Type::from(luisa::string_view(far_z_desc, 5))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> world_mv_img,
::luisa::compute::ImageView<float> viewz_img,
::luisa::compute::ImageView<float> screen_mv_img,
::luisa::compute::ImageView<float> normal_roughness,
::luisa::compute::ImageView<float> out_normal_roughness,
::luisa::float4x4 inv_p,
::luisa::float4x4 inv_v,
::luisa::float4x4 last_vp,
::luisa::float4x4 inv_sky_view,
::luisa::float4x4 last_sky_vp,
float near_z,
float far_z) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 12, _shader->uniform_size());
_cmd_encoder.encode_texture(world_mv_img.handle(), world_mv_img.level());
_cmd_encoder.encode_texture(viewz_img.handle(), viewz_img.level());
_cmd_encoder.encode_texture(screen_mv_img.handle(), screen_mv_img.level());
_cmd_encoder.encode_texture(normal_roughness.handle(), normal_roughness.level());
_cmd_encoder.encode_texture(out_normal_roughness.handle(), out_normal_roughness.level());
_cmd_encoder.encode_uniform(std::addressof(inv_p), 64, 16);
_cmd_encoder.encode_uniform(std::addressof(inv_v), 64, 16);
_cmd_encoder.encode_uniform(std::addressof(last_vp), 64, 16);
_cmd_encoder.encode_uniform(std::addressof(inv_sky_view), 64, 16);
_cmd_encoder.encode_uniform(std::addressof(last_sky_vp), 64, 16);
_cmd_encoder.encode_uniform(std::addressof(near_z), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(far_z), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}