#include <pybind11/pybind11.h>
#include "module_register.h"

namespace py = pybind11;

GuidData::GuidData(luisa::string_view str) {
    auto guid = vstd::Guid::TryParseGuid(str);
    if (guid) [[likely]] {
        new (std::launder(reinterpret_cast<vstd::Guid *>(this))) vstd::Guid(*guid);
    } else {
        LUISA_ERROR("Bad guid string {}, must be valid 22-byte base64 or 32-byte hex-digest.", str);
    }
}
void export_guid(py::module &m) {
    py::class_<GuidData>(m, "GUID")
        .def(py::init<>())
        .def(py::init<luisa::string>())
        .def(py::init<GuidData>())
        .def_static("new", [&]() {
            GuidData d;
            reinterpret_cast<vstd::Guid &>(d).remake();
            return d;
        })
        .def_readwrite("data_high", &GuidData::data0)
        .def_readwrite("data_low", &GuidData::data1)
        .def("__str__", [](GuidData &self) {
            auto str = reinterpret_cast<vstd::Guid &>(self).to_base64();
            return luisa::string(str.data(), str.size());
        })
        .def("to_string", [](GuidData &self) {
            auto str = reinterpret_cast<vstd::Guid &>(self).to_string();
            return luisa::string(str.data(), str.size());
        })
        .def("__repr__", [](GuidData &self) {
            auto str = reinterpret_cast<vstd::Guid &>(self).to_string();
            return luisa::string(str.data(), str.size());
        });
}
static ModuleRegister module_register_export_guid(export_guid);