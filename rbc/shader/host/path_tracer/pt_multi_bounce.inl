::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){
char g_buffer_heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char g_image_heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char g_volume_heap_desc[15] = {98,105,110,100,108,101,115,115,95,97,114,114,97,121,0};
char g_vt_level_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char g_triangle_vis_buffer_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char g_accel_desc[6] = {97,99,99,101,108,0};
char multi_bounce_pixel_desc[80] = {98,117,102,102,101,114,60,115,116,114,117,99,116,60,52,44,97,114,114,97,121,60,102,108,111,97,116,44,51,62,44,117,105,110,116,44,97,114,114,97,121,60,102,108,111,97,116,44,51,62,44,102,108,111,97,116,44,97,114,114,97,121,60,102,108,111,97,116,44,51,62,44,102,108,111,97,116,62,62,0};
char multi_bounce_pixel_counter_desc[13] = {98,117,102,102,101,114,60,117,105,110,116,62,0};
char radiance_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char confidence_img_desc[17] = {116,101,120,116,117,114,101,60,50,44,102,108,111,97,116,62,0};
char light_dirty_map_desc[16] = {116,101,120,116,117,114,101,60,50,44,117,105,110,116,62,0};
char args_desc[215] = {115,116,114,117,99,116,60,49,54,44,102,108,111,97,116,44,102,108,111,97,116,44,109,97,116,114,105,120,60,51,62,44,109,97,116,114,105,120,60,51,62,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,118,101,99,116,111,114,60,102,108,111,97,116,44,51,62,44,118,101,99,116,111,114,60,102,108,111,97,116,44,50,62,44,109,97,116,114,105,120,60,52,62,44,109,97,116,114,105,120,60,52,62,44,109,97,116,114,105,120,60,52,62,44,109,97,116,114,105,120,60,52,62,44,109,97,116,114,105,120,60,52,62,44,117,105,110,116,44,117,105,110,116,44,117,105,110,116,44,118,101,99,116,111,114,60,102,108,111,97,116,44,50,62,44,98,111,111,108,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,102,108,111,97,116,44,117,105,110,116,44,117,105,110,116,62,0};
char size_desc[15] = {118,101,99,116,111,114,60,117,105,110,116,44,50,62,0};

auto arg_types = {::luisa::compute::Type::from(luisa::string_view(g_buffer_heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(g_image_heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(g_volume_heap_desc, 14)),::luisa::compute::Type::from(luisa::string_view(g_vt_level_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(g_triangle_vis_buffer_desc, 12)),::luisa::compute::Type::from(luisa::string_view(g_accel_desc, 5)),::luisa::compute::Type::from(luisa::string_view(multi_bounce_pixel_desc, 79)),::luisa::compute::Type::from(luisa::string_view(multi_bounce_pixel_counter_desc, 12)),::luisa::compute::Type::from(luisa::string_view(radiance_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(confidence_img_desc, 16)),::luisa::compute::Type::from(luisa::string_view(light_dirty_map_desc, 15)),::luisa::compute::Type::from(luisa::string_view(args_desc, 214)),::luisa::compute::Type::from(luisa::string_view(size_desc, 14))};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}

template <typename A0,typename A1>
requires((sizeof(A0) == 48 && alignof(A0) == 4 && std::is_trivially_copyable_v<A0>)&&
(sizeof(A1) == 544 && alignof(A1) == 16 && std::is_trivially_copyable_v<A1>))
::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint _dispatch_size,
::luisa::compute::BindlessArray const & g_buffer_heap,
::luisa::compute::BindlessArray const & g_image_heap,
::luisa::compute::BindlessArray const & g_volume_heap,
::luisa::compute::BufferView<uint32_t> g_vt_level_buffer,
::luisa::compute::BufferView<uint32_t> g_triangle_vis_buffer,
::luisa::compute::Accel const & g_accel,
::luisa::compute::BufferView<A0> multi_bounce_pixel,
::luisa::compute::BufferView<uint32_t> multi_bounce_pixel_counter,
::luisa::compute::ImageView<float> radiance_img,
::luisa::compute::ImageView<float> confidence_img,
::luisa::compute::ImageView<uint32_t> light_dirty_map,
A1 const & args,
::luisa::Vector<uint32_t,2> size) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 13, _shader->uniform_size());
_cmd_encoder.encode_bindless_array(g_buffer_heap.handle());
_cmd_encoder.encode_bindless_array(g_image_heap.handle());
_cmd_encoder.encode_bindless_array(g_volume_heap.handle());
_cmd_encoder.encode_buffer(g_vt_level_buffer.handle(),g_vt_level_buffer.offset_bytes(),g_vt_level_buffer.size_bytes());
_cmd_encoder.encode_buffer(g_triangle_vis_buffer.handle(),g_triangle_vis_buffer.offset_bytes(),g_triangle_vis_buffer.size_bytes());
_cmd_encoder.encode_accel(g_accel.handle());
_cmd_encoder.encode_buffer(multi_bounce_pixel.handle(),multi_bounce_pixel.offset_bytes(),multi_bounce_pixel.size_bytes());
_cmd_encoder.encode_buffer(multi_bounce_pixel_counter.handle(),multi_bounce_pixel_counter.offset_bytes(),multi_bounce_pixel_counter.size_bytes());
_cmd_encoder.encode_texture(radiance_img.handle(), radiance_img.level());
_cmd_encoder.encode_texture(confidence_img.handle(), confidence_img.level());
_cmd_encoder.encode_texture(light_dirty_map.handle(), light_dirty_map.level());
_cmd_encoder.encode_uniform(std::addressof(args), 544, 16);
_cmd_encoder.encode_uniform(std::addressof(size), 8, 8);
_cmd_encoder.set_dispatch_size(::luisa::make_uint3(_dispatch_size,1u,1u));

return std::move(_cmd_encoder).build();
}