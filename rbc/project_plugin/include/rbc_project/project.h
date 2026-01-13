#pragma once
#include <rbc_config.h>
#include <rbc_core/rc.h>
#include <luisa/core/stl/filesystem.h>
namespace rbc {
struct Resource;
struct IProject : RCBase {
    IProject() = default;
    virtual void scan_project() = 0;
    virtual ~IProject() = default;
};
}// namespace rbc::project