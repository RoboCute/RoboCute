::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char histogram_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char source_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char scale_offset_res_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(histogram_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(source_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(scale_offset_res_desc, 15))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::BufferView<uint32_t> histogram_buffer,
::luisa::compute::ImageView<float> source_img,
::luisa::Vector<float,4> scale_offset_res) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 3, _shader->uniform_size());
_cmd_encoder.encode_buffer(histogram_buffer.handle(),histogram_buffer.offset_bytes(),histogram_buffer.size_bytes());
_cmd_encoder.encode_texture(source_img.handle(), source_img.level());
_cmd_encoder.encode_uniform(std::addressof(scale_offset_res), 16, 16);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}