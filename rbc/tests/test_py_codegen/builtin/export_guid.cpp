#include <nanobind/nanobind.h>
#include "module_register.h"

namespace nb = nanobind;

GuidData::GuidData(nanobind::str const &str) {
    auto guid = vstd::Guid::TryParseGuid(str.c_str());
    if (guid) [[likely]] {
        new (std::launder(reinterpret_cast<vstd::Guid *>(this))) vstd::Guid(*guid);
    } else {
        LUISA_ERROR("Bad guid string {}, must be valid 22-byte base64 or 32-byte hex-digest.", str.c_str());
    }
}
void export_guid(nb::module_ &m) {
    nb::class_<GuidData>(m, "GUID")
        .def(nb::init<>())
        .def(nb::init<nb::str>())
        .def(nb::init<GuidData>())
        .def_static("new", [&]() {
            GuidData d;
            reinterpret_cast<vstd::Guid &>(d).remake();
            return d;
        })
        .def_rw("data_high", &GuidData::data0)
        .def_rw("data_low", &GuidData::data1)
        .def("__str__", [](GuidData &self) {
            auto str = reinterpret_cast<vstd::Guid &>(self).to_base64();
            return nb::str(str.data(), str.size());
        })
        .def("to_string", [](GuidData &self) {
            auto str = reinterpret_cast<vstd::Guid &>(self).to_string();
            return nb::str(str.data(), str.size());
        })
        .def("__repr__", [](GuidData &self) {
            auto str = reinterpret_cast<vstd::Guid &>(self).to_string();
            return nb::str(str.data(), str.size());
        });
}
static ModuleRegister _module_register(export_guid);