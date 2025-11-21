::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char image_heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char result_buffer_desc[24] = {98,117,102,102,101,114,60,118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,62,0};
char illum_d65_idx_desc[5] = {117,105,110,116,0};
char cie_xyz_cdfinv_desc[5] = {117,105,110,116,0};
char spectrum_lut3d_idx_desc[5] = {117,105,110,116,0};
char frame_index_desc[5] = {117,105,110,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(image_heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(result_buffer_desc, 23)),::luisa::compute::Type::from(luisa::string_view(illum_d65_idx_desc, 4)),::luisa::compute::Type::from(luisa::string_view(cie_xyz_cdfinv_desc, 4)),::luisa::compute::Type::from(luisa::string_view(spectrum_lut3d_idx_desc, 4)),::luisa::compute::Type::from(luisa::string_view(frame_index_desc, 4))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint3 _dispatch_size,
::luisa::compute::BindlessArray const & image_heap,
::luisa::compute::BufferView<::luisa::Vector<float,4>> result_buffer,
uint32_t illum_d65_idx,
uint32_t cie_xyz_cdfinv,
uint32_t spectrum_lut3d_idx,
uint32_t frame_index) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 6, _shader->uniform_size());
_cmd_encoder.encode_bindless_array(image_heap.handle());
_cmd_encoder.encode_buffer(result_buffer.handle(),result_buffer.offset_bytes(),result_buffer.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(illum_d65_idx), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(cie_xyz_cdfinv), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(spectrum_lut3d_idx), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(frame_index), 4, 4);
_cmd_encoder.set_dispatch_size(_dispatch_size);

return std::move(_cmd_encoder).build();
}