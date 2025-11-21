::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char scale_map_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(scale_map_desc, 13))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> img,
::luisa::compute::BufferView<float> scale_map) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 2, _shader->uniform_size());
_cmd_encoder.encode_texture(img.handle(), img.level());
_cmd_encoder.encode_buffer(scale_map.handle(),scale_map.offset_bytes(),scale_map.size_bytes());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}