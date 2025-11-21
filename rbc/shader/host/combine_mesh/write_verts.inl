::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char data_buffer_desc[69] = {98,117,102,102,101,114,60,115,116,114,117,99,116,60,49,54,44,109,97,116,114,105,120,60,52,62,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,98,111,111,108,44,98,111,111,108,44,117,105,110,116,44,117,105,110,116,62,62,0};
char sum_vert_count_desc[5] = {117,105,110,116,0};
char contained_normal_desc[5] = {98,111,111,108,0};
char contained_tangent_desc[5] = {98,111,111,108,0};
char contained_uv_desc[5] = {117,105,110,116,0};
char dst_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(data_buffer_desc, 68)),::luisa::compute::Type::from(luisa::string_view(sum_vert_count_desc, 4)),::luisa::compute::Type::from(luisa::string_view(contained_normal_desc, 4)),::luisa::compute::Type::from(luisa::string_view(contained_tangent_desc, 4)),::luisa::compute::Type::from(luisa::string_view(contained_uv_desc, 4)),::luisa::compute::Type::from(luisa::string_view(dst_desc, 13))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}

template <typename A0>
requires((sizeof(A0) == 96 && alignof(A0) == 16 && std::is_trivially_copyable_v<A0>))
::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint _dispatch_size,
::luisa::compute::BindlessArray const & heap,
::luisa::compute::BufferView<A0> data_buffer,
uint32_t sum_vert_count,
bool contained_normal,
bool contained_tangent,
uint32_t contained_uv,
::luisa::compute::BufferView<float> dst) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 7, _shader->uniform_size());
_cmd_encoder.encode_bindless_array(heap.handle());
_cmd_encoder.encode_buffer(data_buffer.handle(),data_buffer.offset_bytes(),data_buffer.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(sum_vert_count), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(contained_normal), 1, 1);
_cmd_encoder.encode_uniform(std::addressof(contained_tangent), 1, 1);
_cmd_encoder.encode_uniform(std::addressof(contained_uv), 4, 4);
_cmd_encoder.encode_buffer(dst.handle(),dst.offset_bytes(),dst.size_bytes());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u,1u));

return std::move(_cmd_encoder).build();
}