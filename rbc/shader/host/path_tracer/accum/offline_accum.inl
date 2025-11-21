::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char in_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char out_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char temp_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char render_resolution_desc[15] = {118,101,99,116,111,114,60,117,105,110,116,44,50,62,0};
char frame_desc[5] = {117,105,110,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(in_desc, 16)),::luisa::compute::Type::from(luisa::string_view(out_desc, 16)),::luisa::compute::Type::from(luisa::string_view(temp_desc, 16)),::luisa::compute::Type::from(luisa::string_view(render_resolution_desc, 14)),::luisa::compute::Type::from(luisa::string_view(frame_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> in,
::luisa::compute::ImageView<float> out,
::luisa::compute::ImageView<float> temp,
::luisa::Vector<uint32_t,2> render_resolution,
uint32_t frame) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 5, _shader->uniform_size());
_cmd_encoder.encode_texture(in.handle(), in.level());
_cmd_encoder.encode_texture(out.handle(), out.level());
_cmd_encoder.encode_texture(temp.handle(), temp.level());
_cmd_encoder.encode_uniform(std::addressof(render_resolution), 8, 8);
_cmd_encoder.encode_uniform(std::addressof(frame), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}