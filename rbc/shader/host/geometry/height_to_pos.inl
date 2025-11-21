::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char vert_buffer_desc[24] = {98,117,102,102,101,114,60,118,101,99,116,111,114,60,102,108,111,97,116,44,51,62,62,0};
char height_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char start_coord_desc[15] = {118,101,99,116,111,114,60,117,105,110,116,44,50,62,0};
char height_range_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,50,62,0};
char hori_range_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,50,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(vert_buffer_desc, 23)),::luisa::compute::Type::from(luisa::string_view(height_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(start_coord_desc, 14)),::luisa::compute::Type::from(luisa::string_view(height_range_desc, 15)),::luisa::compute::Type::from(luisa::string_view(hori_range_desc, 15))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::BufferView<::luisa::Vector<float,3>> vert_buffer,
::luisa::compute::ImageView<float> height_img,
::luisa::Vector<uint32_t,2> start_coord,
::luisa::Vector<float,2> height_range,
::luisa::Vector<float,2> hori_range) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 5, _shader->uniform_size());
_cmd_encoder.encode_buffer(vert_buffer.handle(),vert_buffer.offset_bytes(),vert_buffer.size_bytes());
_cmd_encoder.encode_texture(height_img.handle(), height_img.level());
_cmd_encoder.encode_uniform(std::addressof(start_coord), 8, 8);
_cmd_encoder.encode_uniform(std::addressof(height_range), 8, 8);
_cmd_encoder.encode_uniform(std::addressof(hori_range), 8, 8);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}