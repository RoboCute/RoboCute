::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char gbuffers_desc[63] = {98,117,102,102,101,114,60,115,116,114,117,99,116,60,52,44,97,114,114,97,121,60,102,108,111,97,116,44,52,62,44,97,114,114,97,121,60,102,108,111,97,116,44,51,62,44,97,114,114,97,121,60,102,108,111,97,116,44,51,62,62,62,0};
char emission_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char value_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char surfel_mark_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};
char max_accum_desc[5] = {117,105,110,116,0};
char emission_rate_desc[6] = {102,108,111,97,116,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(gbuffers_desc, 62)),::luisa::compute::Type::from(luisa::string_view(emission_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(value_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(surfel_mark_desc, 15)),::luisa::compute::Type::from(luisa::string_view(max_accum_desc, 4)),::luisa::compute::Type::from(luisa::string_view(emission_rate_desc, 5))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}

template <typename A0>
requires((sizeof(A0) == 40 && alignof(A0) == 4 && std::is_trivially_copyable_v<A0>))
::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::BufferView<A0> gbuffers,
::luisa::compute::ImageView<float> emission_img,
::luisa::compute::BufferView<uint32_t> value_buffer,
::luisa::compute::ImageView<uint32_t> surfel_mark,
uint32_t max_accum,
float emission_rate) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 6, _shader->uniform_size());
_cmd_encoder.encode_buffer(gbuffers.handle(),gbuffers.offset_bytes(),gbuffers.size_bytes());
_cmd_encoder.encode_texture(emission_img.handle(), emission_img.level());
_cmd_encoder.encode_buffer(value_buffer.handle(),value_buffer.offset_bytes(),value_buffer.size_bytes());
_cmd_encoder.encode_texture(surfel_mark.handle(), surfel_mark.level());
_cmd_encoder.encode_uniform(std::addressof(max_accum), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(emission_rate), 4, 4);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}