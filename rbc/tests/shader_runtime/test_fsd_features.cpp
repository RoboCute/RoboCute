#include "test_util.h"

#include <fsd/constants.hpp>
#include <spectrum/spectrum_args.hpp>

    "sidecar radius uses the sampled wavelength upper bound"_test = [] {
        constexpr auto min_radius = fsd::query_radius_from_wavelength_nm(
            spectrum::wavelength_min);
        constexpr auto max_radius = fsd::query_radius_from_wavelength_nm(
            spectrum::wavelength_max);
        constexpr auto expected_max_radius_world_units = 6.225e-5f;

        expect(static_cast<bool>(max_radius > min_radius));
        expect(static_cast<bool>(max_radius == Approx(
                                expected_max_radius_world_units)));
        expect(static_cast<bool>(fsd::query_radius_wavelength_factor == 75.0f));
    };