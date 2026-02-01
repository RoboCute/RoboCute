// This file exports LuisaCompute functionalities to a python library using pybind11.
//
// Class:
//   FunctionBuilder
//       define_kernel

#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <luisa/core/logging.h>
#include "ref_counter.h"
#include <luisa/runtime/raster/raster_state.h>
#include "module_register.h"
namespace py = pybind11;
using namespace luisa;
using namespace luisa::compute;
using AccelModification = AccelBuildCommand::Modification;

constexpr auto pyref = py::return_value_policy::reference;// object lifetime is managed on C++ side
// Note: declare pointer & base class;
// use reference policy when python shouldn't destroy returned object
void export_lcapi(py::module &m) {
    m.doc() = "LuisaCompute API";// optional module docstring

    // log
    m.def("set_log_callback", [](const luisa::function<void(const char *level, const char *message)> &callback) {
        luisa::detail::default_logger_set_sink(luisa::detail::create_sink_with_callback(callback));
    });
    m.def("log_verbose", [](const char *msg) { LUISA_VERBOSE("{}", msg); });
    m.def("log_info", [](const char *msg) { LUISA_INFO("{}", msg); });
    m.def("log_warning", [](const char *msg) { LUISA_WARNING("{}", msg); });
    m.def("log_error", [](const char *msg) { LUISA_ERROR("{}", msg); });
    m.def("log_level_verbose", luisa::log_level_verbose);
    m.def("log_level_info", luisa::log_level_info);
    m.def("log_level_warning", luisa::log_level_warning);
    m.def("log_level_error", luisa::log_level_error);
    py::enum_<AccelOption::UsageHint>(m, "AccelUsageHint")
        .value("FAST_TRACE", AccelOption::UsageHint::FAST_TRACE)
        .value("FAST_BUILD", AccelOption::UsageHint::FAST_BUILD);

    py::enum_<AccelBuildRequest>(m, "AccelBuildRequest")
        .value("PREFER_UPDATE", AccelBuildRequest::PREFER_UPDATE)
        .value("FORCE_BUILD", AccelBuildRequest::FORCE_BUILD);

    py::class_<AccelModification>(m, "AccelModification")
        .def("set_transform", &AccelModification::set_transform)
        .def("set_visibility", &AccelModification::set_visibility)
        .def("set_mesh", &AccelModification::set_primitive);

    // pixel
    py::enum_<PixelFormat>(m, "PixelFormat")
        .value("R8SInt", PixelFormat::R8SInt)
        .value("R8UInt", PixelFormat::R8UInt)
        .value("R8UNorm", PixelFormat::R8UNorm)
        .value("RG8SInt", PixelFormat::RG8SInt)
        .value("RG8UInt", PixelFormat::RG8UInt)
        .value("RG8UNorm", PixelFormat::RG8UNorm)
        .value("RGBA8SInt", PixelFormat::RGBA8SInt)
        .value("RGBA8UInt", PixelFormat::RGBA8UInt)
        .value("RGBA8UNorm", PixelFormat::RGBA8UNorm)
        .value("R16SInt", PixelFormat::R16SInt)
        .value("R16UInt", PixelFormat::R16UInt)
        .value("R16UNorm", PixelFormat::R16UNorm)
        .value("RG16SInt", PixelFormat::RG16SInt)
        .value("RG16UInt", PixelFormat::RG16UInt)
        .value("RG16UNorm", PixelFormat::RG16UNorm)
        .value("RGBA16SInt", PixelFormat::RGBA16SInt)
        .value("RGBA16UInt", PixelFormat::RGBA16UInt)
        .value("RGBA16UNorm", PixelFormat::RGBA16UNorm)
        .value("R32SInt", PixelFormat::R32SInt)
        .value("R32UInt", PixelFormat::R32UInt)
        .value("RG32SInt", PixelFormat::RG32SInt)
        .value("RG32UInt", PixelFormat::RG32UInt)
        .value("RGBA32SInt", PixelFormat::RGBA32SInt)
        .value("RGBA32UInt", PixelFormat::RGBA32UInt)
        .value("R16F", PixelFormat::R16F)
        .value("RG16F", PixelFormat::RG16F)
        .value("RGBA16F", PixelFormat::RGBA16F)
        .value("R32F", PixelFormat::R32F)
        .value("RG32F", PixelFormat::RG32F)
        .value("RGBA32F", PixelFormat::RGBA32F)
        .value("BC4UNorm", PixelFormat::BC4UNorm)
        .value("BC5UNorm", PixelFormat::BC5UNorm)
        .value("BC6HUF16", PixelFormat::BC6HUF16)
        .value("BC7UNorm", PixelFormat::BC7UNorm);

    py::enum_<PixelStorage>(m, "PixelStorage")
        .value("BYTE1", PixelStorage::BYTE1)
        .value("BYTE2", PixelStorage::BYTE2)
        .value("BYTE4", PixelStorage::BYTE4)
        .value("SHORT1", PixelStorage::SHORT1)
        .value("SHORT2", PixelStorage::SHORT2)
        .value("SHORT4", PixelStorage::SHORT4)
        .value("INT1", PixelStorage::INT1)
        .value("INT2", PixelStorage::INT2)
        .value("INT4", PixelStorage::INT4)
        .value("HALF1", PixelStorage::HALF1)
        .value("HALF2", PixelStorage::HALF2)
        .value("HALF4", PixelStorage::HALF4)
        .value("FLOAT1", PixelStorage::FLOAT1)
        .value("FLOAT2", PixelStorage::FLOAT2)
        .value("FLOAT4", PixelStorage::FLOAT4)
        .value("BC4", PixelStorage::BC4)
        .value("BC5", PixelStorage::BC5)
        .value("BC6", PixelStorage::BC6)
        .value("BC7", PixelStorage::BC7);

    m.def("pixel_storage_channel_count", pixel_storage_channel_count);
    m.def("pixel_storage_to_format_uint", pixel_storage_to_format<uint>);
    m.def("pixel_storage_to_format_int", pixel_storage_to_format<int>);
    m.def("pixel_storage_to_format_float", pixel_storage_to_format<float>);
    m.def("pixel_storage_size", [](PixelStorage storage, uint32_t w, uint32_t h, uint32_t d) { return pixel_storage_size(storage, make_uint3(w, h, d)); });

    // sampler
    auto m_sampler = py::class_<Sampler>(m, "Sampler")
                         .def(py::init<Sampler::Filter, Sampler::Address>());

    py::enum_<Sampler::Filter>(m, "Filter")
        .value("POINT", Sampler::Filter::POINT)
        .value("LINEAR_POINT", Sampler::Filter::LINEAR_POINT)
        .value("LINEAR_LINEAR", Sampler::Filter::LINEAR_LINEAR)
        .value("ANISOTROPIC", Sampler::Filter::ANISOTROPIC);

    py::enum_<Sampler::Address>(m, "Address")
        .value("EDGE", Sampler::Address::EDGE)
        .value("REPEAT", Sampler::Address::REPEAT)
        .value("MIRROR", Sampler::Address::MIRROR)
        .value("ZERO", Sampler::Address::ZERO);
    py::enum_<VertexAttributeType>(m, "VertexAttributeType")
        .value("Position", VertexAttributeType::Position)
        .value("Normal", VertexAttributeType::Normal)
        .value("Tangent", VertexAttributeType::Tangent)
        .value("Color", VertexAttributeType::Color)
        .value("UV0", VertexAttributeType::UV0)
        .value("UV1", VertexAttributeType::UV1)
        .value("UV2", VertexAttributeType::UV2)
        .value("UV3", VertexAttributeType::UV3);
}

static ModuleRegister module_register_export_lcapi(export_lcapi);