::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char src_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char sun_color_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,51,62,0};
char sun_dir_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,51,62,0};
char sun_angle_radians_desc[6] = {102,108,111,97,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(src_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(sun_color_desc, 15)),::luisa::compute::Type::from(luisa::string_view(sun_dir_desc, 15)),::luisa::compute::Type::from(luisa::string_view(sun_angle_radians_desc, 5))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> src_img,
::luisa::Vector<float,3> sun_color,
::luisa::Vector<float,3> sun_dir,
float sun_angle_radians) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 4, _shader->uniform_size());
_cmd_encoder.encode_texture(src_img.handle(), src_img.level());
_cmd_encoder.encode_uniform(std::addressof(sun_color), 16, 16);
_cmd_encoder.encode_uniform(std::addressof(sun_dir), 16, 16);
_cmd_encoder.encode_uniform(std::addressof(sun_angle_radians), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}