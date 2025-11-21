::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char g_vt_level_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char g_buffer_heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char g_image_heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char g_volume_heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char g_triangle_vis_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char g_accel_desc[6] = {97,99,99,101,108,0};
char emission_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char depth_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char mv_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char radiance_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char specular_albedo_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char albedo_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char beta_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char normal_roughness_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char sample_flags_img_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};
char di_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char vbuffer_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};
char sample_desc[5] = {117,105,110,116,0};
char spp_desc[5] = {117,105,110,116,0};
char confidence_map_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char light_dirty_map_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};
char surfel_key_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char surfel_value_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char surfel_mark_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};
char multi_bounce_pixel_desc[80] = {98,117,102,102,101,114,60,115,116,114,117,99,116,60,52,44,97,114,114,97,121,60,102,108,111,97,116,44,51,62,44,117,105,110,116,44,97,114,114,97,121,60,102,108,111,97,116,44,51,62,44,102,108,111,97,116,44,97,114,114,97,121,60,102,108,111,97,116,44,51,62,44,102,108,111,97,116,62,62,0};
char multi_bounce_pixel_counter_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char last_transform_buffer_desc[18] = {98,117,102,102,101,114,60,109,97,116,114,105,120,60,52,62,62,0};
char args_desc[215] = {115,116,114,117,99,116,60,49,54,44,102,108,111,97,116,44,102,108,111,97,116,44,109,97,116,114,105,120,60,51,62,44,109,97,116,114,105,120,60,51,62,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,118,101,99,116,111,114,60,102,108,111,97,116,44,51,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,50,62,44,109,97,116,114,105,120,60,52,62,44,109,97,116,114,105,120,60,52,62,44,109,97,116,114,105,120,60,52,62,44,109,97,116,114,105,120,60,52,62,44,109,97,116,114,105,120,60,52,62,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,118,101,99,116,111,114,60,102,108,111,97,116,44,50,62,44,98,111,111,108,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,117,105,110,116,44,117,105,110,116,62,0};
char size_desc[15] = {118,101,99,116,111,114,60,117,105,110,116,44,50,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(g_vt_level_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(g_buffer_heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(g_image_heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(g_volume_heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(g_triangle_vis_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(g_accel_desc, 5)),::luisa::compute::Type::from(luisa::string_view(emission_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(depth_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(mv_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(radiance_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(specular_albedo_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(albedo_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(beta_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(normal_roughness_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(sample_flags_img_desc, 15)),::luisa::compute::Type::from(luisa::string_view(di_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(vbuffer_desc, 15)),::luisa::compute::Type::from(luisa::string_view(sample_desc, 4)),::luisa::compute::Type::from(luisa::string_view(spp_desc, 4)),::luisa::compute::Type::from(luisa::string_view(confidence_map_desc, 16)),::luisa::compute::Type::from(luisa::string_view(light_dirty_map_desc, 15)),::luisa::compute::Type::from(luisa::string_view(surfel_key_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(surfel_value_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(surfel_mark_desc, 15)),::luisa::compute::Type::from(luisa::string_view(multi_bounce_pixel_desc, 79)),::luisa::compute::Type::from(luisa::string_view(multi_bounce_pixel_counter_desc, 12)),::luisa::compute::Type::from(luisa::string_view(last_transform_buffer_desc, 17)),::luisa::compute::Type::from(luisa::string_view(args_desc, 214)),::luisa::compute::Type::from(luisa::string_view(size_desc, 14))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}

template <typename A0,typename A1>
requires((sizeof(A0) == 48 && alignof(A0) == 4 && std::is_trivially_copyable_v<A0>)&&
(sizeof(A1) == 544 && alignof(A1) == 16 && std::is_trivially_copyable_v<A1>))
::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint2 _dispatch_size,
::luisa::compute::BufferView<uint32_t> g_vt_level_buffer,
::luisa::compute::BindlessArray const & g_buffer_heap,
::luisa::compute::BindlessArray const & g_image_heap,
::luisa::compute::BindlessArray const & g_volume_heap,
::luisa::compute::BufferView<uint32_t> g_triangle_vis_buffer,
::luisa::compute::Accel const & g_accel,
::luisa::compute::ImageView<float> emission_img,
::luisa::compute::ImageView<float> depth_img,
::luisa::compute::ImageView<float> mv_img,
::luisa::compute::ImageView<float> radiance_img,
::luisa::compute::ImageView<float> specular_albedo_img,
::luisa::compute::ImageView<float> albedo_img,
::luisa::compute::ImageView<float> beta_img,
::luisa::compute::ImageView<float> normal_roughness_img,
::luisa::compute::ImageView<uint32_t> sample_flags_img,
::luisa::compute::ImageView<float> di_img,
::luisa::compute::ImageView<uint32_t> vbuffer,
uint32_t sample,
uint32_t spp,
::luisa::compute::ImageView<float> confidence_map,
::luisa::compute::ImageView<uint32_t> light_dirty_map,
::luisa::compute::BufferView<uint32_t> surfel_key_buffer,
::luisa::compute::BufferView<uint32_t> surfel_value_buffer,
::luisa::compute::ImageView<uint32_t> surfel_mark,
::luisa::compute::BufferView<A0> multi_bounce_pixel,
::luisa::compute::BufferView<uint32_t> multi_bounce_pixel_counter,
::luisa::compute::BufferView<::luisa::float4x4> last_transform_buffer,
A1 const & args,
::luisa::Vector<uint32_t,2> size) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 29, _shader->uniform_size());
_cmd_encoder.encode_buffer(g_vt_level_buffer.handle(),g_vt_level_buffer.offset_bytes(),g_vt_level_buffer.size_bytes());
_cmd_encoder.encode_bindless_array(g_buffer_heap.handle());
_cmd_encoder.encode_bindless_array(g_image_heap.handle());
_cmd_encoder.encode_bindless_array(g_volume_heap.handle());
_cmd_encoder.encode_buffer(g_triangle_vis_buffer.handle(),g_triangle_vis_buffer.offset_bytes(),g_triangle_vis_buffer.size_bytes());
_cmd_encoder.encode_accel(g_accel.handle());
_cmd_encoder.encode_texture(emission_img.handle(), emission_img.level());
_cmd_encoder.encode_texture(depth_img.handle(), depth_img.level());
_cmd_encoder.encode_texture(mv_img.handle(), mv_img.level());
_cmd_encoder.encode_texture(radiance_img.handle(), radiance_img.level());
_cmd_encoder.encode_texture(specular_albedo_img.handle(), specular_albedo_img.level());
_cmd_encoder.encode_texture(albedo_img.handle(), albedo_img.level());
_cmd_encoder.encode_texture(beta_img.handle(), beta_img.level());
_cmd_encoder.encode_texture(normal_roughness_img.handle(), normal_roughness_img.level());
_cmd_encoder.encode_texture(sample_flags_img.handle(), sample_flags_img.level());
_cmd_encoder.encode_texture(di_img.handle(), di_img.level());
_cmd_encoder.encode_texture(vbuffer.handle(), vbuffer.level());
_cmd_encoder.encode_uniform(std::addressof(sample), 4, 4);
_cmd_encoder.encode_uniform(std::addressof(spp), 4, 4);
_cmd_encoder.encode_texture(confidence_map.handle(), confidence_map.level());
_cmd_encoder.encode_texture(light_dirty_map.handle(), light_dirty_map.level());
_cmd_encoder.encode_buffer(surfel_key_buffer.handle(),surfel_key_buffer.offset_bytes(),surfel_key_buffer.size_bytes());
_cmd_encoder.encode_buffer(surfel_value_buffer.handle(),surfel_value_buffer.offset_bytes(),surfel_value_buffer.size_bytes());
_cmd_encoder.encode_texture(surfel_mark.handle(), surfel_mark.level());
_cmd_encoder.encode_buffer(multi_bounce_pixel.handle(),multi_bounce_pixel.offset_bytes(),multi_bounce_pixel.size_bytes());
_cmd_encoder.encode_buffer(multi_bounce_pixel_counter.handle(),multi_bounce_pixel_counter.offset_bytes(),multi_bounce_pixel_counter.size_bytes());
_cmd_encoder.encode_buffer(last_transform_buffer.handle(),last_transform_buffer.offset_bytes(),last_transform_buffer.size_bytes());
_cmd_encoder.encode_uniform(std::addressof(args), 544, 16);
_cmd_encoder.encode_uniform(std::addressof(size), 8, 8);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u));

return std::move(_cmd_encoder).build();
}