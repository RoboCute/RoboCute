::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char g_vt_level_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char image_heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char mesh_meta_desc[35] = {115,116,114,117,99,116,60,52,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,62,0};
char mat_index_desc[5] = {117,105,110,116,0};
char mat_idx_buffer_heap_idx_desc[5] = {117,105,110,116,0};
char result_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(g_vt_level_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(image_heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(mesh_meta_desc, 34)),::luisa::compute::Type::from(luisa::string_view(mat_index_desc, 4)),::luisa::compute::Type::from(luisa::string_view(mat_idx_buffer_heap_idx_desc, 4)),::luisa::compute::Type::from(luisa::string_view(result_desc, 13))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}

template <typename A0>
requires((sizeof(A0) == 20 && alignof(A0) == 4 && std::is_trivially_copyable_v<A0>))
::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint _dispatch_size,
::luisa::compute::BufferView<uint32_t> g_vt_level_buffer,
::luisa::compute::BindlessArray const & heap,
::luisa::compute::BindlessArray const & image_heap,
A0 const & mesh_meta,
uint32_t mat_index,
uint32_t mat_idx_buffer_heap_idx,
::luisa::compute::BufferView<float> result) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 7, _shader->uniform_size());
_cmd_encoder.encode_buffer(g_vt_level_buffer.handle(),g_vt_level_buffer.offset_bytes(),g_vt_level_buffer.size_bytes());
_cmd_encoder.encode_bindless_array(heap.handle());
_cmd_encoder.encode_bindless_array(image_heap.handle());
_cmd_encoder.encode_uniform(std::addressof(mesh_meta), 20, 4);
_cmd_encoder.encode_uniform(std::addressof(mat_index), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(mat_idx_buffer_heap_idx), 4, 4);
_cmd_encoder.encode_buffer(result.handle(),result.offset_bytes(),result.size_bytes());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u,1u));

return std::move(_cmd_encoder).build();
}