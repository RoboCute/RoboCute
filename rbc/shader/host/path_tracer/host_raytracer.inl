::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char accel_desc[6] = {97,99,99,101,108,0};
char heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char ray_input_buffer_desc[65] = {98,117,102,102,101,114,60,115,116,114,117,99,116,60,52,44,97,114,114,97,121,60,102,108,111,97,116,44,51,62,44,97,114,114,97,121,60,102,108,111,97,116,44,51,62,44,102,108,111,97,116,44,102,108,111,97,116,44,117,105,110,116,62,62,0};
char ray_output_buffer_desc[59] = {98,117,102,102,101,114,60,115,116,114,117,99,116,60,52,44,117,105,110,116,44,102,108,111,97,116,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,97,114,114,97,121,60,102,108,111,97,116,44,50,62,62,62,0};
char inst_buffer_heap_idx_desc[5] = {117,105,110,116,0};
char mat_idx_buffer_heap_idx_desc[5] = {117,105,110,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(accel_desc, 5)),::luisa::compute::Type::from(luisa::string_view(heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(ray_input_buffer_desc, 64)),::luisa::compute::Type::from(luisa::string_view(ray_output_buffer_desc, 58)),::luisa::compute::Type::from(luisa::string_view(inst_buffer_heap_idx_desc, 4)),::luisa::compute::Type::from(luisa::string_view(mat_idx_buffer_heap_idx_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}

template <typename A0,typename A1>
requires((sizeof(A0) == 36 && alignof(A0) == 4 && std::is_trivially_copyable_v<A0>)&&
(sizeof(A1) == 28 && alignof(A1) == 4 && std::is_trivially_copyable_v<A1>))
::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint _dispatch_size,
::luisa::compute::Accel const & accel,
::luisa::compute::BindlessArray const & heap,
::luisa::compute::BufferView<A0> ray_input_buffer,
::luisa::compute::BufferView<A1> ray_output_buffer,
uint32_t inst_buffer_heap_idx,
uint32_t mat_idx_buffer_heap_idx) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 6, _shader->uniform_size());
_cmd_encoder.encode_accel(accel.handle());
_cmd_encoder.encode_bindless_array(heap.handle());
_cmd_encoder.encode_buffer(ray_input_buffer.handle(),ray_input_buffer.offset_bytes(),ray_input_buffer.size_bytes());
_cmd_encoder.encode_buffer(ray_output_buffer.handle(),ray_output_buffer.offset_bytes(),ray_output_buffer.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(inst_buffer_heap_idx), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(mat_idx_buffer_heap_idx), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u,1u));

return std::move(_cmd_encoder).build();
}