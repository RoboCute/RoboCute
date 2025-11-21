::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char out_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char offset_texture_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};
char level_buffer_desc[23] = {98,117,102,102,101,114,60,118,101,99,116,111,114,60,117,105,110,116,44,50,62,62,0};
char max_level_desc[5] = {117,105,110,116,0};
char sample_index_desc[15] = {118,101,99,116,111,114,60,117,105,110,116,44,50,62,0};
char level_size_desc[5] = {117,105,110,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(out_desc, 16)),::luisa::compute::Type::from(luisa::string_view(offset_texture_desc, 15)),::luisa::compute::Type::from(luisa::string_view(level_buffer_desc, 22)),::luisa::compute::Type::from(luisa::string_view(max_level_desc, 4)),::luisa::compute::Type::from(luisa::string_view(sample_index_desc, 14)),::luisa::compute::Type::from(luisa::string_view(level_size_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::BindlessArray const & heap,
::luisa::compute::ImageView<float> out,
::luisa::compute::ImageView<uint32_t> offset_texture,
::luisa::compute::BufferView<::luisa::Vector<uint32_t,2>> level_buffer,
uint32_t max_level,
::luisa::Vector<uint32_t,2> sample_index,
uint32_t level_size) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 7, _shader->uniform_size());
_cmd_encoder.encode_bindless_array(heap.handle());
_cmd_encoder.encode_texture(out.handle(), out.level());
_cmd_encoder.encode_texture(offset_texture.handle(), offset_texture.level());
_cmd_encoder.encode_buffer(level_buffer.handle(),level_buffer.offset_bytes(),level_buffer.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(max_level), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(sample_index), 8, 8);
_cmd_encoder.encode_uniform(std::addressof(level_size), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}