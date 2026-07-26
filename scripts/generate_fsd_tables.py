"""Generate FSD inverse-CDF tables from the shader's canonical lobes.

The packed file records both its binary layout version and the deterministic
generator version so the runtime cannot accept a semantically stale table.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from numpy.typing import NDArray


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = (
    ROOT / "build" / "download" / "render_resources" / "fsd_tables.bytes"
)

TABLE_RESOLUTION = 1024
TABLE_MAGIC = 0x4C445346  # "FSDL" in little-endian byte order.
TABLE_VERSION = 2
GENERATOR_VERSION = 2
HEADER = struct.Struct("<9I")

# These define the same g1/g2 functions as FreeSpaceDiffractionBSDF.
CENTRAL_EXPONENT_DENOMINATOR = 6.0
SMALL_ALPHA1_X = 1.0e-4

# q = zeta_x/2 is used for radial integration. The logarithmic section
# resolves the removed-central-lobe transition near q=0; the uniform section
# resolves every oscillation in sinc(q) and cos(q)-sinc(q).
Q_LOG_MINIMUM = 1.0e-12
Q_LOG_MAXIMUM = 1.0
Q_LOG_SAMPLE_COUNT = 8193
Q_OSCILLATION_SUBDIVISIONS = 256
Q_CORE_MAXIMUM = 8.0
Q_MAXIMUM = 4096.0

# theta = pi/2 * (1-(1-u)^2) clusters the marginal integration grid at the
# logarithmic alpha2 singularity at theta=pi/2.
THETA_PARAMETER_SAMPLE_COUNT = 8193
INTEGRATION_COSINE_BATCH_SIZE = 128

# u=1 has an infinite inverse for the unbounded lobes. Use the midpoint of the
# final shader interpolation cell as its finite representative. This avoids
# mapping the entire last cell linearly towards an extreme near-infinite value.
INVERSE_CDF_TAIL_PROBABILITY = 0.5 / (TABLE_RESOLUTION - 1)


FloatArray = NDArray[np.float64]


@dataclass(frozen=True)
class RadialGrid:
    q: FloatArray
    alpha1_base: FloatArray
    alpha2_base: FloatArray
    core_count: int


@dataclass(frozen=True)
class MarginalResult:
    theta: FloatArray
    alpha1_cdf: FloatArray
    alpha2_cdf: FloatArray
    alpha1_integral: float
    alpha2_integral: float
    alpha1_convergence_delta: float
    alpha2_convergence_delta: float
    alpha1_tail_upper_bound: float
    alpha2_tail_upper_bound: float


@dataclass(frozen=True)
class GenerationResult:
    alpha1_integral: float
    alpha2_integral: float
    alpha1_convergence_delta: float
    alpha2_convergence_delta: float
    alpha1_tail_upper_bound: float
    alpha2_tail_upper_bound: float
    sha256: str


def _stable_sinc(values: FloatArray) -> FloatArray:
    result = np.empty_like(values)
    small = np.abs(values) < SMALL_ALPHA1_X
    squared = values[small] * values[small]
    result[small] = 1.0 - squared / 6.0 + squared * squared / 120.0
    result[~small] = np.sin(values[~small]) / values[~small]
    return result


def alpha_lobe_densities(x: FloatArray, y: FloatArray) -> FloatArray:
    """Return shader g1/g2 densities with respect to d^2 zeta."""

    radius_squared = x * x + y * y
    half_x = 0.5 * x
    sinc_half_x = _stable_sinc(half_x)

    alpha1_factor = np.empty_like(x)
    small_x = np.abs(x) < SMALL_ALPHA1_X
    alpha1_factor[small_x] = (
        -x[small_x] / 24.0 + x[small_x] ** 3 / 960.0
    )
    alpha1_factor[~small_x] = (
        np.cos(half_x[~small_x]) - sinc_half_x[~small_x]
    ) / (2.0 * x[~small_x])

    inverse_radius_squared = np.divide(
        1.0,
        radius_squared,
        out=np.zeros_like(radius_squared),
        where=radius_squared > 0.0,
    )
    alpha1 = y * inverse_radius_squared * alpha1_factor / math.pi
    alpha2 = y * inverse_radius_squared * sinc_half_x / (2.0 * math.pi)
    removed_central_intensity = -np.expm1(
        -radius_squared / CENTRAL_EXPONENT_DENOMINATOR
    )
    return np.stack(
        (
            removed_central_intensity * alpha1 * alpha1,
            removed_central_intensity * alpha2 * alpha2,
        ),
        axis=0,
    )


def _cumulative_trapezoid(values: FloatArray, coordinates: FloatArray) -> FloatArray:
    result = np.empty_like(values)
    result[0] = 0.0
    result[1:] = np.cumsum(
        0.5
        * (values[:-1] + values[1:])
        * np.diff(coordinates),
        dtype=np.float64,
    )
    return result


def _trapezoid(values: FloatArray, coordinates: FloatArray) -> float:
    return float(_cumulative_trapezoid(values, coordinates)[-1])


def _build_radial_grid(
    log_sample_count: int = Q_LOG_SAMPLE_COUNT,
    oscillation_subdivisions: int = Q_OSCILLATION_SUBDIVISIONS,
) -> RadialGrid:
    logarithmic = np.geomspace(
        Q_LOG_MINIMUM,
        Q_LOG_MAXIMUM,
        log_sample_count,
        dtype=np.float64,
    )
    step = math.pi / oscillation_subdivisions
    oscillatory = np.arange(
        Q_LOG_MAXIMUM + step,
        Q_MAXIMUM,
        step,
        dtype=np.float64,
    )
    q = np.unique(
        np.concatenate(
            (
                np.asarray((0.0,), dtype=np.float64),
                logarithmic,
                oscillatory,
                np.asarray((Q_CORE_MAXIMUM, Q_MAXIMUM), dtype=np.float64),
            )
        )
    )
    sinc_q = _stable_sinc(q)
    difference = np.empty_like(q)
    small = np.abs(q) < SMALL_ALPHA1_X
    squared = q[small] * q[small]
    difference[small] = (
        -squared / 3.0
        + squared * squared / 30.0
        - squared * squared * squared / 840.0
    )
    difference[~small] = np.cos(q[~small]) - sinc_q[~small]

    alpha1_base = np.divide(
        difference * difference,
        q * q * q,
        out=np.zeros_like(q),
        where=q > 0.0,
    )
    alpha2_base = np.divide(
        sinc_q * sinc_q,
        q,
        out=np.zeros_like(q),
        where=q > 0.0,
    )
    core_count = int(np.searchsorted(q, Q_CORE_MAXIMUM, side="right"))
    return RadialGrid(q, alpha1_base, alpha2_base, core_count)


def _removed_central_factor(q: FloatArray, cosine: FloatArray) -> FloatArray:
    ratio_squared = (q[None, :] / cosine[:, None]) ** 2
    return -np.expm1(-(2.0 / 3.0) * ratio_squared)


def _radial_integrals_for_cosines(
    cosines: FloatArray,
    grid: RadialGrid,
) -> FloatArray:
    q_core = grid.q[: grid.core_count]
    alpha1_core = grid.alpha1_base[: grid.core_count]
    alpha2_core = grid.alpha2_base[: grid.core_count]

    tail_begin = grid.core_count - 1
    alpha1_tail = _trapezoid(
        grid.alpha1_base[tail_begin:], grid.q[tail_begin:]
    )
    alpha2_tail = _trapezoid(
        grid.alpha2_base[tail_begin:], grid.q[tail_begin:]
    )

    result = np.empty((2, len(cosines)), dtype=np.float64)
    for begin in range(0, len(cosines), INTEGRATION_COSINE_BATCH_SIZE):
        end = min(begin + INTEGRATION_COSINE_BATCH_SIZE, len(cosines))
        central = _removed_central_factor(q_core, cosines[begin:end])
        result[0, begin:end] = np.sum(
            0.5
            * (
                central[:, :-1] * alpha1_core[None, :-1]
                + central[:, 1:] * alpha1_core[None, 1:]
            )
            * np.diff(q_core)[None, :],
            axis=1,
        ) + alpha1_tail
        result[1, begin:end] = np.sum(
            0.5
            * (
                central[:, :-1] * alpha2_core[None, :-1]
                + central[:, 1:] * alpha2_core[None, 1:]
            )
            * np.diff(q_core)[None, :],
            axis=1,
        ) + alpha2_tail
    return result


def _build_angular_marginals(grid: RadialGrid) -> MarginalResult:
    parameter = np.linspace(
        0.0, 1.0, THETA_PARAMETER_SAMPLE_COUNT, dtype=np.float64
    )
    theta = 0.5 * math.pi * (1.0 - (1.0 - parameter) ** 2)
    derivative = math.pi * (1.0 - parameter)
    cosine = np.cos(theta)
    sine_squared = np.sin(theta) ** 2

    radial_integrals = np.zeros((2, len(theta)), dtype=np.float64)
    radial_integrals[:, :-1] = _radial_integrals_for_cosines(
        cosine[:-1], grid
    )
    alpha1_marginal = (
        sine_squared * radial_integrals[0] / (16.0 * math.pi**2)
    )
    alpha2_marginal = (
        sine_squared * radial_integrals[1] / (4.0 * math.pi**2)
    )
    alpha1_parameter_density = alpha1_marginal * derivative
    alpha2_parameter_density = alpha2_marginal * derivative
    alpha1_parameter_density[-1] = 0.0
    alpha2_parameter_density[-1] = 0.0

    alpha1_cumulative = _cumulative_trapezoid(
        alpha1_parameter_density, parameter
    )
    alpha2_cumulative = _cumulative_trapezoid(
        alpha2_parameter_density, parameter
    )
    alpha1_quadrant_integral = float(alpha1_cumulative[-1])
    alpha2_quadrant_integral = float(alpha2_cumulative[-1])

    coarse_parameter = parameter[::2]
    alpha1_coarse = 4.0 * _trapezoid(
        alpha1_parameter_density[::2], coarse_parameter
    )
    alpha2_coarse = 4.0 * _trapezoid(
        alpha2_parameter_density[::2], coarse_parameter
    )
    alpha1_integral = 4.0 * alpha1_quadrant_integral
    alpha2_integral = 4.0 * alpha2_quadrant_integral

    q_maximum = grid.q[-1]
    alpha1_radial_tail_bound = (
        1.0 / (2.0 * q_maximum**2)
        + 2.0 / (3.0 * q_maximum**3)
        + 1.0 / (4.0 * q_maximum**4)
    )
    alpha2_radial_tail_bound = 1.0 / (2.0 * q_maximum**2)
    alpha1_full_tail_bound = alpha1_radial_tail_bound / (16.0 * math.pi)
    alpha2_full_tail_bound = alpha2_radial_tail_bound / (4.0 * math.pi)

    return MarginalResult(
        theta=theta,
        alpha1_cdf=alpha1_cumulative / alpha1_quadrant_integral,
        alpha2_cdf=alpha2_cumulative / alpha2_quadrant_integral,
        alpha1_integral=alpha1_integral,
        alpha2_integral=alpha2_integral,
        alpha1_convergence_delta=abs(alpha1_integral - alpha1_coarse),
        alpha2_convergence_delta=abs(alpha2_integral - alpha2_coarse),
        alpha1_tail_upper_bound=alpha1_full_tail_bound,
        alpha2_tail_upper_bound=alpha2_full_tail_bound,
    )


def _inverse_cdf(
    coordinates: FloatArray,
    cdf: FloatArray,
    probabilities: FloatArray,
) -> FloatArray:
    keep = np.concatenate(
        (
            np.asarray((True,)),
            np.diff(cdf) > np.finfo(np.float64).eps,
        )
    )
    keep[-1] = True
    return np.interp(probabilities, cdf[keep], coordinates[keep])


def _inverse_cdf_probabilities() -> FloatArray:
    probabilities = np.linspace(
        0.0, 1.0, TABLE_RESOLUTION, dtype=np.float64
    )
    probabilities[-1] = 1.0 - INVERSE_CDF_TAIL_PROBABILITY
    return probabilities


def _conditional_radial_table(
    alpha_base: FloatArray,
    theta_maximum: float,
    grid: RadialGrid,
) -> FloatArray:
    probabilities = _inverse_cdf_probabilities()
    nominal_theta = np.linspace(
        0.0, 0.5 * math.pi, TABLE_RESOLUTION, dtype=np.float64
    )
    effective_theta = nominal_theta.copy()
    effective_theta[-1] = theta_maximum
    cosines = np.cos(effective_theta)
    table = np.empty(
        (TABLE_RESOLUTION, TABLE_RESOLUTION), dtype=np.float64
    )

    q_core = grid.q[: grid.core_count]
    base_core = alpha_base[: grid.core_count]
    for row, cosine in enumerate(cosines):
        weighted = alpha_base.copy()
        weighted[: grid.core_count] = (
            base_core
            * _removed_central_factor(
                q_core, np.asarray((cosine,), dtype=np.float64)
            )[0]
        )
        cumulative = _cumulative_trapezoid(weighted, grid.q)
        cdf = cumulative / cumulative[-1]
        inverse_q = _inverse_cdf(grid.q, cdf, probabilities)
        table[row] = 2.0 * inverse_q / cosine
    return table


def _pack_tables(output: Path, tables: tuple[FloatArray, ...]) -> str:
    float_tables = tuple(
        np.asarray(table, dtype="<f4").reshape(-1) for table in tables
    )
    offsets: list[int] = []
    float_count = 0
    for table in float_tables:
        offsets.append(float_count)
        float_count += len(table)
    header = HEADER.pack(
        TABLE_MAGIC,
        TABLE_VERSION,
        GENERATOR_VERSION,
        TABLE_RESOLUTION,
        float_count,
        *offsets,
    )
    packed = header + b"".join(table.tobytes() for table in float_tables)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(packed)
    return hashlib.sha256(packed).hexdigest()


def generate(output: Path) -> GenerationResult:
    grid = _build_radial_grid()
    marginal = _build_angular_marginals(grid)
    probabilities = _inverse_cdf_probabilities()
    alpha1_theta = _inverse_cdf(
        marginal.theta, marginal.alpha1_cdf, probabilities
    )
    alpha2_theta = _inverse_cdf(
        marginal.theta, marginal.alpha2_cdf, probabilities
    )
    alpha1_radial = _conditional_radial_table(
        grid.alpha1_base, float(alpha1_theta[-1]), grid
    )
    alpha2_radial = _conditional_radial_table(
        grid.alpha2_base, float(alpha2_theta[-1]), grid
    )
    sha256 = _pack_tables(
        output,
        (alpha1_theta, alpha1_radial, alpha2_theta, alpha2_radial),
    )
    return GenerationResult(
        alpha1_integral=marginal.alpha1_integral,
        alpha2_integral=marginal.alpha2_integral,
        alpha1_convergence_delta=marginal.alpha1_convergence_delta,
        alpha2_convergence_delta=marginal.alpha2_convergence_delta,
        alpha1_tail_upper_bound=marginal.alpha1_tail_upper_bound,
        alpha2_tail_upper_bound=marginal.alpha2_tail_upper_bound,
        sha256=sha256,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    output = args.output.resolve()
    result = generate(output)
    print(f"Generated {output}")
    print(f"format_version={TABLE_VERSION}")
    print(f"generator_version={GENERATOR_VERSION}")
    print(f"resolution={TABLE_RESOLUTION}")
    print(f"inverse_cdf_tail_probability={INVERSE_CDF_TAIL_PROBABILITY:.9g}")
    print(f"alpha1_integral={result.alpha1_integral:.12g}")
    print(f"alpha2_integral={result.alpha2_integral:.12g}")
    print(
        "alpha1_angular_convergence_delta="
        f"{result.alpha1_convergence_delta:.3e}"
    )
    print(
        "alpha2_angular_convergence_delta="
        f"{result.alpha2_convergence_delta:.3e}"
    )
    print(f"alpha1_tail_upper_bound={result.alpha1_tail_upper_bound:.3e}")
    print(f"alpha2_tail_upper_bound={result.alpha2_tail_upper_bound:.3e}")
    print(f"sha256={result.sha256}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
