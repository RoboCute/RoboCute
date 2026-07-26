#include "test_util.h"

#include <fsd/constants.hpp>
#include <spectrum/spectrum_args.hpp>

TEST_SUITE("free-space diffraction features") {
    TEST_CASE("sidecar radius uses the sampled wavelength upper bound") {
        constexpr auto min_radius = fsd::query_radius_from_wavelength_nm(
            spectrum::wavelength_min);
        constexpr auto max_radius = fsd::query_radius_from_wavelength_nm(
            spectrum::wavelength_max);
        constexpr auto expected_max_radius_world_units = 6.225e-5f;

        CHECK(max_radius > min_radius);
        CHECK(max_radius == doctest::Approx(
                                expected_max_radius_world_units));
        CHECK(fsd::query_radius_wavelength_factor == 75.0f);
    }
}
