::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char img_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};
char indices_desc[23] = {98,117,102,102,101,114,60,118,101,99,116,111,114,60,117,105,110,116,44,50,62,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(img_desc, 15)),::luisa::compute::Type::from(luisa::string_view(indices_desc, 22))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint _dispatch_size,
::luisa::compute::ImageView<uint32_t> img,
::luisa::compute::BufferView<::luisa::Vector<uint32_t,2>> indices) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 2, _shader->uniform_size());
_cmd_encoder.encode_texture(img.handle(), img.level());
_cmd_encoder.encode_buffer(indices.handle(),indices.offset_bytes(),indices.size_bytes());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u,1u));

return std::move(_cmd_encoder).build();
}