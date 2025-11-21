::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char curr_key_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char curr_value_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char last_key_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char last_value_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char curr_cam_pos_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,51,62,0};
char last_cam_pos_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,51,62,0};
char map_size_desc[5] = {117,105,110,116,0};
char frame_index_desc[5] = {117,105,110,116,0};
char grid_size_desc[6] = {102,108,111,97,116,0};
char max_offset_desc[5] = {117,105,110,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(curr_key_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(curr_value_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(last_key_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(last_value_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(curr_cam_pos_desc, 15)),::luisa::compute::Type::from(luisa::string_view(last_cam_pos_desc, 15)),::luisa::compute::Type::from(luisa::string_view(map_size_desc, 4)),::luisa::compute::Type::from(luisa::string_view(frame_index_desc, 4)),::luisa::compute::Type::from(luisa::string_view(grid_size_desc, 5)),::luisa::compute::Type::from(luisa::string_view(max_offset_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint _dispatch_size,
::luisa::compute::BufferView<uint32_t> curr_key_buffer,
::luisa::compute::BufferView<uint32_t> curr_value_buffer,
::luisa::compute::BufferView<uint32_t> last_key_buffer,
::luisa::compute::BufferView<uint32_t> last_value_buffer,
::luisa::Vector<float,3> curr_cam_pos,
::luisa::Vector<float,3> last_cam_pos,
uint32_t map_size,
uint32_t frame_index,
float grid_size,
uint32_t max_offset) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 10, _shader->uniform_size());
_cmd_encoder.encode_buffer(curr_key_buffer.handle(),curr_key_buffer.offset_bytes(),curr_key_buffer.size_bytes());
_cmd_encoder.encode_buffer(curr_value_buffer.handle(),curr_value_buffer.offset_bytes(),curr_value_buffer.size_bytes());
_cmd_encoder.encode_buffer(last_key_buffer.handle(),last_key_buffer.offset_bytes(),last_key_buffer.size_bytes());
_cmd_encoder.encode_buffer(last_value_buffer.handle(),last_value_buffer.offset_bytes(),last_value_buffer.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(curr_cam_pos), 16, 16);
_cmd_encoder.encode_uniform(std::addressof(last_cam_pos), 16, 16);
_cmd_encoder.encode_uniform(std::addressof(map_size), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(frame_index), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(grid_size), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(max_offset), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u,1u));

return std::move(_cmd_encoder).build();
}