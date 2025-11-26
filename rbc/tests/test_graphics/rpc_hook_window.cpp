#pragma once
#include "generated/generated.hpp"
#include <luisa/runtime/device.h>
#include <rbc_runtime/render_plugin.h>
#include <rbc_core/rpc/command_list.h>
#include "rpc_hook_window.h"
#include <luisa/core/logging.h>

namespace rbc {
using namespace luisa;
using namespace luisa::compute;
SharedWindow::SharedWindow() {}
SharedWindow::~SharedWindow() {}
void SharedWindow::open_window(uint64_t window_handle) {
    LUISA_INFO("Open window {}", window_handle);
    swapchains.try_emplace(
        window_handle,
        vstd::lazy_eval([&] {
            return device->create_swapchain(
                *stream,
                SwapchainOption{
                    .display = window_handle,
                    .window = window_handle,
                    .size = make_uint2(1024),
                    .wants_hdr = false,
                    .wants_vsync = false,
                    .back_buffer_count = 2});
        }));
}
void SharedWindow::close_window(uint64_t window_handle) {
    if (swapchains.erase(window_handle)) {
        dirty = true;
    }
}
RPCHook::RPCHook(RenderDevice &render_device, Stream &present_stream)
    : thd([this] {
          _thread();
      }) {
    shared_window.device = &render_device.lc_device();
    shared_window.stream = &present_stream;
}
RPCHook::~RPCHook() {
    enabled = false;
    thd.join();
}
bool RPCHook::update() {
    std::lock_guard lck{mtx};
    shared_window.dirty = false;
    if (!execute_blob.empty()) {
        return_blob = RPCCommandList::server_execute(
            ~0ull,
            std::move(execute_blob));
        LUISA_INFO("return value {}", luisa::string_view{
                                          (char const *)return_blob.data(),
                                          return_blob.size()});
        execute_blob = {};
    }
    return shared_window.dirty;
}

void RPCHook::shutdown_remote() {
    std::lock_guard lck{mtx};
    const auto size = sizeof(ipc::TypedHeader) + sizeof(uint);
    return_blob = luisa::BinaryBlob{
        (std::byte *)vengine_malloc(size),
        size,
        [](void *ptr) { vengine_free(ptr); }};
    auto header_ptr = (ipc::TypedHeader *)return_blob.data();
    auto pass_word_ptr = (uint32_t *)(header_ptr + 1);
    header_ptr->custom_id = 255;
    header_ptr->size = sizeof(uint);
    *pass_word_ptr = 1919810;
}

void RPCHook::_thread() {
    receiver = ipc::IMessageReceiver::create(
        "test_window_hook",
        ipc::EMessageQueueBackend::IPC);
    sender = ipc::IMessageSender::create(
        "test_graphics",
        ipc::EMessageQueueBackend::IPC);
    ipc::AsyncTypedMessagePop poper;
    poper.reset(receiver.get());
    while (enabled) {
        if (poper.next_step()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
            auto id = poper.id();
            auto data = poper.steal_data();
            poper.reset(receiver.get());
            if (id == 255) {
                if (data.size() != sizeof(uint32_t) || (*(uint32_t *)data.data()) != 1919810) {
                    LUISA_ERROR("Password error.");
                }
                enabled = false;
                LUISA_INFO("Shotdown by remote.");
                return;
            }
            if (id == 0) {
                // input password
                if (data.size() != sizeof(uint32_t) || (*(uint32_t *)data.data()) != 114514) {
                    LUISA_ERROR("Password error.");
                }
                auto self_handle = &shared_window;
                LUISA_DEBUG_ASSERT(
                    sender->push(
                        {reinterpret_cast<std::byte *>(&self_handle),
                         sizeof(self_handle)}));

            } else {
                LUISA_INFO("Get RPC command {}",
                           luisa::string_view{
                               (char const *)data.data(),
                               data.size()});
                while (true) {
                    std::unique_lock lck{mtx};
                    // run
                    if (execute_blob.empty()) {
                        execute_blob = luisa::BinaryBlob{
                            data.data(),
                            data.size(),
                            [d = std::move(data)](void *) {}};
                        break;
                    }
                    lck.unlock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }
        {
            std::lock_guard lck{mtx};
            if (!return_blob.empty()) {
                LUISA_INFO("Sending return value {}",
                           luisa::string_view{
                               (char const *)return_blob.data(),
                               return_blob.size()});
                LUISA_DEBUG_ASSERT(sender->push(return_blob));
            }
            return_blob = {};
        }
    }
}
}// namespace rbc