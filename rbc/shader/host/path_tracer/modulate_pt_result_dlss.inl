::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char beta_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char emission_tex_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char radiance_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char di_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char out_radiance_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char exposure_buffer_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};
char sample_desc[5] = {117,105,110,116,0};
char spp_desc[5] = {117,105,110,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(beta_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(emission_tex_desc, 16)),::luisa::compute::Type::from(luisa::string_view(radiance_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(di_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(out_radiance_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(exposure_buffer_desc, 13)),::luisa::compute::Type::from(luisa::string_view(sample_desc, 4)),::luisa::compute::Type::from(luisa::string_view(spp_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> beta_img,
::luisa::compute::ImageView<float> emission_tex,
::luisa::compute::ImageView<float> radiance_img,
::luisa::compute::ImageView<float> di_img,
::luisa::compute::ImageView<float> out_radiance_img,
::luisa::compute::BufferView<float> exposure_buffer,
uint32_t sample,
uint32_t spp) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 8, _shader->uniform_size());
_cmd_encoder.encode_texture(beta_img.handle(), beta_img.level());
_cmd_encoder.encode_texture(emission_tex.handle(), emission_tex.level());
_cmd_encoder.encode_texture(radiance_img.handle(), radiance_img.level());
_cmd_encoder.encode_texture(di_img.handle(), di_img.level());
_cmd_encoder.encode_texture(out_radiance_img.handle(), out_radiance_img.level());
_cmd_encoder.encode_buffer(exposure_buffer.handle(),exposure_buffer.offset_bytes(),exposure_buffer.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(sample), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(spp), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}