::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char dst_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char ctl_desc[25] = {97,114,114,97,121,60,118,101,99,116,111,114,60,117,105,110,116,44,52,62,44,49,48,62,0};
char shoulder_desc[5] = {98,111,111,108,0};
char con_desc[5] = {98,111,111,108,0};
char soft_desc[5] = {98,111,111,108,0};
char con2_desc[5] = {98,111,111,108,0};
char clip_desc[5] = {98,111,111,108,0};
char scaleOnly_desc[5] = {98,111,111,108,0};
char pad_desc[15] = {118,101,99,116,111,114,60,117,105,110,116,44,50,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(dst_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(ctl_desc, 24)),::luisa::compute::Type::from(luisa::string_view(shoulder_desc, 4)),::luisa::compute::Type::from(luisa::string_view(con_desc, 4)),::luisa::compute::Type::from(luisa::string_view(soft_desc, 4)),::luisa::compute::Type::from(luisa::string_view(con2_desc, 4)),::luisa::compute::Type::from(luisa::string_view(clip_desc, 4)),::luisa::compute::Type::from(luisa::string_view(scaleOnly_desc, 4)),::luisa::compute::Type::from(luisa::string_view(pad_desc, 14))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::ImageView<float> img,
::luisa::compute::ImageView<float> dst_img,
std::array<::luisa::Vector<uint32_t,4>,10> const & ctl,
bool shoulder,
bool con,
bool soft,
bool con2,
bool clip,
bool scaleOnly,
::luisa::Vector<uint32_t,2> pad) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 10, _shader->uniform_size());
_cmd_encoder.encode_texture(img.handle(), img.level());
_cmd_encoder.encode_texture(dst_img.handle(), dst_img.level());
_cmd_encoder.encode_uniform(ctl.data(), 160, 16);
_cmd_encoder.encode_uniform(std::addressof(shoulder), 1, 1);
_cmd_encoder.encode_uniform(std::addressof(con), 1, 1);
_cmd_encoder.encode_uniform(std::addressof(soft), 1, 1);
_cmd_encoder.encode_uniform(std::addressof(con2), 1, 1);
_cmd_encoder.encode_uniform(std::addressof(clip), 1, 1);
_cmd_encoder.encode_uniform(std::addressof(scaleOnly), 1, 1);
_cmd_encoder.encode_uniform(std::addressof(pad), 8, 8);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}