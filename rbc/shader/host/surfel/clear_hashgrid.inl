::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char key_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char value_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char reset_desc[5] = {98,111,111,108,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(key_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(value_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(reset_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint _dispatch_size,
::luisa::compute::BufferView<uint32_t> key_buffer,
::luisa::compute::BufferView<uint32_t> value_buffer,
bool reset) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 3, _shader->uniform_size());
_cmd_encoder.encode_buffer(key_buffer.handle(),key_buffer.offset_bytes(),key_buffer.size_bytes());
_cmd_encoder.encode_buffer(value_buffer.handle(),value_buffer.offset_bytes(),value_buffer.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(reset), 1, 1);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u,1u));

return std::move(_cmd_encoder).build();
}