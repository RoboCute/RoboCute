::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char noisy_buffer_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};
char denoised_buffer_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};
char img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char weight_desc[6] = {102,108,111,97,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(noisy_buffer_desc, 13)),::luisa::compute::Type::from(luisa::string_view(denoised_buffer_desc, 13)),::luisa::compute::Type::from(luisa::string_view(img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(weight_desc, 5))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::BufferView<float> noisy_buffer,
::luisa::compute::BufferView<float> denoised_buffer,
::luisa::compute::ImageView<float> img,
float weight) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 4, _shader->uniform_size());
_cmd_encoder.encode_buffer(noisy_buffer.handle(),noisy_buffer.offset_bytes(),noisy_buffer.size_bytes());
_cmd_encoder.encode_buffer(denoised_buffer.handle(),denoised_buffer.offset_bytes(),denoised_buffer.size_bytes());
_cmd_encoder.encode_texture(img.handle(), img.level());
_cmd_encoder.encode_uniform(std::addressof(weight), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}