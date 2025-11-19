#pragma once
#include <luisa/vstl/v_guid.h>
#include <nanobind/nanobind.h>
struct GuidData : public vstd::IOperatorNewBase {
    uint64_t data0;
    uint64_t data1;
    GuidData() : data0(0), data1(0) {}
    GuidData(nanobind::str const &str);
    operator vstd::Guid &() {
        return *reinterpret_cast<vstd::Guid *>(this);
    }
    operator vstd::Guid const &() const {
        return *reinterpret_cast<vstd::Guid const *>(this);
    }
    GuidData(GuidData const &) = default;
};