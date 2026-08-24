#include "sha256.h"
#include <array>
#include <cstring>
#include <fstream>
#include <vector>

namespace rbc_shader {

namespace {

constexpr std::uint32_t kInitialState[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

constexpr std::uint32_t kRoundConstants[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

inline std::uint32_t rotr(std::uint32_t value, std::uint32_t bits) noexcept {
    return (value >> bits) | (value << (32u - bits));
}

void to_hex(std::uint8_t const *data, std::size_t size, std::string &out) {
    static constexpr char digits[] = "0123456789abcdef";
    out.reserve(out.size() + size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(digits[data[i] >> 4]);
        out.push_back(digits[data[i] & 0x0f]);
    }
}

} // namespace

Sha256::Sha256() noexcept {
    std::memcpy(_state, kInitialState, sizeof(_state));
    _bit_count = 0;
    _buffer_len = 0;
    _finalized = false;
}

void Sha256::update(std::uint8_t const *data, std::size_t size) noexcept {
    if (_finalized) {
        return;
    }
    if (data == nullptr || size == 0) {
        return;
    }
    _bit_count += static_cast<std::uint64_t>(size) * 8u;
    std::size_t offset = 0;
    if (_buffer_len > 0) {
        std::size_t need = 64 - _buffer_len;
        std::size_t take = size < need ? size : need;
        std::memcpy(_buffer + _buffer_len, data, take);
        _buffer_len += take;
        offset += take;
        if (_buffer_len == 64) {
            transform(_buffer);
            _buffer_len = 0;
        }
    }
    while (offset + 64 <= size) {
        transform(data + offset);
        offset += 64;
    }
    if (offset < size) {
        std::memcpy(_buffer, data + offset, size - offset);
        _buffer_len = size - offset;
    }
}

void Sha256::update(std::string_view data) noexcept {
    update(reinterpret_cast<std::uint8_t const *>(data.data()), data.size());
}

void Sha256::transform(std::uint8_t const *block) noexcept {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24u) |
               (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16u) |
               (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8u) |
               (static_cast<std::uint32_t>(block[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        std::uint32_t s0 = rotr(w[i - 15], 7u) ^ rotr(w[i - 15], 18u) ^ (w[i - 15] >> 3u);
        std::uint32_t s1 = rotr(w[i - 2], 17u) ^ rotr(w[i - 2], 19u) ^ (w[i - 2] >> 10u);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = _state[0];
    std::uint32_t b = _state[1];
    std::uint32_t c = _state[2];
    std::uint32_t d = _state[3];
    std::uint32_t e = _state[4];
    std::uint32_t f = _state[5];
    std::uint32_t g = _state[6];
    std::uint32_t h = _state[7];
    for (int i = 0; i < 64; ++i) {
        std::uint32_t s1 = rotr(e, 6u) ^ rotr(e, 11u) ^ rotr(e, 25u);
        std::uint32_t ch = (e & f) ^ ((~e) & g);
        std::uint32_t temp1 = h + s1 + ch + kRoundConstants[i] + w[i];
        std::uint32_t s0 = rotr(a, 2u) ^ rotr(a, 13u) ^ rotr(a, 22u);
        std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        std::uint32_t temp2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    _state[0] += a;
    _state[1] += b;
    _state[2] += c;
    _state[3] += d;
    _state[4] += e;
    _state[5] += f;
    _state[6] += g;
    _state[7] += h;
}

std::array<std::uint8_t, 32> Sha256::raw() noexcept {
    if (!_finalized) {
        auto message_bits = _bit_count;
        // Append 0x80 padding, then zeros until the buffer is 56 bytes.
        std::uint8_t pad = 0x80;
        update(&pad, 1);
        std::uint8_t zero = 0;
        while (_buffer_len != 56) {
            update(&zero, 1);
        }
        // Append the original message length as a 64-bit big-endian bit count.
        std::uint8_t length_bytes[8];
        std::uint64_t bits = message_bits;
        for (int i = 7; i >= 0; --i) {
            length_bytes[i] = static_cast<std::uint8_t>(bits & 0xffu);
            bits >>= 8;
        }
        update(length_bytes, 8);
        _finalized = true;
    }
    std::array<std::uint8_t, 32> result{};
    for (int i = 0; i < 8; ++i) {
        result[i * 4] = static_cast<std::uint8_t>(_state[i] >> 24u);
        result[i * 4 + 1] = static_cast<std::uint8_t>(_state[i] >> 16u);
        result[i * 4 + 2] = static_cast<std::uint8_t>(_state[i] >> 8u);
        result[i * 4 + 3] = static_cast<std::uint8_t>(_state[i]);
    }
    return result;
}

std::string Sha256::hex() noexcept {
    auto digest = raw();
    std::string out;
    to_hex(digest.data(), digest.size(), out);
    return out;
}

std::string sha256_hex(std::string_view data) {
    Sha256 hasher;
    hasher.update(data);
    return hasher.hex();
}

std::string sha256_bytes(std::uint8_t const *data, std::size_t size) {
    Sha256 hasher;
    hasher.update(data, size);
    return hasher.hex();
}

std::string sha256_file(std::filesystem::path const &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Cannot read file for SHA-256: " + path.string());
    }
    Sha256 hasher;
    std::vector<char> buffer(1024 * 1024);
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        auto count = stream.gcount();
        if (count > 0) {
            hasher.update(reinterpret_cast<std::uint8_t const *>(buffer.data()), static_cast<std::size_t>(count));
        }
    }
    return hasher.hex();
}

} // namespace rbc_shader
