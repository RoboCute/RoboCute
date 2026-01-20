#include "py_stream.h"
#include <luisa/runtime/command_list.h>
#include <luisa/core/logging.h>
#include <rbc_graphics/render_device.h>
namespace luisa::compute {

PyStream::PyStream(Device &device, bool support_window) noexcept
    : _data{luisa::make_shared<Data>(device, support_window)} {}

PyStream::Data::Data(Device &device, bool support_window) noexcept {
}

PyStream::~PyStream() noexcept {
    auto rb = rbc::RenderDevice::instance_ptr();
    if (!rb) [[unlikely]] {
        return;
    }
    if (!_data) return;
    execute();
    rb->lc_main_stream().synchronize();
}

void PyStream::add(Command *cmd) noexcept {
    auto rb = rbc::RenderDevice::instance_ptr();
    if (!rb) [[unlikely]] {
        LUISA_ERROR("RenderDevice not initialized. call init_device() first.");
    }
    rb->lc_main_cmd_list() << luisa::unique_ptr<Command>(cmd);
}

void PyStream::add(luisa::unique_ptr<Command> &&cmd) noexcept {
    auto rb = rbc::RenderDevice::instance_ptr();
    if (!rb) [[unlikely]] {
        LUISA_ERROR("RenderDevice not initialized. call init_device() first.");
    }
    rb->lc_main_cmd_list() << std::move(cmd);
}
void PyStream::Data::sync() noexcept {
    auto rb = rbc::RenderDevice::instance_ptr();
    if (!rb) [[unlikely]] {
        LUISA_ERROR("RenderDevice not initialized. call init_device() first.");
    }
    auto &&stream = rb->lc_main_stream();
    auto &&buffer = rb->lc_main_cmd_list();
    stream << buffer.commit() << synchronize();
    uploadDisposer.clear();
}

void PyStream::execute() noexcept {
    auto rb = rbc::RenderDevice::instance_ptr();
    if (!rb) [[unlikely]] {
        LUISA_ERROR("RenderDevice not initialized. call init_device() first.");
    }
    auto &&stream = rb->lc_main_stream();
    if (!delegates.empty()) {
        rb->lc_main_cmd_list().add_callback([delegates = std::move(delegates)] {
            for (auto &&i : delegates) { i(); }
        });
    }
    stream << rb->lc_main_cmd_list().commit();
    _data->uploadDisposer.clear();
}

void PyStream::sync() noexcept {
    auto rb = rbc::RenderDevice::instance_ptr();
    if (!rb) [[unlikely]] {
        LUISA_ERROR("RenderDevice not initialized. call init_device() first.");
    }
    auto &&stream = rb->lc_main_stream();
    execute();
    stream.synchronize();
}

PyStream::PyStream(PyStream &&s) noexcept
    : _data(std::move(s._data)) {}

}// namespace luisa::compute
