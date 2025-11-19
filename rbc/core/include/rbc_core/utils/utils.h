#pragma once
#include <rbc_config.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/vstl/common.h>
namespace rbc
{
RBC_CORE_API luisa::filesystem::path get_binary_path();
} // namespace rbc