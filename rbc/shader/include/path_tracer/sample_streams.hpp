#pragma once

#include <luisa/resources/common_extern.hpp>
#include <sampling/heitz_sobol.hpp>
#include <sampling/pcg.hpp>

using namespace luisa::shader;

namespace pt {

struct PrimarySampleStreams {
    sampling::HeitzSobol trace;
    sampling::PCGSamplerOffsetted surface;

    PrimarySampleStreams(uint2 coord, uint frame_index)
        : trace(coord, frame_index),
          surface(uint3(coord, frame_index)) {}

    float next_lobe() {
        return trace.next(g_buffer_heap);
    }
};

struct ContinuationSampleStreams {
    sampling::PCGSampler path;
    sampling::PCGSampler lobe;

    ContinuationSampleStreams(
        uint path_id,
        uint lobe_id,
        uint frame_index)
        : path(uint2(path_id, frame_index)),
          lobe(uint2(lobe_id, frame_index)) {}

    float next_lobe(bool evaluate_bsdf) {
        return evaluate_bsdf ? lobe.next() : 0.0f;
    }
};

}// namespace pt
