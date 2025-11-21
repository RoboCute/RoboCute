::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char out_tex_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char main_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char params_desc[6] = {102,108,111,97,116,0};
char threshold_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,0};
char exposure_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(out_tex_desc, 16)),::luisa::compute::Type::from(luisa::string_view(main_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(params_desc, 5)),::luisa::compute::Type::from(luisa::string_view(threshold_desc, 15)),::luisa::compute::Type::from(luisa::string_view(exposure_desc, 13))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> out_tex,
::luisa::compute::ImageView<float> main_img,
float params,
::luisa::Vector<float,4> threshold,
::luisa::compute::BufferView<float> exposure) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 5, _shader->uniform_size());
_cmd_encoder.encode_texture(out_tex.handle(), out_tex.level());
_cmd_encoder.encode_texture(main_img.handle(), main_img.level());
_cmd_encoder.encode_uniform(std::addressof(params), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(threshold), 16, 16);
_cmd_encoder.encode_buffer(exposure.handle(),exposure.offset_bytes(),exposure.size_bytes());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}