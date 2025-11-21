::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char dst_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char src_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char reverse_rgb_desc[5] = {98,111,111,108,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(dst_desc, 16)),::luisa::compute::Type::from(luisa::string_view(src_desc, 16)),::luisa::compute::Type::from(luisa::string_view(reverse_rgb_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> dst,
::luisa::compute::ImageView<float> src,
bool reverse_rgb) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 3, _shader->uniform_size());
_cmd_encoder.encode_texture(dst.handle(), dst.level());
_cmd_encoder.encode_texture(src.handle(), src.level());
_cmd_encoder.encode_uniform(std::addressof(reverse_rgb), 1, 1);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}