::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char accel_desc[6] = {97,99,99,101,108,0};
char last_transform_desc[18] = {98,117,102,102,101,114,60,109,97,116,114,105,120,60,52,62,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(accel_desc, 5)),::luisa::compute::Type::from(luisa::string_view(last_transform_desc, 17))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint _dispatch_size,
::luisa::compute::Accel const & accel,
::luisa::compute::BufferView<::luisa::float4x4> last_transform) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 2, _shader->uniform_size());
_cmd_encoder.encode_accel(accel.handle());
_cmd_encoder.encode_buffer(last_transform.handle(),last_transform.offset_bytes(),last_transform.size_bytes());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u,1u));

return std::move(_cmd_encoder).build();
}