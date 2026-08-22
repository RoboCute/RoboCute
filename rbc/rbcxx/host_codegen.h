#pragma once
#include <luisa/core/stl/string.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/clangcxx/build_arguments.h>
#include <luisa/vstl/common.h>
#include <luisa/vstl/functional.h>
#include <luisa/vstl/string_builder.h>

struct HostCodegen {
    using MapType = vstd::HashMap<vstd::string, vstd::function<void(vstd::StringBuilder &)>>;
    static void codegen_replace(
        vstd::StringBuilder &sb,
        MapType const &replace_funcs,
        luisa::string_view template_type,
        char replace_char);

	static vstd::StringBuilder codegen(
        uint32_t dimension,
		luisa::span<luisa::clangcxx::BuildArgument const> build_args);
    static void write_to(
        luisa::span<luisa::clangcxx::BuildArgument const> build_args,
        uint32_t dimension,
        luisa::filesystem::path const& path
    );
};