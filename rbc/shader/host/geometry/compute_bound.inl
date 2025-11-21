::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char buffer_heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char offset_buffer_desc[23] = {98,117,102,102,101,114,60,118,101,99,116,111,114,60,117,105,110,116,44,50,62,62,0};
char result_aabb_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char clear_desc[5] = {98,111,111,108,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(buffer_heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(offset_buffer_desc, 22)),::luisa::compute::Type::from(luisa::string_view(result_aabb_desc, 12)),::luisa::compute::Type::from(luisa::string_view(clear_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint _dispatch_size,
::luisa::compute::BindlessArray const & buffer_heap,
::luisa::compute::BufferView<::luisa::Vector<uint32_t,2>> offset_buffer,
::luisa::compute::BufferView<uint32_t> result_aabb,
bool clear) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 4, _shader->uniform_size());
_cmd_encoder.encode_bindless_array(buffer_heap.handle());
_cmd_encoder.encode_buffer(offset_buffer.handle(),offset_buffer.offset_bytes(),offset_buffer.size_bytes());
_cmd_encoder.encode_buffer(result_aabb.handle(),result_aabb.offset_bytes(),result_aabb.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(clear), 1, 1);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u,1u));

return std::move(_cmd_encoder).build();
}