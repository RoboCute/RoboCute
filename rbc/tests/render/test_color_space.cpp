#include "test_util.h"

#include <rbc_render/utils/color_space.h>

#include <array>
#include <cmath>

namespace {

using namespace luisa;
using namespace rbc;

void check_vector(double3 actual, double3 expected, double tolerance = 1e-9) {
    expect(static_cast<bool>(std::abs(actual.x - expected.x) < tolerance));
    expect(static_cast<bool>(std::abs(actual.y - expected.y) < tolerance));
    expect(static_cast<bool>(std::abs(actual.z - expected.z) < tolerance));
}

void check_matrix_rows(
    const double3x3 &actual,
    const std::array<double, 9u> &expected,
    double tolerance = 1e-9) {
    for (auto row = 0u; row < 3u; row++) {
        for (auto column = 0u; column < 3u; column++) {
            expect(static_cast<bool>(std::abs(actual.cols[column][row] - expected[row * 3u + column]) < tolerance));
        }
    }
}

}// namespace


    "standard RGB-to-XYZ matrices use column vectors"_test = [] {
        const ColorSpace rec2020{EColorSpace::Rec2020};
        check_matrix_rows(
            rec2020.to_xyz,
            {
                0.6369580483012914, 0.14461690358620832, 0.1688809751641721,
                0.2627002120112671, 0.6779980715188708, 0.05930171646986196,
                0.0, 0.028072693049087428, 1.060985057710791,
            });

        const ColorSpace ap0{EColorSpace::ACES_AP0};
        check_matrix_rows(
            ap0.to_xyz,
            {
                0.9525523959, 0.0, 0.0000936786,
                0.3439664498, 0.7281660966, -0.0721325464,
                0.0, 0.0, 1.0088251844,
            },
            1e-8);
    };

    "AP0 primaries can use a D65 neutral white"_test = [] {
        const Chromaticities ap0{EColorSpace::ACES_AP0};
        const auto d65 = Chromaticities::get_white_point(EWhitePoint::CIE1931_D65);
        const ColorSpace ap0_d65{ap0.red, ap0.green, ap0.blue, d65};

        const auto d65_xyz = Chromaticities::xyY_to_XYZ(make_double3(d65, 1.0));
        check_vector(ap0_d65.color_to_XYZ(double3{1.0}), d65_xyz, 1e-12);
        check_vector(ap0_d65.color_from_XYZ(d65_xyz), double3{1.0}, 1e-12);

        check_matrix_rows(
            ap0_d65.from_xyz,
            {
                1.052238599, 0.0, -0.000097709964,
                -0.491495220, 1.36110644, 0.0973668356,
                0.0, 0.0, 0.918224951,
            },
            1e-8);
    };

    "RGB and XYZ conversions round-trip"_test = [] {
        const Chromaticities ap0{EColorSpace::ACES_AP0};
        const ColorSpace spaces[]{
            ColorSpace{EColorSpace::Rec2020},
            ColorSpace{EColorSpace::ACES_AP0},
            ColorSpace{
                ap0.red,
                ap0.green,
                ap0.blue,
                Chromaticities::get_white_point(EWhitePoint::CIE1931_D65)},
        };
        const auto color = double3{0.125, 0.5, 1.75};

        for (auto &&space : spaces) {
            check_vector(space.color_from_XYZ(space.color_to_XYZ(color)), color, 1e-12);
        }
    };

    "color-space conversion preserves the selected white"_test = [] {
        const ColorSpace rec2020{EColorSpace::Rec2020};
        const ColorSpace ap0{EColorSpace::ACES_AP0};
        const Chromaticities ap0_chromaticities{EColorSpace::ACES_AP0};
        const ColorSpace ap0_d65{
            ap0_chromaticities.red,
            ap0_chromaticities.green,
            ap0_chromaticities.blue,
            Chromaticities::get_white_point(EWhitePoint::CIE1931_D65)};

        const auto ap0_to_rec2020 = ColorSpace::convert_matrix(ap0, rec2020);
        const auto rec2020_to_ap0 = ColorSpace::convert_matrix(rec2020, ap0);
        check_vector(ap0_to_rec2020 * double3{1.0}, double3{1.0}, 1e-10);
        check_vector(rec2020_to_ap0 * double3{1.0}, double3{1.0}, 1e-10);

        const auto color = double3{0.125, 0.5, 1.75};
        check_vector(rec2020_to_ap0 * (ap0_to_rec2020 * color), color, 1e-10);

        const auto rec2020_to_ap0_d65 = ColorSpace::convert_matrix(rec2020, ap0_d65);
        const auto expected_ap0_d65 = ap0_d65.color_from_XYZ(rec2020.color_to_XYZ(color));
        check_vector(rec2020_to_ap0_d65 * color, expected_ap0_d65, 1e-12);
    };