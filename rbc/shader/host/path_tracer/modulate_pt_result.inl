::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char radiance_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char beta_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char spec_albedo_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char albedo_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char di_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char sample_flags_img_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};
char normal_rough_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char diffuse_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char specular_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char exposure_buffer_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};
char sample_desc[5] = {117,105,110,116,0};
char spp_desc[5] = {117,105,110,116,0};
char denoise_desc[5] = {98,111,111,108,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(radiance_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(beta_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(spec_albedo_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(albedo_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(di_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(sample_flags_img_desc, 15)),::luisa::compute::Type::from(luisa::string_view(normal_rough_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(diffuse_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(specular_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(exposure_buffer_desc, 13)),::luisa::compute::Type::from(luisa::string_view(sample_desc, 4)),::luisa::compute::Type::from(luisa::string_view(spp_desc, 4)),::luisa::compute::Type::from(luisa::string_view(denoise_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> radiance_img,
::luisa::compute::ImageView<float> beta_img,
::luisa::compute::ImageView<float> spec_albedo_img,
::luisa::compute::ImageView<float> albedo_img,
::luisa::compute::ImageView<float> di_img,
::luisa::compute::ImageView<uint32_t> sample_flags_img,
::luisa::compute::ImageView<float> normal_rough_img,
::luisa::compute::ImageView<float> diffuse_img,
::luisa::compute::ImageView<float> specular_img,
::luisa::compute::BufferView<float> exposure_buffer,
uint32_t sample,
uint32_t spp,
bool denoise) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 13, _shader->uniform_size());
_cmd_encoder.encode_texture(radiance_img.handle(), radiance_img.level());
_cmd_encoder.encode_texture(beta_img.handle(), beta_img.level());
_cmd_encoder.encode_texture(spec_albedo_img.handle(), spec_albedo_img.level());
_cmd_encoder.encode_texture(albedo_img.handle(), albedo_img.level());
_cmd_encoder.encode_texture(di_img.handle(), di_img.level());
_cmd_encoder.encode_texture(sample_flags_img.handle(), sample_flags_img.level());
_cmd_encoder.encode_texture(normal_rough_img.handle(), normal_rough_img.level());
_cmd_encoder.encode_texture(diffuse_img.handle(), diffuse_img.level());
_cmd_encoder.encode_texture(specular_img.handle(), specular_img.level());
_cmd_encoder.encode_buffer(exposure_buffer.handle(),exposure_buffer.offset_bytes(),exposure_buffer.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(sample), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(spp), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(denoise), 1, 1);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}