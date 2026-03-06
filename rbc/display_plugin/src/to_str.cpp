#include <rbc_display/to_str.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace luisa {

luisa::wstring string_to_wstring(const luisa::string &str) {
    if (str.empty()) {
        return {};
    }
#ifdef _WIN32
    // Get the required buffer size
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return {};
    }
    // Convert the string
    luisa::wstring result;
    result.resize(static_cast<size_t>(size) - 1);// -1 to exclude null terminator
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), size);
    return result;
#else
    // On non-Windows platforms, use mbstowcs
    size_t size = std::mbstowcs(nullptr, str.c_str(), 0);
    if (size == static_cast<size_t>(-1)) {
        return {};
    }
    luisa::wstring result;
    result.resize(size);
    std::mbstowcs(result.data(), str.c_str(), size + 1);
    return result;
#endif
}

}// namespace luisa
