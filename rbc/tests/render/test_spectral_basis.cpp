#include "rbc_test.hpp"

#include <rbc_render/utils/color_space.h>

#include "../../render_plugin/src/spectrum_data.h"

#include <array>
#include <cmath>

namespace rbc::test {
namespace {

using namespace luisa;
using namespace rbc;

constexpr size_t wavelength_count = 471u;

void check_vector(double3 actual, double3 expected, double tolerance) {
    expect(static_cast<bool>(std::abs(actual.x - expected.x) < tolerance));
    expect(static_cast<bool>(std::abs(actual.y - expected.y) < tolerance));
    expect(static_cast<bool>(std::abs(actual.z - expected.z) < tolerance));
}

}// namespace


suite<"Render|SpectralBasis"> RenderSpectralBasisTestSuite = [] {

    "AP0D65 responses are nonnegative and consistently normalized"_test = [] {
        std::array<double3, wavelength_count> cmf{};
        std::array<double, wavelength_count> wavelengths{};
        double3 cmf_integral{0.0};
        double3 d65_white{0.0};
        const auto d65_start = luisa::test::spectrum::CIE_std_illum_D65[0].first;

        for (size_t i = 0; i < wavelength_count; ++i) {
            const auto &sample = luisa::test::spectrum::CIE_xyz_1931_2deg[i];
            wavelengths[i] = double(sample.first);
            cmf[i] = make_double3(sample.second);
        }
        for (size_t i = 1; i < wavelength_count; ++i) {
            const auto dx = wavelengths[i] - wavelengths[i - 1u];
            cmf_integral += (cmf[i - 1u] + cmf[i]) * (0.5 * dx);
            const auto d65_index0 = size_t(wavelengths[i - 1u]) - size_t(d65_start);
            const auto d65_index1 = size_t(wavelengths[i]) - size_t(d65_start);
            const auto e0 = double(
                luisa::test::spectrum::CIE_std_illum_D65[d65_index0].second);
            const auto e1 = double(
                luisa::test::spectrum::CIE_std_illum_D65[d65_index1].second);
            d65_white += (cmf[i - 1u] * e0 + cmf[i] * e1) * (0.5 * dx);
        }
        d65_white /= d65_white.y;
        const auto d65_xyY = Chromaticities::XYZ_to_xyY(d65_white);
        const auto d65_xy = make_double2(d65_xyY.x, d65_xyY.y);

        const Chromaticities ap0_chromaticities{EColorSpace::ACES_AP0};
        const ColorSpace ap0_d65{
            ap0_chromaticities.red,
            ap0_chromaticities.green,
            ap0_chromaticities.blue,
            d65_xy};

        std::array<double3, wavelength_count> responses{};
        double3 response_integral{0.0};
        for (size_t i = 0; i < wavelength_count; ++i) {
            responses[i] = ap0_d65.color_from_XYZ(cmf[i]);
            expect(static_cast<bool>(responses[i].x >= -1e-9));
            expect(static_cast<bool>(responses[i].y >= -1e-9));
            expect(static_cast<bool>(responses[i].z >= -1e-9));
        }
        for (size_t i = 1; i < wavelength_count; ++i) {
            const auto dx = wavelengths[i] - wavelengths[i - 1u];
            response_integral += (responses[i - 1u] + responses[i]) * (0.5 * dx);
        }

        check_vector(
            response_integral,
            double3{112.435757980, 103.330576660, 98.171617720},
            1e-5);
        check_vector(
            response_integral / cmf_integral.y,
            double3{1.052208540, 0.966999440, 0.918720310},
            1e-7);
        check_vector(ap0_d65.color_from_XYZ(d65_white), double3{1.0}, 1e-10);

        for (size_t channel = 0; channel < 3u; ++channel) {
            double pdf_integral = 0.0;
            for (size_t i = 1; i < wavelength_count; ++i) {
                const auto dx = wavelengths[i] - wavelengths[i - 1u];
                pdf_integral +=
                    (responses[i - 1u][channel] + responses[i][channel]) *
                    (0.5 * dx / response_integral[channel]);
            }
            expect(static_cast<bool>(std::abs(pdf_integral - 1.0) < 1e-12));
        }

        const Chromaticities rec2020_chromaticities{EColorSpace::Rec2020};
        const ColorSpace rec2020{
            rec2020_chromaticities.red,
            rec2020_chromaticities.green,
            rec2020_chromaticities.blue,
            d65_xy};
        const auto rec2020_to_ap0d65 = ap0_d65.from_xyz * rec2020.to_xyz;
        const auto ap0d65_to_rec2020 = rec2020.from_xyz * ap0_d65.to_xyz;
        const auto round_trip = ap0d65_to_rec2020 * rec2020_to_ap0d65;
        for (size_t column = 0; column < 3u; ++column) {
            for (size_t row = 0; row < 3u; ++row) {
                const auto expected = column == row ? 1.0 : 0.0;
                expect(static_cast<bool>(std::abs(round_trip.cols[column][row] - expected) < 1e-10));
            }
        }
    };
}; // suite

} // namespace rbc::test
