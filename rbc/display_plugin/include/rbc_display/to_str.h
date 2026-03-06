#pragma once
#include <luisa/core/stl.h>

namespace luisa {

/**
 * @brief Convert a UTF-8 string to a wide string (UTF-16 on Windows)
 * @param str The input UTF-8 string
 * @return The converted wide string
 */
luisa::wstring string_to_wstring(const luisa::string &str);

}// namespace luisa
