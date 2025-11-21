#pragma once
#include <rbc_render/pass.h>
#include <luisa/vstl/lmdb.hpp>
#include <luisa/runtime/shader.h>
#include <rbc_render/generated/pipeline_settings.hpp>
namespace post_uber_pass {
using namespace luisa;
template<typename T, size_t i>
using Array = std::array<T, i>;
#include <post_process/uber_arg.inl>
}// namespace post_uber_pass
namespace rbc {
using namespace luisa;
using namespace luisa::compute;
struct LPM {
	static post_uber_pass::LpmArgs compute(LpmDispatchParameters const& d);
};
}// namespace rbc