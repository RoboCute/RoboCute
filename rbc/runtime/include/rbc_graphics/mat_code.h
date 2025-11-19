#pragma once
#include <luisa/core/intrin.h>
#include <luisa/core/mathematics.h>
namespace rbc {
struct MatCode {
	uint32_t value;
	uint32_t get_type() const {
		return value >> 24u;
	}
	uint32_t get_inst_id() const {
		return value & ((1u << 24u) - 1u);
	}
	MatCode() : value(std::numeric_limits<uint32_t>::max()) {}
	explicit MatCode(uint32_t value) : value(value) {}
	MatCode(uint32_t mat_type, uint32_t mat_inst_id) {
		LUISA_ASSUME(mat_type < 256);
		LUISA_ASSUME(mat_inst_id < ((1u << 24u) - 1u));
		value = mat_type;
		value <<= 24u;
		value |= mat_inst_id;
	}
	bool operator==(rbc::MatCode const& rhs) const {
		return value == rhs.value;
	}
};
}// namespace rbc

namespace std {
template<>
struct hash<rbc::MatCode> {
	size_t operator()(const rbc::MatCode& code) const noexcept {
		return code.value;
	}
};

template<>
struct equal_to<rbc::MatCode> {
	bool operator()(const rbc::MatCode& lhs, const rbc::MatCode& rhs) const noexcept {
		return lhs.value == rhs.value;
	}
};
}// namespace std