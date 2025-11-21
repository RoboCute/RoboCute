::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char buffer_desc[13] = {98,117,102,102,101,114,60,104,97,108,102,62,0};
char img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(img_desc, 16))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::BufferView<::luisa::half> buffer,
::luisa::compute::ImageView<float> img) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 2, _shader->uniform_size());
_cmd_encoder.encode_buffer(buffer.handle(),buffer.offset_bytes(),buffer.size_bytes());
_cmd_encoder.encode_texture(img.handle(), img.level());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}