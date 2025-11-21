::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char sample_flags_img_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};
char confidence_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char radiance_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char key_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char value_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char grid_element_size_desc[6] = {102,108,111,97,116,0};
char surfel_mark_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(sample_flags_img_desc, 15)),::luisa::compute::Type::from(luisa::string_view(confidence_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(radiance_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(key_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(value_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(grid_element_size_desc, 5)),::luisa::compute::Type::from(luisa::string_view(surfel_mark_desc, 15))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<uint32_t> sample_flags_img,
::luisa::compute::ImageView<float> confidence_img,
::luisa::compute::ImageView<float> radiance_img,
::luisa::compute::BufferView<uint32_t> key_buffer,
::luisa::compute::BufferView<uint32_t> value_buffer,
float grid_element_size,
::luisa::compute::ImageView<uint32_t> surfel_mark) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 7, _shader->uniform_size());
_cmd_encoder.encode_texture(sample_flags_img.handle(), sample_flags_img.level());
_cmd_encoder.encode_texture(confidence_img.handle(), confidence_img.level());
_cmd_encoder.encode_texture(radiance_img.handle(), radiance_img.level());
_cmd_encoder.encode_buffer(key_buffer.handle(),key_buffer.offset_bytes(),key_buffer.size_bytes());
_cmd_encoder.encode_buffer(value_buffer.handle(),value_buffer.offset_bytes(),value_buffer.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(grid_element_size), 4, 4);
_cmd_encoder.encode_texture(surfel_mark.handle(), surfel_mark.level());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}