#include "variant_util.h"
#include "sha256.h"

#include <yyjson.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <random>
#include <set>
#include <system_error>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace rbc_shader {

// ---------------------------------------------------------------------------
// Json value
// ---------------------------------------------------------------------------
Json Json::null() {
    Json value;
    value.type = Type::Null;
    return value;
}
Json Json::make_bool(bool value) {
    Json result;
    result.type = Type::Bool;
    result.boolean = value;
    return result;
}
Json Json::make_int(int64_t value) {
    Json result;
    result.type = Type::Int;
    result.integer = value;
    return result;
}
Json Json::make_string(std::string value) {
    Json result;
    result.type = Type::String;
    result.str = std::move(value);
    return result;
}
Json Json::make_array(std::vector<Json> value) {
    Json result;
    result.type = Type::Array;
    result.arr = std::move(value);
    return result;
}
Json Json::make_object(std::vector<std::pair<std::string, Json>> value) {
    Json result;
    result.type = Type::Object;
    result.obj = std::move(value);
    return result;
}

Json const *Json::get(std::string_view key) const {
    for (auto const &entry : obj) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}
Json *Json::get_mut(std::string_view key) {
    for (auto &entry : obj) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}
void Json::set(std::string key, Json value) {
    for (auto &entry : obj) {
        if (entry.first == key) {
            entry.second = std::move(value);
            return;
        }
    }
    obj.emplace_back(std::move(key), std::move(value));
}

namespace {

// UTF-8 decode: returns code point and advances. Returns -1 on invalid input.
int decode_utf8(std::string_view text, std::size_t &index) {
    auto lead = static_cast<std::uint8_t>(text[index]);
    if (lead < 0x80) {
        return lead;
    }
    int count = 0;
    int code = 0;
    if ((lead & 0xe0u) == 0xc0u) {
        count = 1;
        code = lead & 0x1fu;
    } else if ((lead & 0xf0u) == 0xe0u) {
        count = 2;
        code = lead & 0x0fu;
    } else if ((lead & 0xf8u) == 0xf0u) {
        count = 3;
        code = lead & 0x07u;
    } else {
        return -1;
    }
    if (index + static_cast<std::size_t>(count) >= text.size()) {
        return -1;
    }
    for (int i = 1; i <= count; ++i) {
        auto cont = static_cast<std::uint8_t>(text[index + static_cast<std::size_t>(i)]);
        if ((cont & 0xc0u) != 0x80u) {
            return -1;
        }
        code = (code << 6) | (cont & 0x3fu);
    }
    index += static_cast<std::size_t>(count);
    return code;
}

void append_json_escaped(std::string &out, std::string_view text) {
    static constexpr char hex[] = "0123456789abcdef";
    std::size_t i = 0;
    while (i < text.size()) {
        auto c = static_cast<std::uint8_t>(text[i]);
        if (c < 0x80u) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\t': out += "\\t"; break;
                case '\n': out += "\\n"; break;
                case '\f': out += "\\f"; break;
                case '\r': out += "\\r"; break;
                default:
                    if (c < 0x20u) {
                        out += "\\u00";
                        out.push_back(hex[(c >> 4) & 0x0fu]);
                        out.push_back(hex[c & 0x0fu]);
                    } else {
                        out.push_back(static_cast<char>(c));
                    }
            }
            ++i;
        } else {
            auto code = decode_utf8(text, i);
            ++i;
            if (code < 0) {
                // Invalid UTF-8: emit replacement behavior matching Python's
                // surrogateescape? Python would raise; keep a placeholder.
                out += "\\ufffd";
                continue;
            }
            if (code > 0xffff) {
                auto value = static_cast<std::uint32_t>(code) - 0x10000u;
                auto high = 0xd800u + (value >> 10u);
                auto low = 0xdc00u + (value & 0x3ffu);
                out += "\\u";
                out.push_back(hex[(high >> 12) & 0x0fu]);
                out.push_back(hex[(high >> 8) & 0x0fu]);
                out.push_back(hex[(high >> 4) & 0x0fu]);
                out.push_back(hex[high & 0x0fu]);
                out += "\\u";
                out.push_back(hex[(low >> 12) & 0x0fu]);
                out.push_back(hex[(low >> 8) & 0x0fu]);
                out.push_back(hex[(low >> 4) & 0x0fu]);
                out.push_back(hex[low & 0x0fu]);
            } else {
                out += "\\u";
                out.push_back(hex[(code >> 12) & 0x0fu]);
                out.push_back(hex[(code >> 8) & 0x0fu]);
                out.push_back(hex[(code >> 4) & 0x0fu]);
                out.push_back(hex[code & 0x0fu]);
            }
        }
    }
}

void canonical_json_impl(Json const &value, std::string &out) {
    switch (value.type) {
        case Json::Type::Null:
            out += "null";
            break;
        case Json::Type::Bool:
            out += value.boolean ? "true" : "false";
            break;
        case Json::Type::Int:
            out += std::to_string(value.integer);
            break;
        case Json::Type::String:
            out.push_back('"');
            append_json_escaped(out, value.str);
            out.push_back('"');
            break;
        case Json::Type::Array: {
            out.push_back('[');
            bool first = true;
            for (auto const &item : value.arr) {
                if (!first) {
                    out.push_back(',');
                }
                first = false;
                canonical_json_impl(item, out);
            }
            out.push_back(']');
            break;
        }
        case Json::Type::Object: {
            std::vector<std::pair<std::string, Json const *>> entries;
            entries.reserve(value.obj.size());
            for (auto const &entry : value.obj) {
                entries.emplace_back(entry.first, &entry.second);
            }
            std::sort(entries.begin(), entries.end(), [](auto const &a, auto const &b) {
                return a.first < b.first;
            });
            out.push_back('{');
            bool first = true;
            for (auto const &entry : entries) {
                if (!first) {
                    out.push_back(',');
                }
                first = false;
                out.push_back('"');
                append_json_escaped(out, entry.first);
                out.push_back('"');
                out.push_back(':');
                canonical_json_impl(*entry.second, out);
            }
            out.push_back('}');
            break;
        }
    }
}

void pretty_json_impl(Json const &value, std::string &out, int depth) {
    auto indent = [&](int level) {
        for (int i = 0; i < level; ++i) {
            out += "  ";
        }
    };
    switch (value.type) {
        case Json::Type::Null:
            out += "null";
            break;
        case Json::Type::Bool:
            out += value.boolean ? "true" : "false";
            break;
        case Json::Type::Int:
            out += std::to_string(value.integer);
            break;
        case Json::Type::String:
            out.push_back('"');
            append_json_escaped(out, value.str);
            out.push_back('"');
            break;
        case Json::Type::Array:
            if (value.arr.empty()) {
                out += "[]";
                break;
            }
            out.push_back('[');
            for (std::size_t i = 0; i < value.arr.size(); ++i) {
                if (i != 0) {
                    out.push_back(',');
                }
                out.push_back('\n');
                indent(depth + 1);
                pretty_json_impl(value.arr[i], out, depth + 1);
            }
            out.push_back('\n');
            indent(depth);
            out.push_back(']');
            break;
        case Json::Type::Object:
            if (value.obj.empty()) {
                out += "{}";
                break;
            }
            out.push_back('{');
            for (std::size_t i = 0; i < value.obj.size(); ++i) {
                if (i != 0) {
                    out.push_back(',');
                }
                out.push_back('\n');
                indent(depth + 1);
                out.push_back('"');
                append_json_escaped(out, value.obj[i].first);
                out += "\": ";
                pretty_json_impl(value.obj[i].second, out, depth + 1);
            }
            out.push_back('\n');
            indent(depth);
            out.push_back('}');
            break;
    }
}

} // namespace

std::string canonical_json(Json const &value) {
    std::string out;
    canonical_json_impl(value, out);
    return out;
}

std::string pretty_json(Json const &value) {
    std::string out;
    pretty_json_impl(value, out, 0);
    out.push_back('\n');
    return out;
}

// ---------------------------------------------------------------------------
// yyjson parsing helpers
// ---------------------------------------------------------------------------
namespace {

Json json_from_yyjson(yyjson_val *value) {
    if (value == nullptr) {
        return Json::null();
    }
    switch (unsafe_yyjson_get_type(value)) {
        case YYJSON_TYPE_NULL:
            return Json::null();
        case YYJSON_TYPE_BOOL:
            return Json::make_bool(unsafe_yyjson_get_bool(value));
        case YYJSON_TYPE_NUM: {
            if (unsafe_yyjson_is_int(value)) {
                return Json::make_int(static_cast<int64_t>(unsafe_yyjson_get_int(value)));
            }
            if (unsafe_yyjson_is_uint(value)) {
                return Json::make_int(static_cast<int64_t>(unsafe_yyjson_get_uint(value)));
            }
            // Payloads never use floats; parse as int when integral.
            auto number = unsafe_yyjson_get_real(value);
            return Json::make_int(static_cast<int64_t>(number));
        }
        case YYJSON_TYPE_STR:
            return Json::make_string(std::string(unsafe_yyjson_get_str(value),
                                            unsafe_yyjson_get_len(value)));
        case YYJSON_TYPE_ARR: {
            std::vector<Json> items;
            yyjson_arr_iter iter;
            yyjson_arr_iter_init(value, &iter);
            yyjson_val *item;
            while ((item = yyjson_arr_iter_next(&iter))) {
                items.push_back(json_from_yyjson(item));
            }
            return Json::make_array(std::move(items));
        }
        case YYJSON_TYPE_OBJ: {
            std::vector<std::pair<std::string, Json>> entries;
            yyjson_obj_iter iter;
            yyjson_obj_iter_init(value, &iter);
            yyjson_val *key;
            while ((key = yyjson_obj_iter_next(&iter))) {
                auto item = yyjson_obj_iter_get_val(key);
                entries.emplace_back(
                    std::string(unsafe_yyjson_get_str(key), unsafe_yyjson_get_len(key)),
                    json_from_yyjson(item));
            }
            return Json::make_object(std::move(entries));
        }
        default:
            return Json::null();
    }
}

} // namespace

Json read_json_file(std::filesystem::path const &path, std::string const &context) {
    std::string text;
    try {
        text = read_file_text(path);
    } catch (std::exception const &error) {
        throw ShaderVariantError("Cannot read " + context + ": " + error.what());
    }
    yyjson_read_err error{};
    auto doc = yyjson_read_opts(text.data(), text.size(), 0, nullptr, &error);
    if (doc == nullptr) {
        std::string message = error.msg ? error.msg : "unknown JSON error";
        throw ShaderVariantError("Cannot read " + context + ": " + message);
    }
    auto root = yyjson_doc_get_root(doc);
    Json result = json_from_yyjson(root);
    yyjson_doc_free(doc);
    return result;
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------
std::string read_file_text(std::filesystem::path const &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw ShaderVariantError("Cannot open file: " + path.string());
    }
    std::string content;
    stream.seekg(0, std::ios::end);
    auto size = stream.tellg();
    stream.seekg(0, std::ios::beg);
    if (size > 0) {
        content.resize(static_cast<std::size_t>(size));
        stream.read(content.data(), static_cast<std::streamsize>(size));
    }
    return content;
}

std::vector<std::byte> read_file_bytes(std::filesystem::path const &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw ShaderVariantError("Cannot open file: " + path.string());
    }
    std::vector<std::byte> content;
    stream.seekg(0, std::ios::end);
    auto size = stream.tellg();
    stream.seekg(0, std::ios::beg);
    if (size > 0) {
        content.resize(static_cast<std::size_t>(size));
        stream.read(reinterpret_cast<char *>(content.data()), static_cast<std::streamsize>(size));
    }
    return content;
}

void write_file_bytes(std::filesystem::path const &path, std::vector<std::byte> const &bytes, bool write_if_different) {
    if (write_if_different && std::filesystem::is_regular_file(path)) {
        auto existing = read_file_bytes(path);
        if (existing == bytes) {
            return;
        }
    }
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw ShaderVariantError("Cannot write file: " + path.string());
    }
    if (!bytes.empty()) {
        stream.write(reinterpret_cast<char const *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    stream.close();
    if (!stream) {
        throw ShaderVariantError("Failed writing file: " + path.string());
    }
}

void write_file_text(std::filesystem::path const &path, std::string const &text, bool write_if_different) {
    std::vector<std::byte> bytes(reinterpret_cast<std::byte const *>(text.data()),
                                 reinterpret_cast<std::byte const *>(text.data() + text.size()));
    write_file_bytes(path, bytes, write_if_different);
}

void write_json_atomic(std::filesystem::path const &path, Json const &value, bool write_if_different) {
    auto serialized = pretty_json(value);
    if (write_if_different && std::filesystem::is_regular_file(path)) {
        auto existing = read_file_text(path);
        if (existing == serialized) {
            return;
        }
    }
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    auto temporary = path;
    temporary += ".tmp";
    thread_local std::mt19937_64 rng{std::random_device{}()};
    temporary += std::to_string(rng());
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            throw ShaderVariantError("Cannot write temporary file: " + temporary.string());
        }
        stream.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        stream.close();
        if (!stream) {
            throw ShaderVariantError("Failed writing temporary file: " + temporary.string());
        }
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        // Fall back to remove + rename (Windows rename-over-existing semantics).
        std::filesystem::remove(path, ec);
        std::filesystem::rename(temporary, path, ec);
        if (ec) {
            throw ShaderVariantError("Cannot replace file: " + path.string() + ": " + ec.message());
        }
    }
}

// ---------------------------------------------------------------------------
// FileLock
// ---------------------------------------------------------------------------
#ifdef _WIN32
namespace {
std::string win32_message(DWORD code) {
    char *buffer = nullptr;
    auto length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                     FORMAT_MESSAGE_IGNORE_INSERTS,
                                 nullptr, code, 0, reinterpret_cast<char *>(&buffer), 0, nullptr);
    std::string message;
    if (buffer != nullptr) {
        message.assign(buffer, length);
        LocalFree(buffer);
    }
    while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' ')) {
        message.pop_back();
    }
    return message;
}
} // namespace
#endif

FileLock::FileLock(std::filesystem::path path, double timeout_seconds, double poll_seconds)
    : _path(std::move(path)), _timeout_seconds(timeout_seconds), _poll_seconds(poll_seconds) {}

FileLock::~FileLock() {
    if (_locked) {
        unlock();
    }
#ifdef _WIN32
    if (_handle != nullptr) {
        CloseHandle(static_cast<HANDLE>(_handle));
        _handle = nullptr;
    }
#else
    if (_fd >= 0) {
        ::close(_fd);
        _fd = -1;
    }
#endif
}

void FileLock::lock() {
    if (_locked) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(_path.parent_path(), ec);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(_timeout_seconds);
#ifdef _WIN32
    HANDLE handle = nullptr;
    while (handle == nullptr) {
        handle = CreateFileA(_path.string().c_str(), GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            handle = nullptr;
            if (std::chrono::steady_clock::now() >= deadline) {
                throw ShaderVariantError("Timed out initializing shader build lock: " + _path.string());
            }
            std::this_thread::sleep_for(std::chrono::duration<double>(_poll_seconds));
        }
    }
    _handle = handle;
    // Ensure the file has at least one byte so LockFileEx regions are valid.
    LARGE_INTEGER size{};
    GetFileSizeEx(static_cast<HANDLE>(_handle), &size);
    if (size.QuadPart == 0) {
        DWORD written = 0;
        WriteFile(static_cast<HANDLE>(_handle), "\0", 1, &written, nullptr);
    }
    OVERLAPPED overlapped{};
    while (true) {
        if (LockFileEx(static_cast<HANDLE>(_handle), LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                       0, 1, 0, &overlapped)) {
            _locked = true;
            return;
        }
        auto error = GetLastError();
        if (error != ERROR_LOCK_VIOLATION && error != ERROR_IO_PENDING) {
            throw ShaderVariantError("Failed to acquire shader build lock: " + _path.string() + ": " + win32_message(error));
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            throw ShaderVariantError("Timed out waiting for shader build lock: " + _path.string());
        }
        std::this_thread::sleep_for(std::chrono::duration<double>(_poll_seconds));
    }
#else
    int fd = -1;
    while (fd < 0) {
        fd = ::open(_path.string().c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
        if (fd < 0) {
            if (std::chrono::steady_clock::now() >= deadline) {
                throw ShaderVariantError("Timed out initializing shader build lock: " + _path.string());
            }
            std::this_thread::sleep_for(std::chrono::duration<double>(_poll_seconds));
        }
    }
    _fd = fd;
    struct stat st{};
    if (::fstat(fd, &st) == 0 && st.st_size == 0) {
        if (::write(fd, "\0", 1) < 0) {
            // ignore
        }
    }
    while (true) {
        if (::flock(fd, LOCK_EX | LOCK_NB) == 0) {
            _locked = true;
            return;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            throw ShaderVariantError("Timed out waiting for shader build lock: " + _path.string());
        }
        std::this_thread::sleep_for(std::chrono::duration<double>(_poll_seconds));
    }
#endif
}

void FileLock::unlock() {
    if (!_locked) {
        return;
    }
#ifdef _WIN32
    OVERLAPPED overlapped{};
    UnlockFileEx(static_cast<HANDLE>(_handle), 0, 1, 0, &overlapped);
#else
    ::flock(_fd, LOCK_UN);
#endif
    _locked = false;
}

// ---------------------------------------------------------------------------
// Filesystem helpers
// ---------------------------------------------------------------------------
void remove_path(std::filesystem::path const &path) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        std::filesystem::remove_all(path, ec);
        if (ec) {
            throw ShaderVariantError("Cannot remove directory: " + path.string() + ": " + ec.message());
        }
    } else if (std::filesystem::exists(path, ec)) {
        std::filesystem::remove(path, ec);
        if (ec) {
            throw ShaderVariantError("Cannot remove file: " + path.string() + ": " + ec.message());
        }
    }
}

std::filesystem::path directory_backup_path(std::filesystem::path const &destination) {
    auto parent = destination.parent_path();
    return parent / ("." + destination.filename().string() + ".backup");
}

void recover_directory_backup(std::filesystem::path const &destination) {
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    auto backup = directory_backup_path(destination);
    if (!std::filesystem::exists(backup, ec)) {
        return;
    }
    if (std::filesystem::exists(destination, ec)) {
        remove_path(backup);
    } else {
        std::filesystem::rename(backup, destination, ec);
        if (ec) {
            throw ShaderVariantError("Cannot recover directory backup: " + backup.string() + ": " + ec.message());
        }
    }
}

void atomic_replace_directory(std::filesystem::path const &staged, std::filesystem::path const &destination) {
    // New no-op fast path: skip the whole swap when the staged tree is identical.
    if (std::filesystem::is_directory(destination) && tree_identical(staged, destination)) {
        return;
    }
    recover_directory_backup(destination);
    auto backup = directory_backup_path(destination);
    bool had_destination = std::filesystem::exists(destination);
    std::error_code ec;
    if (had_destination) {
        std::filesystem::rename(destination, backup, ec);
        if (ec) {
            throw ShaderVariantError("Cannot rename destination to backup: " + destination.string() + ": " + ec.message());
        }
    }
    try {
        std::filesystem::rename(staged, destination, ec);
        if (ec) {
            throw ShaderVariantError("Cannot rename staged directory: " + staged.string() + ": " + ec.message());
        }
    } catch (...) {
        if (had_destination && std::filesystem::exists(backup) && !std::filesystem::exists(destination)) {
            std::error_code restore_ec;
            std::filesystem::rename(backup, destination, restore_ec);
        }
        throw;
    }
    if (std::filesystem::exists(backup)) {
        try {
            remove_path(backup);
        } catch (std::exception const &) {
            // New directory is committed; keep backup as recovery journal.
        }
    }
}

bool files_identical(std::filesystem::path const &a, std::filesystem::path const &b) {
    std::error_code ec;
    auto status_a = std::filesystem::status(a, ec);
    if (ec || !std::filesystem::is_regular_file(status_a)) {
        return false;
    }
    auto status_b = std::filesystem::status(b, ec);
    if (ec || !std::filesystem::is_regular_file(status_b)) {
        return false;
    }
    auto size_a = std::filesystem::file_size(a, ec);
    if (ec) {
        return false;
    }
    auto size_b = std::filesystem::file_size(b, ec);
    if (ec) {
        return false;
    }
    if (size_a != size_b) {
        return false;
    }
    return sha256_file(a) == sha256_file(b);
}

bool tree_identical(std::filesystem::path const &a, std::filesystem::path const &b) {
    std::map<std::string, std::filesystem::path> files_a;
    std::error_code ec;
    if (!std::filesystem::is_directory(a, ec)) {
        return false;
    }
    for (auto const &entry : std::filesystem::recursive_directory_iterator(a, ec)) {
        if (entry.is_regular_file(ec)) {
            files_a[path_to_posix(entry.path().lexically_relative(a))] = entry.path();
        }
    }
    if (!std::filesystem::is_directory(b, ec)) {
        return false;
    }
    std::map<std::string, std::filesystem::path> files_b;
    for (auto const &entry : std::filesystem::recursive_directory_iterator(b, ec)) {
        if (entry.is_regular_file(ec)) {
            files_b[path_to_posix(entry.path().lexically_relative(b))] = entry.path();
        }
    }
    if (files_a.size() != files_b.size()) {
        return false;
    }
    for (auto const &entry : files_a) {
        auto iter = files_b.find(entry.first);
        if (iter == files_b.end() || !files_identical(entry.second, iter->second)) {
            return false;
        }
    }
    return true;
}

bool copy_if_different(std::filesystem::path const &src, std::filesystem::path const &dst) {
    std::error_code ec;
    if (std::filesystem::exists(dst, ec) && files_identical(src, dst)) {
        return false;
    }
    std::filesystem::create_directories(dst.parent_path(), ec);
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        throw ShaderVariantError("Cannot copy file: " + src.string() + " -> " + dst.string() + ": " + ec.message());
    }
    return true;
}

void copy_tree(std::filesystem::path const &src, std::filesystem::path const &dst) {
    std::error_code ec;
    std::filesystem::create_directories(dst, ec);
    for (auto const &entry : std::filesystem::recursive_directory_iterator(src, ec)) {
        auto relative = entry.path().lexically_relative(src);
        auto target = dst / relative;
        if (entry.is_directory(ec)) {
            std::filesystem::create_directories(target, ec);
        } else if (entry.is_regular_file(ec)) {
            std::filesystem::create_directories(target.parent_path(), ec);
            std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                throw ShaderVariantError("Cannot copy file: " + entry.path().string() + ": " + ec.message());
            }
        }
    }
}

void copy_tree_if_different(std::filesystem::path const &src, std::filesystem::path const &dst) {
    std::error_code ec;
    std::filesystem::create_directories(dst, ec);
    for (auto const &entry : std::filesystem::recursive_directory_iterator(src, ec)) {
        auto relative = entry.path().lexically_relative(src);
        auto target = dst / relative;
        if (entry.is_directory(ec)) {
            std::filesystem::create_directories(target, ec);
        } else if (entry.is_regular_file(ec)) {
            copy_if_different(entry.path(), target);
        }
    }
}

std::filesystem::path make_temp_dir(std::filesystem::path const &parent, std::string const &prefix) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    thread_local std::mt19937_64 rng{std::random_device{}()};
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto candidate = parent / (prefix + std::to_string(rng()));
        if (std::filesystem::create_directory(candidate, ec) && !ec) {
            return candidate;
        }
    }
    throw ShaderVariantError("Cannot create temporary directory under: " + parent.string());
}

// ---------------------------------------------------------------------------
// Fingerprints and digests
// ---------------------------------------------------------------------------
CompilerInfo compiler_fingerprint(std::filesystem::path const &compiler_path) {
    std::error_code ec;
    auto resolved = std::filesystem::weakly_canonical(compiler_path, ec);
    if (ec || !std::filesystem::is_regular_file(resolved, ec)) {
        throw ShaderVariantError("Shader compiler not found: " + compiler_path.string());
    }
    std::set<std::filesystem::path> related;
    related.insert(resolved);
    static constexpr char const *patterns[] = {"*.dll", "*.so", "*.dylib", "template.txt"};
    auto parent = resolved.parent_path();
    for (auto const *pattern : patterns) {
        for (auto const &entry : std::filesystem::directory_iterator(parent, ec)) {
            auto filename = entry.path().filename().string();
            bool matches = false;
            if (std::string(pattern) == "template.txt") {
                matches = (filename == "template.txt");
            } else {
                // Simple glob for the three library patterns.
                auto dot = filename.rfind('.');
                if (dot != std::string::npos) {
                    auto ext = filename.substr(dot);
                    auto expected = std::string(pattern);
                    if (expected.rfind("*", 0) == 0) {
                        matches = (ext == expected.substr(1));
                    }
                }
            }
            if (matches && entry.is_regular_file(ec)) {
                related.insert(std::filesystem::weakly_canonical(entry.path(), ec));
            }
        }
    }
    std::vector<std::filesystem::path> unique(related.begin(), related.end());
    std::sort(unique.begin(), unique.end(), [](auto const &a, auto const &b) {
        return a.filename().string() < b.filename().string();
    });
    Sha256 digest;
    CompilerInfo info;
    for (auto const &path : unique) {
        std::string file_hash;
        try {
            file_hash = sha256_file(path);
        } catch (std::exception const &) {
            // The toolchain directory may be updated asynchronously (xmake copies
            // DLLs with async=true); skip files that are momentarily unreadable.
            // The next invocation will include them and naturally invalidate the
            // cached fingerprint once the copy completes.
            continue;
        }
        info.files[path.filename().string()] = file_hash;
        auto name = path.filename().string();
        digest.update(name);
        digest.update("\0");
        digest.update(file_hash);
        digest.update("\0");
    }
    info.sha256 = digest.hex();
    info.id = info.sha256.substr(0, 16);
    return info;
}

std::string build_input_id(std::string const &dependency_digest, CompilerInfo const &compiler_info) {
    Json payload = Json::make_object({
        {"compiler", Json::make_string(compiler_info.sha256)},
        {"dependency_digest", Json::make_string(dependency_digest)},
        {"source_schema", Json::make_int(2)},
    });
    return sha256_hex(canonical_json(payload));
}

// ---------------------------------------------------------------------------
// Path defaults / normalization
// ---------------------------------------------------------------------------
std::filesystem::path default_compiler(std::filesystem::path const &project_root) {
#ifdef _WIN32
    return project_root / "build" / "tool" / "rbcxx" / "rbcxx.exe";
#else
    return project_root / "build" / "tool" / "rbcxx" / "rbcxx";
#endif
}

std::filesystem::path default_build_root(std::filesystem::path const &project_root) {
    // Mirror the original Python driver's platform/architecture detection so a
    // fresh checkout on any host resolves a sensible default build directory.
    std::string arch = "x86_x64";
#if defined(_M_X64) || defined(__x86_64__)
    arch = "x64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    arch = "arm64";
#elif defined(_M_IX86) || defined(__i386__) || defined(__i686__)
    arch = "x86";
#endif
    std::string platform = "linux";
#if defined(_WIN32)
    platform = "windows";
#elif defined(__APPLE__)
    platform = "macos";
#endif
    auto directory = platform;
    if (platform == "windows") {
        directory += "/" + arch;
    } else {
        // macOS/Linux follow the Python convention (x86_x64, arm64, x86).
        directory += "/" + (arch == "x64" ? "x86_x64" : arch);
    }
    return project_root / "build" / directory;
}

std::string path_to_posix(std::filesystem::path const &path) {
    auto text = path.generic_string();
#ifdef _WIN32
    for (auto &ch : text) {
        if (ch == '\\') {
            ch = '/';
        }
    }
#endif
    return text;
}

std::string path_casefold(std::filesystem::path const &path) {
    auto text = path_to_posix(path);
    for (auto &ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::string normalize_host_interface(std::string const &content, std::filesystem::path const &shader_root) {
    std::string normalized;
    normalized.reserve(content.size());
    for (std::size_t i = 0; i < content.size();) {
        if (content[i] == '\r') {
            if (i + 1 < content.size() && content[i + 1] == '\n') {
                normalized.push_back('\n');
                i += 2;
            } else {
                normalized.push_back('\n');
                ++i;
            }
        } else {
            normalized.push_back(content[i]);
            ++i;
        }
    }
    std::set<std::string, std::greater<std::string>> root_values;
    auto root = std::filesystem::weakly_canonical(shader_root);
    root_values.insert(root.string());
    auto forward = root.string();
    std::replace(forward.begin(), forward.end(), '\\', '/');
    root_values.insert(forward);
    auto backward = root.string();
    std::replace(backward.begin(), backward.end(), '/', '\\');
    root_values.insert(backward);
    for (auto const &root_value : root_values) {
        std::string replaced;
        std::size_t pos = 0;
        while (pos < normalized.size()) {
            auto found = normalized.find(root_value, pos);
            if (found == std::string::npos) {
                replaced.append(normalized, pos, std::string::npos);
                break;
            }
            replaced.append(normalized, pos, found - pos);
            replaced += "<shader-root>";
            pos = found + root_value.size();
        }
        normalized = std::move(replaced);
    }
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= normalized.size()) {
        auto newline = normalized.find('\n', start);
        if (newline == std::string::npos) {
            lines.emplace_back(normalized.substr(start));
            break;
        }
        lines.emplace_back(normalized.substr(start, newline - start));
        start = newline + 1;
    }
    for (auto &line : lines) {
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.pop_back();
        }
    }
    while (!lines.empty() && lines.back().empty()) {
        lines.pop_back();
    }
    std::string result;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i != 0) {
            result.push_back('\n');
        }
        result += lines[i];
    }
    result.push_back('\n');
    return result;
}

// ---------------------------------------------------------------------------
// Build state marker
// ---------------------------------------------------------------------------
std::optional<BuildState> load_build_state(std::filesystem::path const &build_root) {
    auto path = build_root / ".shader_state.json";
    if (!std::filesystem::is_regular_file(path)) {
        return std::nullopt;
    }
    try {
        auto root = read_json_file(path, "shader state");
        if (!root.is_object()) {
            return std::nullopt;
        }
        BuildState state;
        auto input_id = root.get("input_id");
        auto compiler_sha256 = root.get("compiler_sha256");
        auto compiler = root.get("compiler");
        if (!input_id || !input_id->is_string() || !compiler_sha256 || !compiler_sha256->is_string() ||
            !compiler || !compiler->is_string()) {
            return std::nullopt;
        }
        state.input_id = input_id->str;
        state.compiler_sha256 = compiler_sha256->str;
        state.compiler = compiler->str;
        auto host_out = root.get("host_out");
        if (host_out && host_out->is_string()) {
            state.host_out = host_out->str;
        }
        auto backends = root.get("backends");
        if (backends && backends->is_array()) {
            for (auto const &backend : backends->arr) {
                if (backend.is_string()) {
                    state.backends.push_back(backend.str);
                }
            }
        }
        return state;
    } catch (std::exception const &) {
        return std::nullopt;
    }
}

void save_build_state(std::filesystem::path const &build_root, BuildState const &state) {
    std::vector<Json> backends;
    backends.reserve(state.backends.size());
    for (auto const &backend : state.backends) {
        backends.emplace_back(Json::make_string(backend));
    }
    Json root = Json::make_object({
        {"input_id", Json::make_string(state.input_id)},
        {"compiler_sha256", Json::make_string(state.compiler_sha256)},
        {"compiler", Json::make_string(state.compiler)},
        {"host_out", Json::make_string(state.host_out)},
        {"backends", Json::make_array(std::move(backends))},
    });
    write_json_atomic(build_root / ".shader_state.json", root, true);
}

} // namespace rbc_shader
