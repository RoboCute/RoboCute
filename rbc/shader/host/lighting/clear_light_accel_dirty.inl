::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char img_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};
char arr_desc[14] = {97,114,114,97,121,60,117,105,110,116,44,53,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(img_desc, 15)),::luisa::compute::Type::from(luisa::string_view(arr_desc, 13))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<uint32_t> img,
std::array<uint32_t,5> const & arr) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 2, _shader->uniform_size());
_cmd_encoder.encode_texture(img.handle(), img.level());
_cmd_encoder.encode_uniform(arr.data(), 20, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}