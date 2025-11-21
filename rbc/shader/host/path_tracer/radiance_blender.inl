::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char spec_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char diff_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char last_spec_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char last_diff_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char rate_desc[6] = {102,108,111,97,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(spec_desc, 16)),::luisa::compute::Type::from(luisa::string_view(diff_desc, 16)),::luisa::compute::Type::from(luisa::string_view(last_spec_desc, 16)),::luisa::compute::Type::from(luisa::string_view(last_diff_desc, 16)),::luisa::compute::Type::from(luisa::string_view(rate_desc, 5))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> spec,
::luisa::compute::ImageView<float> diff,
::luisa::compute::ImageView<float> last_spec,
::luisa::compute::ImageView<float> last_diff,
float rate) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 5, _shader->uniform_size());
_cmd_encoder.encode_texture(spec.handle(), spec.level());
_cmd_encoder.encode_texture(diff.handle(), diff.level());
_cmd_encoder.encode_texture(last_spec.handle(), last_spec.level());
_cmd_encoder.encode_texture(last_diff.handle(), last_diff.level());
_cmd_encoder.encode_uniform(std::addressof(rate), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}