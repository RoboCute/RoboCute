::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char dst_desc[17] = {116,101,120,116,117,114,101,60,51,44,102,108,111,97,116,62,0};
char src_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(dst_desc, 16)),::luisa::compute::Type::from(luisa::string_view(src_desc, 13))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint3 _dispatch_size,
::luisa::compute::VolumeView<float> dst,
::luisa::compute::BufferView<float> src) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 2, _shader->uniform_size());
_cmd_encoder.encode_texture(dst.handle(), dst.level());
_cmd_encoder.encode_buffer(src.handle(),src.offset_bytes(),src.size_bytes());
_cmd_encoder.set_dispatch_size(_dispatch_size);

return std::move(_cmd_encoder).build();
}