::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char depth_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char normal_rough_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char radiance_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char key_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char value_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char invProj_desc[10] = {109,97,116,114,105,120,60,52,62,0};
char invView_desc[10] = {109,97,116,114,105,120,60,52,62,0};
char jitter_offset_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,50,62,0};
char cam_pos_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,51,62,0};
char grid_size_desc[6] = {102,108,111,97,116,0};
char buffer_size_desc[5] = {117,105,110,116,0};
char max_offset_desc[5] = {117,105,110,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(depth_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(normal_rough_desc, 16)),::luisa::compute::Type::from(luisa::string_view(radiance_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(key_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(value_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(invProj_desc, 9)),::luisa::compute::Type::from(luisa::string_view(invView_desc, 9)),::luisa::compute::Type::from(luisa::string_view(jitter_offset_desc, 15)),::luisa::compute::Type::from(luisa::string_view(cam_pos_desc, 15)),::luisa::compute::Type::from(luisa::string_view(grid_size_desc, 5)),::luisa::compute::Type::from(luisa::string_view(buffer_size_desc, 4)),::luisa::compute::Type::from(luisa::string_view(max_offset_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> depth_img,
::luisa::compute::ImageView<float> normal_rough,
::luisa::compute::ImageView<float> radiance_img,
::luisa::compute::BufferView<uint32_t> key_buffer,
::luisa::compute::BufferView<uint32_t> value_buffer,
::luisa::float4x4 invProj,
::luisa::float4x4 invView,
::luisa::Vector<float,2> jitter_offset,
::luisa::Vector<float,3> cam_pos,
float grid_size,
uint32_t buffer_size,
uint32_t max_offset) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 12, _shader->uniform_size());
_cmd_encoder.encode_texture(depth_img.handle(), depth_img.level());
_cmd_encoder.encode_texture(normal_rough.handle(), normal_rough.level());
_cmd_encoder.encode_texture(radiance_img.handle(), radiance_img.level());
_cmd_encoder.encode_buffer(key_buffer.handle(),key_buffer.offset_bytes(),key_buffer.size_bytes());
_cmd_encoder.encode_buffer(value_buffer.handle(),value_buffer.offset_bytes(),value_buffer.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(invProj), 64, 16);
_cmd_encoder.encode_uniform(std::addressof(invView), 64, 16);
_cmd_encoder.encode_uniform(std::addressof(jitter_offset), 8, 8);
_cmd_encoder.encode_uniform(std::addressof(cam_pos), 16, 16);
_cmd_encoder.encode_uniform(std::addressof(grid_size), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(buffer_size), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(max_offset), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}