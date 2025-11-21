::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char out_tex_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char main_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char bloom_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char sample_scale_desc[6] = {102,108,111,97,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(out_tex_desc, 16)),::luisa::compute::Type::from(luisa::string_view(main_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(bloom_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(sample_scale_desc, 5))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> out_tex,
::luisa::compute::ImageView<float> main_img,
::luisa::compute::ImageView<float> bloom_img,
float sample_scale) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 4, _shader->uniform_size());
_cmd_encoder.encode_texture(out_tex.handle(), out_tex.level());
_cmd_encoder.encode_texture(main_img.handle(), main_img.level());
_cmd_encoder.encode_texture(bloom_img.handle(), bloom_img.level());
_cmd_encoder.encode_uniform(std::addressof(sample_scale), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}