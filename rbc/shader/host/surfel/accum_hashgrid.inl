::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char radiance_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char value_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char surfel_mark_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};
char exposure_buffer_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(radiance_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(value_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(surfel_mark_desc, 15)),::luisa::compute::Type::from(luisa::string_view(exposure_buffer_desc, 13))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> radiance_img,
::luisa::compute::BufferView<uint32_t> value_buffer,
::luisa::compute::ImageView<uint32_t> surfel_mark,
::luisa::compute::BufferView<float> exposure_buffer) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 4, _shader->uniform_size());
_cmd_encoder.encode_texture(radiance_img.handle(), radiance_img.level());
_cmd_encoder.encode_buffer(value_buffer.handle(),value_buffer.offset_bytes(),value_buffer.size_bytes());
_cmd_encoder.encode_texture(surfel_mark.handle(), surfel_mark.level());
_cmd_encoder.encode_buffer(exposure_buffer.handle(),exposure_buffer.offset_bytes(),exposure_buffer.size_bytes());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}