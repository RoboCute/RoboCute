::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char src_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char dst_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char max_lum_desc[6] = {102,108,111,97,116,0};
char blur_offset_desc[15] = {118,101,99,116,111,114,60,117,105,110,116,44,50,62,0};
char blur_radius_desc[5] = {117,105,110,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(src_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(dst_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(max_lum_desc, 5)),::luisa::compute::Type::from(luisa::string_view(blur_offset_desc, 14)),::luisa::compute::Type::from(luisa::string_view(blur_radius_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> src_img,
::luisa::compute::ImageView<float> dst_img,
float max_lum,
::luisa::Vector<uint32_t,2> blur_offset,
uint32_t blur_radius) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 5, _shader->uniform_size());
_cmd_encoder.encode_texture(src_img.handle(), src_img.level());
_cmd_encoder.encode_texture(dst_img.handle(), dst_img.level());
_cmd_encoder.encode_uniform(std::addressof(max_lum), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(blur_offset), 8, 8);
_cmd_encoder.encode_uniform(std::addressof(blur_radius), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}