#pragma once
#include <luisa/std.hpp>
#include <bsdfs/base/bsdf_flags.hpp>

namespace mtl {

using namespace luisa::shader;

struct Throughput {
	float3 val = 0.0f;
	BSDFFlags flags = BSDFFlags::None;

	Throughput() = default;

	operator bool() const {
		return flags != BSDFFlags::None;
	}

	void operator+=(Throughput other) {
		val += other.val;
		flags |= other.flags;
	}

	void operator*=(Throughput other) {
		val *= other.val;
		flags = BSDFFlags(flags & other.flags);
	}

	void operator*=(float3 other) {
		val *= other;
		flags = BSDFFlags(flags & (all(other == 0.0f) ? BSDFFlags::None : BSDFFlags::All));
	}

	void operator*=(float other) {
		val *= other;
		flags = BSDFFlags(flags & (other == 0.0f ? BSDFFlags::None : BSDFFlags::All));
	}

	Throughput operator+(Throughput other) const {
		Throughput copy{*this};
		copy += other;
		return copy;
	}

	Throughput operator*(Throughput other) const {
		Throughput copy{*this};
		copy *= other;
		return copy;
	}

	Throughput operator*(float3 other) const {
		Throughput copy{*this};
		copy *= other;
		return copy;
	}

	Throughput operator*(float other) const {
		Throughput copy{*this};
		copy *= other;
		return copy;
	}
};

}// namespace mtl