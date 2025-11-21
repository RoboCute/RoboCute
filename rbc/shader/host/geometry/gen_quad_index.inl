::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(buffer_desc, 12))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::BufferView<uint32_t> buffer) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 1, _shader->uniform_size());
_cmd_encoder.encode_buffer(buffer.handle(),buffer.offset_bytes(),buffer.size_bytes());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}