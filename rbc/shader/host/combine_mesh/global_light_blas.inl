::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char nodes_desc[76] = {98,117,102,102,101,114,60,115,116,114,117,99,116,60,49,54,44,97,114,114,97,121,60,102,108,111,97,116,44,51,62,44,117,105,110,116,44,97,114,114,97,121,60,102,108,111,97,116,44,51,62,44,102,108,111,97,116,44,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,62,62,0};
char local_to_global_desc[10] = {109,97,116,114,105,120,60,52,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(nodes_desc, 75)),::luisa::compute::Type::from(luisa::string_view(local_to_global_desc, 9))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}

template <typename A0>
requires((sizeof(A0) == 48 && alignof(A0) == 16 && std::is_trivially_copyable_v<A0>))
::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint _dispatch_size,
::luisa::compute::BufferView<A0> nodes,
::luisa::float4x4 local_to_global) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 2, _shader->uniform_size());
_cmd_encoder.encode_buffer(nodes.handle(),nodes.offset_bytes(),nodes.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(local_to_global), 64, 16);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u,1u));

return std::move(_cmd_encoder).build();
}