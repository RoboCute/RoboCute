::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char in_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char out_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(in_desc, 16)),::luisa::compute::Type::from(luisa::string_view(out_desc, 13))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> in,
::luisa::compute::BufferView<float> out) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 2, _shader->uniform_size());
_cmd_encoder.encode_texture(in.handle(), in.level());
_cmd_encoder.encode_buffer(out.handle(),out.offset_bytes(),out.size_bytes());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}