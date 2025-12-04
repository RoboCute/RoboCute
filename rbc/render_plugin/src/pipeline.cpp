#include <rbc_render/pipeline.h>
#include <luisa/core/logging.h>
#include <luisa/core/binary_file_stream.h>
#include <rbc_io/binary_file_writer.h>
#include <rbc_graphics/scene_manager.h>
#include <luisa/vstl/v_guid.h>

namespace rbc {
Pipeline::Pipeline() = default;

Pipeline::~Pipeline() {
    wait_enable();
}

Pass *Pipeline::_emplace_instance(vstd::unique_ptr<Pass> &&component, TypeInfo const &name, bool init_enabled) {
    auto ptr = component.get();
    ptr->_actived = true;
    auto iter = _pass_indices.try_emplace(
        std::move(name), vstd::lazy_eval([&]() {
            auto idx = _passes.size();
            auto &v = _passes.emplace_back(std::move(component));
            return idx;
        }));
    if (!iter.second) [[unlikely]] {
        vstd::Guid guid(false);
        std::memcpy(&guid, &iter.first.key(), sizeof(vstd::Guid));
        LUISA_ERROR("Component {} has been emplaced twice.", guid.to_string());
    }
    return ptr;
}

Pass *Pipeline::_get_pass(TypeInfo const &name) const {
    auto iter = _pass_indices.find(name);
    if (!iter) return nullptr;
    auto &ids = iter.value();
    return _passes[ids].get();
}

void Pipeline::enable(
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
    for (auto &i : _passes) {
        i->on_enable(
            *this,
            device,
            cmdlist,
            scene);
    }
}

void Pipeline::wait_enable() {
    for (auto &i : _passes) {
        i->wait_enable();
    }
}

void Pipeline::update(PipelineContext &ctx) {
    for (auto &i : _enabled_passes) {
        i->update(*this, ctx);
    }
    for (auto &i : _enabled_passes) {
        i->on_frame_end(*this, *ctx.device, *ctx.scene);
    }
    _enabled_passes.clear();
}

void Pipeline::early_update(PipelineContext &ctx) {
    _enabled_passes.clear();
    _enabled_passes.reserve(_passes.size());
    for (auto &i : _passes) {

        if (i->_actived) {
            _enabled_passes.emplace_back(i.get());
            i->early_update(*this, ctx);
        }
    }
}

void Pipeline::disable(
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
    for (auto &i : _passes) {
        i->on_disable(*this, device, cmdlist, scene);
    }
}
}// namespace rbc