::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char dst_desc[23] = {98,117,102,102,101,114,60,118,101,99,116,111,114,60,117,105,110,116,44,50,62,62,0};
char src_desc[23] = {98,117,102,102,101,114,60,118,101,99,116,111,114,60,117,105,110,116,44,50,62,62,0};
char indices_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(dst_desc, 22)),::luisa::compute::Type::from(luisa::string_view(src_desc, 22)),::luisa::compute::Type::from(luisa::string_view(indices_desc, 12))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::BufferView<::luisa::Vector<uint32_t,2>> dst,
::luisa::compute::BufferView<::luisa::Vector<uint32_t,2>> src,
::luisa::compute::BufferView<uint32_t> indices) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 3, _shader->uniform_size());
_cmd_encoder.encode_buffer(dst.handle(),dst.offset_bytes(),dst.size_bytes());
_cmd_encoder.encode_buffer(src.handle(),src.offset_bytes(),src.size_bytes());
_cmd_encoder.encode_buffer(indices.handle(),indices.offset_bytes(),indices.size_bytes());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}