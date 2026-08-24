#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace rbc_shader {

// Self-contained SHA-256 (FIPS 180-4). No third-party dependency.
class Sha256 {
public:
    Sha256() noexcept;
    void update(std::string_view data) noexcept;
    void update(std::uint8_t const *data, std::size_t size) noexcept;
    // Finalizes and returns the hex digest (lowercase, 64 chars).
    [[nodiscard]] std::string hex() noexcept;
    // Finalizes and returns the raw 32-byte digest.
    [[nodiscard]] std::array<std::uint8_t, 32> raw() noexcept;

private:
    void transform(std::uint8_t const *block) noexcept;
    std::uint32_t _state[8];
    std::uint64_t _bit_count;
    std::uint8_t _buffer[64];
    std::size_t _buffer_len;
    bool _finalized;
};

// Convenience helpers.
[[nodiscard]] std::string sha256_hex(std::string_view data);
[[nodiscard]] std::string sha256_file(std::filesystem::path const &path);
[[nodiscard]] std::string sha256_bytes(std::uint8_t const *data, std::size_t size);

} // namespace rbc_shader
