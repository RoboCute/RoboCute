::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char albedo_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char emission_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char radiance_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(albedo_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(emission_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(radiance_img_desc, 16))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> albedo_img,
::luisa::compute::ImageView<float> emission_img,
::luisa::compute::ImageView<float> radiance_img) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 3, _shader->uniform_size());
_cmd_encoder.encode_texture(albedo_img.handle(), albedo_img.level());
_cmd_encoder.encode_texture(emission_img.handle(), emission_img.level());
_cmd_encoder.encode_texture(radiance_img.handle(), radiance_img.level());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}