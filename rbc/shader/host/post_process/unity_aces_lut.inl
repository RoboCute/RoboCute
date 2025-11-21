::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char dst_volume_desc[17] = {116,101,120,116,117,114,101,60,51,44,102,108,111,97,116,62,0};
char curve_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char args_desc[225] = {115,116,114,117,99,116,60,49,54,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,44,105,110,116,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,62,0};
char disable_aces_desc[5] = {98,111,111,108,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(dst_volume_desc, 16)),::luisa::compute::Type::from(luisa::string_view(curve_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(args_desc, 224)),::luisa::compute::Type::from(luisa::string_view(disable_aces_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}

template <typename A0>
requires((sizeof(A0) == 192 && alignof(A0) == 16 && std::is_trivially_copyable_v<A0>))
::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint3 _dispatch_size,
::luisa::compute::VolumeView<float> dst_volume,
::luisa::compute::ImageView<float> curve_img,
A0 const & args,
bool disable_aces) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 4, _shader->uniform_size());
_cmd_encoder.encode_texture(dst_volume.handle(), dst_volume.level());
_cmd_encoder.encode_texture(curve_img.handle(), curve_img.level());
_cmd_encoder.encode_uniform(std::addressof(args), 192, 16);
_cmd_encoder.encode_uniform(std::addressof(disable_aces), 1, 1);
_cmd_encoder.set_dispatch_size(_dispatch_size);

return std::move(_cmd_encoder).build();
}