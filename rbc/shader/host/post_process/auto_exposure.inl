::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char global_exposure_desc[6] = {102,108,111,97,116,0};
char _Params1_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,0};
char _Params2_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,0};
char _ScaleOffsetRes_desc[16] = {118,101,99,116,111,114,60,102,108,111,97,116,44,52,62,0};
char progressive_desc[5] = {98,111,111,108,0};
char _HistogramBuffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char SrcDst_desc[14] = {98,117,102,102,101,114,60,102,108,111,97,116,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(global_exposure_desc, 5)),::luisa::compute::Type::from(luisa::string_view(_Params1_desc, 15)),::luisa::compute::Type::from(luisa::string_view(_Params2_desc, 15)),::luisa::compute::Type::from(luisa::string_view(_ScaleOffsetRes_desc, 15)),::luisa::compute::Type::from(luisa::string_view(progressive_desc, 4)),::luisa::compute::Type::from(luisa::string_view(_HistogramBuffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(SrcDst_desc, 13))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
float global_exposure,
::luisa::Vector<float,4> _Params1,
::luisa::Vector<float,4> _Params2,
::luisa::Vector<float,4> _ScaleOffsetRes,
bool progressive,
::luisa::compute::BufferView<uint32_t> _HistogramBuffer,
::luisa::compute::BufferView<float> SrcDst) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 7, _shader->uniform_size());
_cmd_encoder.encode_uniform(std::addressof(global_exposure), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(_Params1), 16, 16);
_cmd_encoder.encode_uniform(std::addressof(_Params2), 16, 16);
_cmd_encoder.encode_uniform(std::addressof(_ScaleOffsetRes), 16, 16);
_cmd_encoder.encode_uniform(std::addressof(progressive), 1, 1);
_cmd_encoder.encode_buffer(_HistogramBuffer.handle(),_HistogramBuffer.offset_bytes(),_HistogramBuffer.size_bytes());
_cmd_encoder.encode_buffer(SrcDst.handle(),SrcDst.offset_bytes(),SrcDst.size_bytes());
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}