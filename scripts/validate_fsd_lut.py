"""Audit the packed FSD inverse-CDF tables against the shader proposal.

This validator deliberately derives its target density from the formulas in
``free_space_diffraction.hpp`` instead of treating the reference tables as
ground truth. It checks the binary format, inverse-CDF invariants, numerical
normalization, and sampled radial/angular distributions.
"""

from __future__ import annotations

import argparse
import math
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
from numpy.typing import NDArray

from generate_fsd_tables import (
    CENTRAL_EXPONENT_DENOMINATOR,
    GENERATOR_VERSION,
    HEADER,
    TABLE_MAGIC,
    TABLE_RESOLUTION,
    TABLE_VERSION,
    alpha_lobe_densities,
)


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TABLE_PATH = (
    ROOT / "build" / "download" / "render_resources" / "fsd_tables.bytes"
)
SHADER_HEADER_PATH = (
    ROOT / "rbc" / "shader" / "include" / "bsdfs" / "mono"
    / "free_space_diffraction.hpp"
)

FLOAT_BYTES = 4

SHADER_INTEGRAL_NAMES = (
    "alpha1_removed_central_power_integral",
    "alpha2_removed_central_power_integral",
)

RADIAL_THRESHOLDS = np.asarray(
    (1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0),
    dtype=np.float64,
)
ANGULAR_THRESHOLDS = np.radians(
    np.asarray((15.0, 30.0, 45.0, 60.0, 75.0), dtype=np.float64)
)

INTEGRATION_CHUNK_SIZE = 250_000
SEMI_ANALYTIC_QUADRATURE_ORDER = 64
SEMI_ANALYTIC_QUADRATURE_PERIODS = 4096
SEMI_ANALYTIC_QUADRATURE_BLOCK_PERIODS = 128


FloatArray = NDArray[np.float64]


def _load_shader_integrals(path: Path = SHADER_HEADER_PATH) -> tuple[float, float]:
    values: dict[str, float] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        for name in SHADER_INTEGRAL_NAMES:
            for qualifier in ("constexpr", "static constexpr"):
                prefix = f"{qualifier} float {name} = "
                if stripped.startswith(prefix) and stripped.endswith("f;"):
                    values[name] = float(stripped[len(prefix) : -2])
    missing = [name for name in SHADER_INTEGRAL_NAMES if name not in values]
    if missing:
        raise ValueError(
            f"missing FSD integral constants in {path}: {', '.join(missing)}"
        )
    return (
        values[SHADER_INTEGRAL_NAMES[0]],
        values[SHADER_INTEGRAL_NAMES[1]],
    )


@dataclass(frozen=True)
class Tables:
    resolution: int
    alpha1_theta: FloatArray
    alpha1_radial: FloatArray
    alpha2_theta: FloatArray
    alpha2_radial: FloatArray


@dataclass
class Audit:
    failures: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)

    def check(self, condition: bool, name: str, detail: str) -> None:
        status = "PASS" if condition else "FAIL"
        print(f"[{status}] {name}: {detail}")
        if not condition:
            self.failures.append(name)

    def warn_if_false(self, condition: bool, name: str, detail: str) -> None:
        status = "PASS" if condition else "WARN"
        print(f"[{status}] {name}: {detail}")
        if not condition:
            self.warnings.append(name)


@dataclass(frozen=True)
class IntegrationEstimate:
    integral_replicates: FloatArray
    radial_cdf_replicates: FloatArray
    angular_cdf_replicates: FloatArray

    @property
    def integral_mean(self) -> FloatArray:
        return self.integral_replicates.mean(axis=0)

    @property
    def integral_standard_error(self) -> FloatArray:
        return _replicate_standard_error(self.integral_replicates)

    @property
    def radial_cdf_mean(self) -> FloatArray:
        return self.radial_cdf_replicates.mean(axis=0)

    @property
    def radial_cdf_standard_error(self) -> FloatArray:
        return _replicate_standard_error(self.radial_cdf_replicates)

    @property
    def angular_cdf_mean(self) -> FloatArray:
        return self.angular_cdf_replicates.mean(axis=0)

    @property
    def angular_cdf_standard_error(self) -> FloatArray:
        return _replicate_standard_error(self.angular_cdf_replicates)


@dataclass(frozen=True)
class SampleEstimate:
    radial_cdf: FloatArray
    radial_standard_error: FloatArray
    angular_cdf: FloatArray
    angular_standard_error: FloatArray
    quadrant_frequencies: FloatArray


def _positive_int(value: str) -> int:
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("expected a positive integer")
    return parsed


def _replicate_standard_error(values: FloatArray) -> FloatArray:
    if values.shape[0] < 2:
        return np.full(values.shape[1:], math.nan, dtype=np.float64)
    return values.std(axis=0, ddof=1) / math.sqrt(values.shape[0])


def _load_tables(path: Path, audit: Audit) -> Tables:
    packed = path.read_bytes()
    audit.check(
        len(packed) >= HEADER.size,
        "binary header",
        f"{len(packed)} bytes, header requires {HEADER.size}",
    )
    if len(packed) < HEADER.size:
        raise ValueError("truncated FSD LUT header")

    (
        magic,
        format_version,
        generator_version,
        resolution,
        float_count,
        alpha1_theta_offset,
        alpha1_radial_offset,
        alpha2_theta_offset,
        alpha2_radial_offset,
    ) = HEADER.unpack_from(packed)
    radial_count = resolution * resolution
    expected_offsets = (
        0,
        resolution,
        resolution + radial_count,
        2 * resolution + radial_count,
    )
    actual_offsets = (
        alpha1_theta_offset,
        alpha1_radial_offset,
        alpha2_theta_offset,
        alpha2_radial_offset,
    )
    expected_float_count = 2 * resolution + 2 * radial_count
    expected_bytes = HEADER.size + FLOAT_BYTES * float_count

    audit.check(
        magic == TABLE_MAGIC,
        "binary magic",
        f"0x{magic:08x}, expected 0x{TABLE_MAGIC:08x}",
    )
    audit.check(
        format_version == TABLE_VERSION,
        "binary format version",
        f"{format_version}, expected {TABLE_VERSION}",
    )
    audit.check(
        generator_version == GENERATOR_VERSION,
        "generator version",
        f"{generator_version}, expected {GENERATOR_VERSION}",
    )
    audit.check(
        resolution == TABLE_RESOLUTION,
        "table resolution",
        f"{resolution}, expected {TABLE_RESOLUTION}",
    )
    audit.check(
        actual_offsets == expected_offsets,
        "table offsets",
        f"{actual_offsets}, expected {expected_offsets}",
    )
    audit.check(
        float_count == expected_float_count,
        "payload float count",
        f"{float_count}, expected {expected_float_count}",
    )
    audit.check(
        len(packed) == expected_bytes,
        "payload byte count",
        f"{len(packed)}, expected {expected_bytes}",
    )
    if (
        format_version != TABLE_VERSION
        or generator_version != GENERATOR_VERSION
        or resolution != TABLE_RESOLUTION
        or actual_offsets != expected_offsets
        or float_count != expected_float_count
        or len(packed) != expected_bytes
    ):
        raise ValueError("unsupported or malformed FSD LUT layout")

    payload = np.frombuffer(
        packed,
        dtype="<f4",
        count=float_count,
        offset=HEADER.size,
    ).astype(np.float64)

    def take(offset: int, count: int) -> FloatArray:
        return payload[offset : offset + count]

    return Tables(
        resolution=resolution,
        alpha1_theta=take(alpha1_theta_offset, resolution),
        alpha1_radial=take(alpha1_radial_offset, radial_count).reshape(
            resolution, resolution
        ),
        alpha2_theta=take(alpha2_theta_offset, resolution),
        alpha2_radial=take(alpha2_radial_offset, radial_count).reshape(
            resolution, resolution
        ),
    )


def _validate_inverse_cdf(
    name: str,
    theta: FloatArray,
    radial: FloatArray,
    audit: Audit,
) -> None:
    finite = bool(np.all(np.isfinite(theta)) and np.all(np.isfinite(radial)))
    audit.check(finite, f"{name} finite values", "all entries are finite")
    theta_in_range = bool(
        np.all(theta >= 0.0) and np.all(theta <= 0.5 * math.pi + 1.0e-6)
    )
    audit.check(
        theta_in_range,
        f"{name} theta range",
        f"[{theta.min():.9g}, {theta.max():.9g}] rad",
    )
    radial_in_range = bool(np.all(radial >= 0.0))
    audit.check(
        radial_in_range,
        f"{name} radial range",
        f"[{radial.min():.9g}, {radial.max():.9g}]",
    )
    theta_violations = int(np.count_nonzero(np.diff(theta) < 0.0))
    radial_violations = int(np.count_nonzero(np.diff(radial, axis=1) < 0.0))
    audit.check(
        theta_violations == 0,
        f"{name} theta inverse-CDF monotonicity",
        f"{theta_violations} descending steps",
    )
    audit.check(
        radial_violations == 0,
        f"{name} radial inverse-CDF monotonicity",
        f"{radial_violations} descending steps",
    )


def _integrate_shader_targets(
    samples_per_replicate: int,
    replicates: int,
    seed: int,
) -> IntegrationEstimate:
    integral_replicates = np.empty((replicates, 2), dtype=np.float64)
    radial_replicates = np.empty(
        (replicates, 2, len(RADIAL_THRESHOLDS)), dtype=np.float64
    )
    angular_replicates = np.empty(
        (replicates, 2, len(ANGULAR_THRESHOLDS)), dtype=np.float64
    )

    for replicate in range(replicates):
        rng = np.random.default_rng(seed + replicate)
        total_mass = np.zeros(2, dtype=np.float64)
        radial_mass = np.zeros(
            (2, len(RADIAL_THRESHOLDS)), dtype=np.float64
        )
        angular_mass = np.zeros(
            (2, len(ANGULAR_THRESHOLDS)), dtype=np.float64
        )
        remaining = samples_per_replicate
        while remaining:
            count = min(remaining, INTEGRATION_CHUNK_SIZE)
            random_sample = rng.random((count, 2))
            radial_parameter = 0.5 * math.pi * random_sample[:, 0]
            radius = np.tan(radial_parameter)
            phi = 2.0 * math.pi * random_sample[:, 1]
            x = radius * np.cos(phi)
            y = radius * np.sin(phi)
            densities = alpha_lobe_densities(x, y)

            # r=tan(pi*u/2), phi=2*pi*v. The full-plane area Jacobian is
            # r*(dr/du)*(dphi/dv)=pi^2*r*sec^2(pi*u/2).
            area_jacobian = (
                math.pi**2
                * radius
                / (np.cos(radial_parameter) ** 2)
            )
            weighted = densities * area_jacobian[None, :]
            if not np.all(np.isfinite(weighted)):
                raise FloatingPointError("non-finite integration weight")
            total_mass += weighted.sum(axis=1)

            canonical_angle = np.arctan2(np.abs(y), np.abs(x))
            for index, threshold in enumerate(RADIAL_THRESHOLDS):
                radial_mass[:, index] += weighted[:, radius <= threshold].sum(
                    axis=1
                )
            for index, threshold in enumerate(ANGULAR_THRESHOLDS):
                angular_mass[:, index] += weighted[
                    :, canonical_angle <= threshold
                ].sum(axis=1)
            remaining -= count

        integral_replicates[replicate] = total_mass / samples_per_replicate
        radial_replicates[replicate] = radial_mass / total_mass[:, None]
        angular_replicates[replicate] = angular_mass / total_mass[:, None]

    return IntegrationEstimate(
        integral_replicates=integral_replicates,
        radial_cdf_replicates=radial_replicates,
        angular_cdf_replicates=angular_replicates,
    )


def _erfcx_nonnegative(values: FloatArray) -> FloatArray:
    result = np.empty_like(values)
    direct = values < 25.0
    result[direct] = np.fromiter(
        (
            math.exp(float(value * value)) * math.erfc(float(value))
            for value in values[direct]
        ),
        dtype=np.float64,
        count=int(np.count_nonzero(direct)),
    )
    asymptotic_values = values[~direct]
    if asymptotic_values.size:
        inverse_squared = 1.0 / (asymptotic_values * asymptotic_values)
        result[~direct] = (
            1.0
            / (math.sqrt(math.pi) * asymptotic_values)
            * (
                1.0
                - 0.5 * inverse_squared
                + 0.75 * inverse_squared**2
                - 1.875 * inverse_squared**3
            )
        )
    return result


def _integrate_lobes_semi_analytic() -> tuple[FloatArray, FloatArray]:
    """Integrate g1/g2 after analytically eliminating the y dimension.

    For beta=1/6 and a=|x|, the remaining y integral is obtained from
    integral exp(-beta*y^2)/(y^2+a^2) dy = pi*erfcx(a*sqrt(beta))/a.
    The final even x integral is evaluated per oscillation period.
    """

    nodes, weights = np.polynomial.legendre.leggauss(
        SEMI_ANALYTIC_QUADRATURE_ORDER
    )
    period = 2.0 * math.pi
    beta = 1.0 / CENTRAL_EXPONENT_DENOMINATOR
    accumulated = np.zeros(2, dtype=np.float64)

    for begin in range(
        0,
        SEMI_ANALYTIC_QUADRATURE_PERIODS,
        SEMI_ANALYTIC_QUADRATURE_BLOCK_PERIODS,
    ):
        end = min(
            begin + SEMI_ANALYTIC_QUADRATURE_BLOCK_PERIODS,
            SEMI_ANALYTIC_QUADRATURE_PERIODS,
        )
        intervals = np.arange(begin, end, dtype=np.float64)
        midpoint = (intervals + 0.5) * period
        x = (
            midpoint[:, None] + 0.5 * period * nodes[None, :]
        ).reshape(-1)
        scaled_x = x * math.sqrt(beta)
        erfcx = _erfcx_nonnegative(scaled_x)
        gaussian_y_integral = 0.5 * math.pi * (
            erfcx / x
            + 2.0 * beta * x * erfcx
            - 2.0 * math.sqrt(beta / math.pi)
        )
        removed_y_integral = (
            math.pi / (2.0 * x)
            - np.exp(-(scaled_x * scaled_x)) * gaussian_y_integral
        )
        half_x = 0.5 * x
        sinc_half_x = np.sin(half_x) / half_x
        alpha1_difference = np.cos(half_x) - sinc_half_x
        values = np.stack(
            (
                alpha1_difference**2 * removed_y_integral / x**2,
                sinc_half_x**2 * removed_y_integral,
            ),
            axis=0,
        ).reshape(
            2, -1, SEMI_ANALYTIC_QUADRATURE_ORDER
        )
        accumulated += 0.5 * period * np.sum(
            values * weights[None, None, :], axis=(1, 2)
        )

    integrals = accumulated / (2.0 * math.pi**2)
    maximum_x = SEMI_ANALYTIC_QUADRATURE_PERIODS * period
    tail_upper_bounds = np.asarray(
        (
            1.0 / (8.0 * math.pi * maximum_x**2)
            + 1.0 / (3.0 * math.pi * maximum_x**3)
            + 1.0 / (4.0 * math.pi * maximum_x**4),
            1.0 / (2.0 * math.pi * maximum_x**2),
        ),
        dtype=np.float64,
    )
    return integrals, tail_upper_bounds


def _interpolate_inverse_cdf(values: FloatArray, sample: FloatArray) -> FloatArray:
    resolution = len(values)
    coordinate = np.clip(sample, 0.0, 1.0) * (resolution - 1)
    lower = np.floor(coordinate).astype(np.int64)
    upper = np.minimum(lower + 1, resolution - 1)
    fraction = coordinate - lower
    return values[lower] * (1.0 - fraction) + values[upper] * fraction


def _sample_lut_statistics(
    theta_lut: FloatArray,
    radial_lut: FloatArray,
    sample_count: int,
    seed: int,
) -> SampleEstimate:
    rng = np.random.default_rng(seed)
    radial_counts = np.zeros(len(RADIAL_THRESHOLDS), dtype=np.int64)
    angular_counts = np.zeros(len(ANGULAR_THRESHOLDS), dtype=np.int64)
    quadrant_counts = np.zeros(4, dtype=np.int64)
    remaining = sample_count

    while remaining:
        count = min(remaining, INTEGRATION_CHUNK_SIZE)
        random_sample = rng.random((count, 3))
        theta = _interpolate_inverse_cdf(theta_lut, random_sample[:, 0])

        theta_coordinate = (
            np.clip(theta * (2.0 / math.pi), 0.0, 1.0)
            * (len(theta_lut) - 1)
        )
        lower_row = np.floor(theta_coordinate).astype(np.int64)
        upper_row = np.minimum(lower_row + 1, len(theta_lut) - 1)
        row_fraction = theta_coordinate - lower_row

        radial_coordinate = random_sample[:, 1] * (len(theta_lut) - 1)
        lower_column = np.floor(radial_coordinate).astype(np.int64)
        upper_column = np.minimum(lower_column + 1, len(theta_lut) - 1)
        column_fraction = radial_coordinate - lower_column

        lower_radius = (
            radial_lut[lower_row, lower_column]
            * (1.0 - column_fraction)
            + radial_lut[lower_row, upper_column] * column_fraction
        )
        upper_radius = (
            radial_lut[upper_row, lower_column]
            * (1.0 - column_fraction)
            + radial_lut[upper_row, upper_column] * column_fraction
        )
        radius = np.maximum(
            0.0,
            lower_radius * (1.0 - row_fraction)
            + upper_radius * row_fraction,
        )
        quadrant = np.minimum((4.0 * random_sample[:, 2]).astype(int), 3)

        for index, threshold in enumerate(RADIAL_THRESHOLDS):
            radial_counts[index] += np.count_nonzero(radius <= threshold)
        for index, threshold in enumerate(ANGULAR_THRESHOLDS):
            angular_counts[index] += np.count_nonzero(theta <= threshold)
        quadrant_counts += np.bincount(quadrant, minlength=4)
        remaining -= count

    radial_cdf = radial_counts.astype(np.float64) / sample_count
    angular_cdf = angular_counts.astype(np.float64) / sample_count

    def binomial_standard_error(probability: FloatArray) -> FloatArray:
        return np.sqrt(probability * (1.0 - probability) / sample_count)

    return SampleEstimate(
        radial_cdf=radial_cdf,
        radial_standard_error=binomial_standard_error(radial_cdf),
        angular_cdf=angular_cdf,
        angular_standard_error=binomial_standard_error(angular_cdf),
        quadrant_frequencies=quadrant_counts.astype(np.float64) / sample_count,
    )


def _compare_cdf(
    name: str,
    thresholds: FloatArray,
    target: FloatArray,
    target_standard_error: FloatArray,
    sampled: FloatArray,
    sampled_standard_error: FloatArray,
    absolute_tolerance: float,
    sigma_tolerance: float,
    audit: Audit,
    threshold_format: str,
) -> None:
    difference = sampled - target
    combined_standard_error = np.sqrt(
        target_standard_error**2 + sampled_standard_error**2
    )
    statistically_large = np.abs(difference) > (
        sigma_tolerance * combined_standard_error
    )
    materially_large = np.abs(difference) > absolute_tolerance
    mismatched = statistically_large & materially_large

    print(f"\n{name}")
    print(" threshold       target          LUT        delta       sigma")
    for threshold, expected, actual, delta, error, failed in zip(
        thresholds,
        target,
        sampled,
        difference,
        combined_standard_error,
        mismatched,
    ):
        sigma = abs(delta) / error if error > 0.0 else math.inf
        marker = "*" if failed else " "
        print(
            f"{marker}{threshold_format.format(threshold):>9}  "
            f"{expected:12.8f}  {actual:12.8f}  "
            f"{delta:+10.6f}  {sigma:9.2f}"
        )

    audit.check(
        not bool(np.any(mismatched)),
        f"{name} agreement",
        f"{int(np.count_nonzero(mismatched))}/{len(thresholds)} "
        "materially significant bins marked with *",
    )


def _audit_numerics(
    tables: Tables,
    args: argparse.Namespace,
    audit: Audit,
) -> None:
    shader_integrals = _load_shader_integrals()
    print(
        "\nIntegrating shader targets with "
        f"{args.integration_replicates} x "
        f"{args.integration_samples:,} samples"
    )
    integration = _integrate_shader_targets(
        args.integration_samples,
        args.integration_replicates,
        args.seed,
    )
    quadrature_integrals, quadrature_tail_bounds = (
        _integrate_lobes_semi_analytic()
    )
    numeric_integrals = integration.integral_mean
    numeric_standard_error = integration.integral_standard_error

    print("\nProposal normalization")
    print(" lobe   shader const      MC integral        MC SE      ratio")
    for index, (name, shader_integral) in enumerate(
        zip(("alpha1", "alpha2"), shader_integrals, strict=True)
    ):
        ratio = numeric_integrals[index] / shader_integral
        print(
            f" {name:6}  {shader_integral:12.10f}  "
            f"{numeric_integrals[index]:14.10f}  "
            f"{numeric_standard_error[index]:9.2e}  {ratio:9.6f}"
        )

    print("\nSemi-analytic full-plane integrals")
    for index, name in enumerate(("alpha1", "alpha2")):
        print(
            f" {name}: {quadrature_integrals[index]:.10f}, "
            f"tail bound {quadrature_tail_bounds[index]:.2e}"
        )
        mc_difference = abs(
            numeric_integrals[index] - quadrature_integrals[index]
        )
        mc_tolerance = max(
            args.integral_relative_tolerance * quadrature_integrals[index],
            args.sigma_tolerance * numeric_standard_error[index],
        ) + quadrature_tail_bounds[index]
        audit.check(
            mc_difference <= mc_tolerance,
            f"{name} MC/quadrature cross-check",
            f"difference {mc_difference:.3e}, "
            f"tolerance {mc_tolerance:.3e}",
        )

    alpha1_difference = abs(quadrature_integrals[0] - shader_integrals[0])
    alpha1_tolerance = (
        args.integral_relative_tolerance * shader_integrals[0]
        + quadrature_tail_bounds[0]
    )
    alpha1_constants_match = alpha1_difference <= alpha1_tolerance
    alpha2_difference = abs(quadrature_integrals[1] - shader_integrals[1])
    alpha2_tolerance = (
        args.integral_relative_tolerance * shader_integrals[1]
        + quadrature_tail_bounds[1]
    )
    alpha2_constants_match = alpha2_difference <= alpha2_tolerance
    constant_checks = (
        (
            alpha1_constants_match,
            "alpha1 shader normalization",
            f"difference {alpha1_difference:.3e}, "
            f"tolerance {alpha1_tolerance:.3e}",
        ),
        (
            alpha2_constants_match,
            "alpha2 shader normalization",
            f"difference {alpha2_difference:.3e}, "
            f"tolerance {alpha2_tolerance:.3e}",
        ),
    )
    for condition, name, detail in constant_checks:
        if args.require_shader_constants:
            audit.check(condition, name, detail)
        else:
            audit.warn_if_false(condition, name, detail)

    samples = (
        _sample_lut_statistics(
            tables.alpha1_theta,
            tables.alpha1_radial,
            args.samples,
            args.seed + 10_000,
        ),
        _sample_lut_statistics(
            tables.alpha2_theta,
            tables.alpha2_radial,
            args.samples,
            args.seed + 20_000,
        ),
    )
    expected_quadrant = 0.25
    quadrant_standard_error = math.sqrt(
        expected_quadrant * (1.0 - expected_quadrant) / args.samples
    )

    for lobe_index, (name, sample) in enumerate(
        (("alpha1", samples[0]), ("alpha2", samples[1]))
    ):
        max_quadrant_deviation = float(
            np.max(np.abs(sample.quadrant_frequencies - expected_quadrant))
        )
        audit.check(
            max_quadrant_deviation
            <= args.sigma_tolerance * quadrant_standard_error,
            f"{name} quadrant signs",
            f"frequencies {sample.quadrant_frequencies.tolist()}",
        )
        _compare_cdf(
            f"{name} radial CDF",
            RADIAL_THRESHOLDS,
            integration.radial_cdf_mean[lobe_index],
            integration.radial_cdf_standard_error[lobe_index],
            sample.radial_cdf,
            sample.radial_standard_error,
            args.cdf_absolute_tolerance,
            args.sigma_tolerance,
            audit,
            "{:.0f}",
        )
        _compare_cdf(
            f"{name} canonical-angle CDF",
            np.degrees(ANGULAR_THRESHOLDS),
            integration.angular_cdf_mean[lobe_index],
            integration.angular_cdf_standard_error[lobe_index],
            sample.angular_cdf,
            sample.angular_standard_error,
            args.cdf_absolute_tolerance,
            args.sigma_tolerance,
            audit,
            "{:.0f} deg",
        )

    shader_mixture_alpha1 = shader_integrals[0] / (
        shader_integrals[0] + shader_integrals[1]
    )
    numeric_mixture_alpha1 = quadrature_integrals[0] / (
        quadrature_integrals[0] + quadrature_integrals[1]
    )
    print("\nEqual-coefficient lobe mixture")
    print(
        " alpha1 probability from shader constants: "
        f"{shader_mixture_alpha1:.9f}"
    )
    print(
        " alpha1 probability from numerical g1/g2 integrals: "
        f"{numeric_mixture_alpha1:.9f}"
    )
    print(
        " Pure-alpha PDF solid-angle integrals with the shader's cos^3 "
        "Jacobian are the ratios in the normalization table above."
    )


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--table", type=Path, default=DEFAULT_TABLE_PATH)
    parser.add_argument(
        "--samples",
        type=_positive_int,
        default=1_000_000,
        help="LUT samples per lobe (default: 1000000)",
    )
    parser.add_argument(
        "--integration-samples",
        type=_positive_int,
        default=500_000,
        help="full-plane samples per replicate (default: 500000)",
    )
    parser.add_argument(
        "--integration-replicates",
        type=_positive_int,
        default=8,
        help="independent integration replicates (default: 8)",
    )
    parser.add_argument("--seed", type=int, default=20_260_725)
    parser.add_argument(
        "--cdf-absolute-tolerance",
        type=float,
        default=0.005,
    )
    parser.add_argument("--sigma-tolerance", type=float, default=6.0)
    parser.add_argument(
        "--integral-relative-tolerance",
        type=float,
        default=0.005,
    )
    parser.add_argument(
        "--report-only",
        action="store_true",
        help="report failures without returning a failing exit status",
    )
    parser.add_argument(
        "--require-shader-constants",
        action="store_true",
        help="treat shader-integral normalization mismatches as failures",
    )
    return parser


def main() -> int:
    args = _build_parser().parse_args()
    if args.integration_replicates < 2:
        raise SystemExit("--integration-replicates must be at least 2")
    for name in (
        "cdf_absolute_tolerance",
        "sigma_tolerance",
        "integral_relative_tolerance",
    ):
        if getattr(args, name) <= 0.0:
            raise SystemExit(f"--{name.replace('_', '-')} must be positive")

    audit = Audit()
    table_path = args.table.resolve()
    print(f"FSD LUT audit: {table_path}")
    tables = _load_tables(table_path, audit)
    _validate_inverse_cdf(
        "alpha1", tables.alpha1_theta, tables.alpha1_radial, audit
    )
    _validate_inverse_cdf(
        "alpha2", tables.alpha2_theta, tables.alpha2_radial, audit
    )
    _audit_numerics(tables, args, audit)

    if audit.failures:
        print(
            f"\nRESULT: FAIL ({len(audit.failures)} checks): "
            + ", ".join(audit.failures)
        )
        print(
            "Recommendation: regenerate both inverse-CDF tables and lobe "
            "integrals from one versioned implementation of the shader "
            "g1/g2 functions, with explicit tail/error bounds."
        )
        return 0 if args.report_only else 1

    if audit.warnings:
        print(
            f"\nRESULT: PASS ({len(audit.warnings)} warnings): "
            + ", ".join(audit.warnings)
        )
        print(
            "The LUT matches g1/g2, but the shader power constants must be "
            "updated before its reported PDF is normalized."
        )
    else:
        print("\nRESULT: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
