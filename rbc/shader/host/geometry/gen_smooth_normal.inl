::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char vert_buffer_desc[13] = {98,117,102,102,101,114,60,118,111,105,100,62,0};
char vertex_count_desc[5] = {117,105,110,116,0};
char write_uv_desc[5] = {98,111,111,108,0};
char uv_scale_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,50,62,0};
char uv_offset_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,50,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(vert_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(vertex_count_desc, 4)),::luisa::compute::Type::from(luisa::string_view(write_uv_desc, 4)),::luisa::compute::Type::from(luisa::string_view(uv_scale_desc, 15)),::luisa::compute::Type::from(luisa::string_view(uv_offset_desc, 15))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
ByteBufferView vert_buffer,
uint32_t vertex_count,
bool write_uv,
::luisa::Vector<float,2> uv_scale,
::luisa::Vector<float,2> uv_offset) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 5, _shader->uniform_size());
_cmd_encoder.encode_buffer(vert_buffer.handle(),vert_buffer.offset_bytes(),vert_buffer.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(vertex_count), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(write_uv), 1, 1);
_cmd_encoder.encode_uniform(std::addressof(uv_scale), 8, 8);
_cmd_encoder.encode_uniform(std::addressof(uv_offset), 8, 8);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}