::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char dst_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char data_buffer_desc[69] = {98,117,102,102,101,114,60,115,116,114,117,99,116,60,49,54,44,109,97,116,114,105,120,60,52,62,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,98,111,111,108,44,98,111,111,108,44,117,105,110,116,44,117,105,110,116,62,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(dst_desc, 12)),::luisa::compute::Type::from(luisa::string_view(heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(data_buffer_desc, 68))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}

template <typename A0>
requires((sizeof(A0) == 96 && alignof(A0) == 16 && std::is_trivially_copyable_v<A0>))
::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint _dispatch_size,
::luisa::compute::BufferView<uint32_t> dst,
::luisa::compute::BindlessArray const & heap,
::luisa::compute::BufferView<A0> data_buffer) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 3, _shader->uniform_size());
_cmd_encoder.encode_buffer(dst.handle(),dst.offset_bytes(),dst.size_bytes());
_cmd_encoder.encode_bindless_array(heap.handle());
_cmd_encoder.encode_buffer(data_buffer.handle(),data_buffer.offset_bytes(),data_buffer.size_bytes());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u,1u));

return std::move(_cmd_encoder).build();
}