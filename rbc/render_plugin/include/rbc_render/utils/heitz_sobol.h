#pragma once
#include <luisa/runtime/command_list.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/buffer.h>
#include <luisa/vstl/common.h>
#include <rbc_graphics/dispose_queue.h>
#include <rbc_config.h>

enum struct HeitzSobolSPP {
    SPP1,
    SPP2,
    SPP4,
    SPP8,
    SPP16,
    SPP32,
    SPP64,
    SPP128,
    SPP256,
};
luisa::compute::Buffer<uint> heitz_sobol_256d(
    luisa::compute::Device &device,
    luisa::compute::CommandList &cmdlist,
    rbc::DisposeQueue &after_commit_dspqueue);

luisa::compute::Buffer<uint> heitz_sobol_scrambling(
    luisa::compute::Device &device,
    luisa::compute::CommandList &cmdlist,
    rbc::DisposeQueue &after_commit_dspqueue,
    HeitzSobolSPP spp);

luisa::compute::Buffer<uint> heitz_sobol_ranking(
    luisa::compute::Device &device,
    luisa::compute::CommandList &cmdlist,
    rbc::DisposeQueue &after_commit_dspqueue,
    HeitzSobolSPP spp);