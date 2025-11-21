::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char src_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char bloom_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char tonemap_volume_desc[17] = {116,101,120,116,117,114,101,60,51,44,102,108,111,97,116,62,0};
char args_desc[140] = {115,116,114,117,99,116,60,49,54,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,117,105,110,116,44,50,62,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,98,111,111,108,44,98,111,111,108,62,0};
char lpm_args_desc[41] = {115,116,114,117,99,116,60,49,54,44,97,114,114,97,121,60,118,101,99,116,111,114,60,117,105,110,116,44,52,62,44,49,48,62,44,117,105,110,116,62,0};
char exposure_buffer_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};
char result_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(src_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(bloom_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(tonemap_volume_desc, 16)),::luisa::compute::Type::from(luisa::string_view(args_desc, 139)),::luisa::compute::Type::from(luisa::string_view(lpm_args_desc, 40)),::luisa::compute::Type::from(luisa::string_view(exposure_buffer_desc, 13)),::luisa::compute::Type::from(luisa::string_view(result_desc, 16))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}

template <typename A0,typename A1>
requires((sizeof(A0) == 112 && alignof(A0) == 16 && std::is_trivially_copyable_v<A0>)&&
(sizeof(A1) == 176 && alignof(A1) == 16 && std::is_trivially_copyable_v<A1>))
::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> src_img,
::luisa::compute::ImageView<float> bloom_img,
::luisa::compute::VolumeView<float> tonemap_volume,
A0 const & args,
A1 const & lpm_args,
::luisa::compute::BufferView<float> exposure_buffer,
::luisa::compute::ImageView<float> result) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 7, _shader->uniform_size());
_cmd_encoder.encode_texture(src_img.handle(), src_img.level());
_cmd_encoder.encode_texture(bloom_img.handle(), bloom_img.level());
_cmd_encoder.encode_texture(tonemap_volume.handle(), tonemap_volume.level());
_cmd_encoder.encode_uniform(std::addressof(args), 112, 16);
_cmd_encoder.encode_uniform(std::addressof(lpm_args), 176, 16);
_cmd_encoder.encode_buffer(exposure_buffer.handle(),exposure_buffer.offset_bytes(),exposure_buffer.size_bytes());
_cmd_encoder.encode_texture(result.handle(), result.level());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}