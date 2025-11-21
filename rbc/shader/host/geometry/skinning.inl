::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char src_buffer_desc[13] = {98,117,102,102,101,114,60,118,111,105,100,62,0};
char dst_buffer_desc[13] = {98,117,102,102,101,114,60,118,111,105,100,62,0};
char last_pos_buffer_desc[24] = {98,117,102,102,101,114,60,118,101,99,116,111,114,60,102,108,111,97,116,44,51,62,62,0};
char dq_bone_buffer_desc[51] = {98,117,102,102,101,114,60,115,116,114,117,99,116,60,49,54,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,62,62,0};
char bone_indices_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char bone_weights_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};
char bones_count_per_vert_desc[5] = {117,105,110,116,0};
char vertex_count_desc[5] = {117,105,110,116,0};
char contained_normal_desc[5] = {98,111,111,108,0};
char contained_tangent_desc[5] = {98,111,111,108,0};
char reset_frame_desc[5] = {98,111,111,108,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(src_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(dst_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(last_pos_buffer_desc, 23)),::luisa::compute::Type::from(luisa::string_view(dq_bone_buffer_desc, 50)),::luisa::compute::Type::from(luisa::string_view(bone_indices_desc, 12)),::luisa::compute::Type::from(luisa::string_view(bone_weights_desc, 13)),::luisa::compute::Type::from(luisa::string_view(bones_count_per_vert_desc, 4)),::luisa::compute::Type::from(luisa::string_view(vertex_count_desc, 4)),::luisa::compute::Type::from(luisa::string_view(contained_normal_desc, 4)),::luisa::compute::Type::from(luisa::string_view(contained_tangent_desc, 4)),::luisa::compute::Type::from(luisa::string_view(reset_frame_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}

template <typename A0>
requires((sizeof(A0) == 32 && alignof(A0) == 16 && std::is_trivially_copyable_v<A0>))
::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
ByteBufferView src_buffer,
ByteBufferView dst_buffer,
::luisa::compute::BufferView<::luisa::Vector<float,3>> last_pos_buffer,
::luisa::compute::BufferView<A0> dq_bone_buffer,
::luisa::compute::BufferView<uint32_t> bone_indices,
::luisa::compute::BufferView<float> bone_weights,
uint32_t bones_count_per_vert,
uint32_t vertex_count,
bool contained_normal,
bool contained_tangent,
bool reset_frame) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 11, _shader->uniform_size());
_cmd_encoder.encode_buffer(src_buffer.handle(),src_buffer.offset_bytes(),src_buffer.size_bytes());
_cmd_encoder.encode_buffer(dst_buffer.handle(),dst_buffer.offset_bytes(),dst_buffer.size_bytes());
_cmd_encoder.encode_buffer(last_pos_buffer.handle(),last_pos_buffer.offset_bytes(),last_pos_buffer.size_bytes());
_cmd_encoder.encode_buffer(dq_bone_buffer.handle(),dq_bone_buffer.offset_bytes(),dq_bone_buffer.size_bytes());
_cmd_encoder.encode_buffer(bone_indices.handle(),bone_indices.offset_bytes(),bone_indices.size_bytes());
_cmd_encoder.encode_buffer(bone_weights.handle(),bone_weights.offset_bytes(),bone_weights.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(bones_count_per_vert), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(vertex_count), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(contained_normal), 1, 1);
_cmd_encoder.encode_uniform(std::addressof(contained_tangent), 1, 1);
_cmd_encoder.encode_uniform(std::addressof(reset_frame), 1, 1);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}