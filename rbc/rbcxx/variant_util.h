#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rbc_shader {

class ShaderVariantError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------
// Minimal JSON value type mirroring Python json semantics for this driver.
// ---------------------------------------------------------------------------
class Json {
public:
    enum class Type { Null, Bool, Int, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    int64_t integer = 0;
    std::string str;
    std::vector<Json> arr;
    // Object entries keep insertion order; serializers sort keys as needed.
    std::vector<std::pair<std::string, Json>> obj;

    static Json null();
    static Json make_bool(bool value);
    static Json make_int(int64_t value);
    static Json make_string(std::string value);
    static Json make_array(std::vector<Json> value);
    static Json make_object(std::vector<std::pair<std::string, Json>> value);

    [[nodiscard]] bool is_null() const noexcept { return type == Type::Null; }
    [[nodiscard]] bool is_bool() const noexcept { return type == Type::Bool; }
    [[nodiscard]] bool is_int() const noexcept { return type == Type::Int; }
    [[nodiscard]] bool is_string() const noexcept { return type == Type::String; }
    [[nodiscard]] bool is_array() const noexcept { return type == Type::Array; }
    [[nodiscard]] bool is_object() const noexcept { return type == Type::Object; }

    [[nodiscard]] Json const *get(std::string_view key) const;
    [[nodiscard]] Json *get_mut(std::string_view key);
    void set(std::string key, Json value);
};

// Python json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).
[[nodiscard]] std::string canonical_json(Json const &value);
// Python json.dumps(value, indent=2, ensure_ascii=True) + "\n".
[[nodiscard]] std::string pretty_json(Json const &value);

// Read a JSON file into a Json tree (throws ShaderVariantError on failure).
[[nodiscard]] Json read_json_file(std::filesystem::path const &path, std::string const &context);
// Write JSON atomically (temp file + rename), Python _write_json equivalent.
// When write_if_different is true the existing file is not rewritten when bytes match.
void write_json_atomic(std::filesystem::path const &path, Json const &value, bool write_if_different = true);

[[nodiscard]] std::string read_file_text(std::filesystem::path const &path);
[[nodiscard]] std::vector<std::byte> read_file_bytes(std::filesystem::path const &path);
void write_file_bytes(std::filesystem::path const &path, std::vector<std::byte> const &bytes, bool write_if_different = true);
void write_file_text(std::filesystem::path const &path, std::string const &text, bool write_if_different = true);

// ---------------------------------------------------------------------------
// Cross-process file lock (port of CrossProcessFileLock).
// ---------------------------------------------------------------------------
class FileLock {
public:
    explicit FileLock(std::filesystem::path path,
                      double timeout_seconds = 600.0,
                      double poll_seconds = 0.05);
    ~FileLock();
    FileLock(FileLock const &) = delete;
    FileLock &operator=(FileLock const &) = delete;

    void lock();
    void unlock();

private:
    std::filesystem::path _path;
    double _timeout_seconds;
    double _poll_seconds;
#ifdef _WIN32
    void *_handle = nullptr; // HANDLE
#else
    int _fd = -1;
#endif
    bool _locked = false;
};

// ---------------------------------------------------------------------------
// Filesystem / atomicity helpers (ports from shader_variants.py).
// ---------------------------------------------------------------------------
void remove_path(std::filesystem::path const &path);
[[nodiscard]] std::filesystem::path directory_backup_path(std::filesystem::path const &destination);
void recover_directory_backup(std::filesystem::path const &destination);
void atomic_replace_directory(std::filesystem::path const &staged, std::filesystem::path const &destination);
// Returns true when a copy actually happened.
bool copy_if_different(std::filesystem::path const &src, std::filesystem::path const &dst);
[[nodiscard]] bool files_identical(std::filesystem::path const &a, std::filesystem::path const &b);
[[nodiscard]] bool tree_identical(std::filesystem::path const &a, std::filesystem::path const &b);
void copy_tree(std::filesystem::path const &src, std::filesystem::path const &dst);
void copy_tree_if_different(std::filesystem::path const &src, std::filesystem::path const &dst);
[[nodiscard]] std::filesystem::path make_temp_dir(std::filesystem::path const &parent, std::string const &prefix);

// ---------------------------------------------------------------------------
// Hashing / digests.
// ---------------------------------------------------------------------------
struct CompilerInfo {
    std::string id;      // fingerprint[:16]
    std::string sha256;  // full fingerprint
    std::map<std::string, std::string> files; // name -> sha256
};
[[nodiscard]] CompilerInfo compiler_fingerprint(std::filesystem::path const &compiler_path);
[[nodiscard]] std::string build_input_id(std::string const &dependency_digest, CompilerInfo const &compiler_info);

// ---------------------------------------------------------------------------
// Path defaults and normalization.
// ---------------------------------------------------------------------------
[[nodiscard]] std::filesystem::path default_compiler(std::filesystem::path const &project_root);
[[nodiscard]] std::filesystem::path default_build_root(std::filesystem::path const &project_root);
[[nodiscard]] std::string path_to_posix(std::filesystem::path const &path);
[[nodiscard]] std::string path_casefold(std::filesystem::path const &path);

// \r\n -> \n, strip trailing blank lines, replace shader root with <shader-root>.
[[nodiscard]] std::string normalize_host_interface(std::string const &content,
                                                    std::filesystem::path const &shader_root);

// ---------------------------------------------------------------------------
// Build-state marker (.shader_state.json) for fast no-op --variant=build.
// ---------------------------------------------------------------------------
struct BuildState {
    std::string input_id;
    std::string compiler_sha256;
    std::vector<std::string> backends;
    std::string host_out;   // normalized absolute path, empty when no hostgen
    std::string compiler;   // normalized absolute compiler path
};
[[nodiscard]] std::optional<BuildState> load_build_state(std::filesystem::path const &build_root);
void save_build_state(std::filesystem::path const &build_root, BuildState const &state);

} // namespace rbc_shader
