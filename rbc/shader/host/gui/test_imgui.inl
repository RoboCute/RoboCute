::luisa::compute::ShaderBase const* load_shader(luisa::filesystem::path const& shader_path){

auto arg_types = {};
return ShaderManager::instance()->load_typeless(
    shader_path,
    arg_types
);
}


::luisa::unique_ptr<::luisa::compute::ShaderDispatchCommand> dispatch_shader(::luisa::compute::ShaderBase const* _shader, ::luisa::uint3156711419 _dispatch_size) {
::luisa::compute::ComputeDispatchCmdEncoder _cmd_encoder(_shader->handle(), 0, _shader->uniform_size());
_cmd_encoder.set_dispatch_size(_dispatch_size);

return std::move(_cmd_encoder).build();
}