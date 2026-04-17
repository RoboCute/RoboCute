"""Pure Pursuit path-tracking algorithm using numpy data structures.

This module provides a high-performance, configurable pure-pursuit implementation
with adaptive lookahead, bicycle-model curvature, bidirectional support, and
comprehensive path-preprocessing.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from enum import Enum, auto
from typing import Optional, Tuple, Union

import numpy as np


class LookaheadMode(Enum):
    """Strategy for determining the lookahead distance."""

    FIXED = auto()
    ADAPTIVE = auto()
    VELOCITY_SCALED = auto()


class DirectionMode(Enum):
    """Allowed driving direction along the path."""

    FORWARD = auto()
    REVERSE = auto()
    BIDIRECTIONAL = auto()


@dataclass(frozen=True, slots=True)
class PursuitOptions:
    """Configuration for the Pure Pursuit tracker.

    Parameters
    ----------
    lookahead_distance : float
        Base lookahead distance (must be > 0).  Defaults to ``1.0``.
    lookahead_mode : LookaheadMode
        How the lookahead distance is chosen.  Defaults to ``FIXED``.
    adaptive_gain : float
        Gain used for adaptive lookahead (larger = more aggressive adaptation).
        Defaults to ``0.5``.
    velocity_scale_factor : float
        Multiplier for velocity-scaled lookahead: ``L = base + factor * |v|``.
        Defaults to ``0.5``.
    wheelbase : float
        Wheelbase length for bicycle-model curvature.  ``0.0`` disables the
        bicycle model and returns raw curvature.  Defaults to ``0.0``.
    max_curvature : float | None
        Maximum allowed curvature (1 / min_turning_radius).  ``None`` means
        unlimited.  Defaults to ``None``.
    max_steering_angle : float | None
        Maximum steering angle in radians.  If set, it overrides ``max_curvature``
        via ``curvature = tan(delta) / wheelbase``.  Defaults to ``None``.
    direction_mode : DirectionMode
        Whether the vehicle may drive forward, reverse, or both.  Defaults to
        ``FORWARD``.
    path_resolution : float
        Minimum distance between consecutive path waypoints.  Waypoints closer
        than this are merged during preprocessing.  Defaults to ``1e-6``.
    done_tolerance : float
        Distance from the final waypoint considered as "reached".  Defaults to
        ``0.1``.
    initial_path_index : int
        Segment index to start searching from on the first update.  Defaults
        to ``0``.
    """

    lookahead_distance: float = 1.0
    lookahead_mode: LookaheadMode = LookaheadMode.FIXED
    adaptive_gain: float = 0.5
    velocity_scale_factor: float = 0.5
    wheelbase: float = 0.0
    max_curvature: Optional[float] = None
    max_steering_angle: Optional[float] = None
    direction_mode: DirectionMode = DirectionMode.FORWARD
    path_resolution: float = 1e-6
    done_tolerance: float = 0.1
    initial_path_index: int = 0

    def __post_init__(self) -> None:
        if self.lookahead_distance <= 0.0:
            raise ValueError("lookahead_distance must be positive")
        if self.wheelbase < 0.0:
            raise ValueError("wheelbase must be non-negative")
        if self.max_curvature is not None and self.max_curvature < 0.0:
            raise ValueError("max_curvature must be non-negative")
        if self.max_steering_angle is not None and self.max_steering_angle < 0.0:
            raise ValueError("max_steering_angle must be non-negative")
        if self.path_resolution < 0.0:
            raise ValueError("path_resolution must be non-negative")
        if self.done_tolerance < 0.0:
            raise ValueError("done_tolerance must be non-negative")


@dataclass(frozen=True, slots=True)
class VehicleState:
    """Vehicle pose and velocity.

    Parameters
    ----------
    position : np.ndarray
        1-D array of shape ``(dim,)``.
    heading : float
        Heading angle in radians (0 = +x axis, CCW positive).
    velocity : float
        Signed longitudinal velocity.  Negative means reverse motion.
    """

    position: np.ndarray
    heading: float
    velocity: float

    def __post_init__(self) -> None:
        if not isinstance(self.position, np.ndarray):
            object.__setattr__(self, "position", np.array(self.position, dtype=np.float64))
        object.__setattr__(
            self, "position", self.position.astype(np.float64, copy=False).reshape(-1)
        )


@dataclass(frozen=True, slots=True)
class TrackingResult:
    """Output of a single pure-pursuit update.

    Boolean evaluation returns ``success``.
    

    Parameters
    ----------
    success : bool
        True if a valid lookahead point was found.
    lookahead_point : np.ndarray
        Target point on the path.  Empty array if ``success`` is False.
    curvature : float
        Signed curvature command.  Positive = left turn.
    steering_angle : float
        Steering angle in radians derived from curvature and wheelbase.
    cross_track_error : float
        Shortest distance from vehicle to path.
    heading_error : float
        Angle difference between path tangent at the closest point and vehicle
        heading, wrapped to ``[-pi, pi]``.
    closest_point : np.ndarray
        Closest point on the path to the vehicle.
    closest_index : int
        Index of the line segment that contains the closest point.
    lookahead_distance : float
        Effective lookahead distance used for this update.
    distance_to_goal : float
        Arc-length remaining from the closest point to the path end.
    is_done : bool
        True if the vehicle is within ``done_tolerance`` of the final waypoint.
    reverse : bool
        True if the tracker recommends driving in reverse.
    """

    success: bool
    lookahead_point: np.ndarray
    curvature: float
    steering_angle: float
    cross_track_error: float
    heading_error: float
    closest_point: np.ndarray
    closest_index: int
    lookahead_distance: float
    distance_to_goal: float
    is_done: bool
    reverse: bool

    def __bool__(self) -> bool:
        return self.success


class PursuitPath:
    """Preprocessed reference path with fast geometric queries.

    The constructor automatically filters duplicate or nearly-coincident
    waypoints and builds cumulative arc-length tables.
    """

    def __init__(self, waypoints: np.ndarray, resolution: float = 1e-6) -> None:
        """
        Parameters
        ----------
        waypoints : np.ndarray
            Array of shape ``(N, dim)`` where ``N >= 1``.
        resolution : float
            Minimum distance between consecutive waypoints; closer points are merged.
        """
        pts = np.array(waypoints, dtype=np.float64)
        if pts.ndim != 2:
            raise ValueError("waypoints must be a 2-D array")
        if len(pts) == 0:
            raise ValueError("waypoints must contain at least one point")

        # Merge nearly-coincident points
        filtered = [pts[0]]
        for p in pts[1:]:
            if np.linalg.norm(p - filtered[-1]) > resolution:
                filtered.append(p)
        self.waypoints = np.array(filtered, dtype=np.float64)
        self.n_points = len(self.waypoints)
        self.dim = self.waypoints.shape[1]

        if self.n_points >= 2:
            self.diffs = np.diff(self.waypoints, axis=0)
            self.seg_lengths = np.linalg.norm(self.diffs, axis=1)
            self.cumlen = np.concatenate(([0.0], np.cumsum(self.seg_lengths)))
            self.total_length = float(self.cumlen[-1])
            self.tangents = self.diffs / (self.seg_lengths[:, None] + 1e-12)
        else:
            self.diffs = np.empty((0, self.dim), dtype=np.float64)
            self.seg_lengths = np.array([], dtype=np.float64)
            self.cumlen = np.array([0.0], dtype=np.float64)
            self.total_length = 0.0
            self.tangents = np.empty((0, self.dim), dtype=np.float64)

    def __len__(self) -> int:
        return self.n_points

    def closest_point(
        self, position: np.ndarray, search_start: int = 0
    ) -> Tuple[np.ndarray, float, int]:
        """Return (closest_point, arc_length, segment_index).

        The search is accelerated by starting at ``search_start`` and scanning
        forward/backward until the distance starts increasing.
        """
        position = np.asarray(position, dtype=np.float64).reshape(-1)
        if self.n_points == 1:
            return self.waypoints[0].copy(), 0.0, 0

        # Local search around search_start for O(1) amortized cost when the
        # vehicle progresses monotonically along the path.
        start_idx = max(0, min(search_start, self.n_points - 2))
        best_idx = start_idx
        best_dist_sq, best_t = self._project_dist_sq(position, start_idx)

        # Scan forward
        idx = start_idx + 1
        while idx <= self.n_points - 2:
            dist_sq, t = self._project_dist_sq(position, idx)
            if dist_sq < best_dist_sq:
                best_dist_sq = dist_sq
                best_idx = idx
                best_t = t
                idx += 1
            else:
                # Once distance increases, monotonicity usually holds forward.
                break

        # Scan backward
        idx = start_idx - 1
        while idx >= 0:
            dist_sq, t = self._project_dist_sq(position, idx)
            if dist_sq < best_dist_sq:
                best_dist_sq = dist_sq
                best_idx = idx
                best_t = t
                idx -= 1
            else:
                break

        closest = self.waypoints[best_idx] + best_t * self.diffs[best_idx]
        s = self.cumlen[best_idx] + best_t * self.seg_lengths[best_idx]
        return closest, s, best_idx

    def _project_dist_sq(self, position: np.ndarray, seg_idx: int) -> Tuple[float, float]:
        diff = self.diffs[seg_idx]
        denom = float(np.dot(diff, diff)) + 1e-18
        t = float(np.dot(position - self.waypoints[seg_idx], diff)) / denom
        t = max(0.0, min(1.0, t))
        proj = self.waypoints[seg_idx] + t * diff
        dist_sq = float(np.sum((proj - position) ** 2))
        return dist_sq, t

    def lookahead_point(
        self, position: np.ndarray, arc_length: float, lookahead_distance: float
    ) -> Tuple[np.ndarray, bool]:
        """Return (lookahead_point, found).

        The search starts at ``arc_length`` and moves forward by
        ``lookahead_distance`` along the path.
        """
        if self.n_points == 1:
            dist = float(np.linalg.norm(self.waypoints[0] - position))
            return self.waypoints[0].copy(), dist <= lookahead_distance + 1e-9

        target_s = arc_length + lookahead_distance
        if target_s >= self.total_length - 1e-9:
            return self.waypoints[-1].copy(), True

        seg_idx = int(np.searchsorted(self.cumlen, target_s) - 1)
        seg_idx = max(0, min(seg_idx, self.n_points - 2))
        seg_start = self.cumlen[seg_idx]
        seg_len = self.seg_lengths[seg_idx]
        if seg_len < 1e-12:
            return self.waypoints[seg_idx].copy(), True
        alpha = (target_s - seg_start) / seg_len
        pt = self.waypoints[seg_idx] + alpha * self.diffs[seg_idx]
        return pt, True

    def tangent_at(self, seg_idx: int) -> np.ndarray:
        """Return the unit tangent of segment ``seg_idx``."""
        if self.n_points <= 1 or seg_idx < 0 or seg_idx >= len(self.tangents):
            return np.zeros(self.dim, dtype=np.float64)
        return self.tangents[seg_idx].copy()

    def end_point(self) -> np.ndarray:
        return self.waypoints[-1].copy()


class PurePursuit:
    """Pure Pursuit path tracker backed by numpy arrays.

    Parameters
    ----------
    path : np.ndarray
        Reference path as ``(N, dim)`` array.
    options : PursuitOptions | None
        Tracker configuration.
    """

    def __init__(
        self,
        path: np.ndarray,
        options: Optional[PursuitOptions] = None,
    ) -> None:
        self.options = options or PursuitOptions()
        self.path = PursuitPath(path, resolution=self.options.path_resolution)
        self._last_closest_index: int = max(
            0, min(self.options.initial_path_index, self.path.n_points - 2)
        )

    def update(self, state: Union[VehicleState, np.ndarray], velocity: float = 0.0, heading: float = 0.0) -> TrackingResult:
        """Compute the tracking command for the current vehicle state.

        Parameters
        ----------
        state : VehicleState | np.ndarray
            Either a ``VehicleState`` instance or a raw position array.  If an
            array is passed, ``velocity`` and ``heading`` must also be supplied.
        velocity : float
            Signed longitudinal velocity (only used when ``state`` is an array).
        heading : float
            Vehicle heading in radians (only used when ``state`` is an array).

        Returns
        -------
        TrackingResult
            Complete tracking output including curvature and steering angle.
        """
        if isinstance(state, VehicleState):
            position = state.position
            velocity = state.velocity
            heading = state.heading
        else:
            position = np.asarray(state, dtype=np.float64).reshape(-1)

        if position.shape[0] != self.path.dim:
            raise ValueError(
                f"position dimension {position.shape[0]} does not match path dimension {self.path.dim}"
            )

        # Determine motion direction
        reverse = False
        if self.options.direction_mode == DirectionMode.REVERSE:
            reverse = True
        elif self.options.direction_mode == DirectionMode.BIDIRECTIONAL:
            reverse = velocity < 0.0

        # Effective heading (flip by pi when driving in reverse)
        effective_heading = heading + math.pi if reverse else heading
        effective_heading = self._normalize_angle(effective_heading)

        # Closest point on path
        closest_pt, closest_s, closest_idx = self.path.closest_point(
            position, search_start=self._last_closest_index
        )
        self._last_closest_index = max(0, min(closest_idx, self.path.n_points - 2))

        # Cross-track error
        cte_vec = position - closest_pt
        cte = float(np.linalg.norm(cte_vec))

        # Heading error
        tangent = self.path.tangent_at(closest_idx)
        path_heading = math.atan2(tangent[1], tangent[0]) if self.path.dim >= 2 else 0.0
        heading_error = self._normalize_angle(path_heading - effective_heading)

        # Compute adaptive lookahead distance
        ld = self._compute_lookahead(cte, velocity)

        # Check if we are done
        dist_to_goal = max(0.0, self.path.total_length - closest_s)
        is_done = dist_to_goal <= self.options.done_tolerance and cte <= self.options.done_tolerance

        # If very close to the end, snap to end point
        if dist_to_goal <= ld:
            target = self.path.end_point()
            ld = dist_to_goal
        else:
            target, found = self.path.lookahead_point(position, closest_s, ld)
            if not found:
                empty = np.empty(self.path.dim, dtype=np.float64)
                return TrackingResult(
                    success=False,
                    lookahead_point=empty,
                    curvature=0.0,
                    steering_angle=0.0,
                    cross_track_error=cte,
                    heading_error=heading_error,
                    closest_point=closest_pt,
                    closest_index=closest_idx,
                    lookahead_distance=ld,
                    distance_to_goal=dist_to_goal,
                    is_done=is_done,
                    reverse=reverse,
                )

        # Curvature from pure-pursuit geometry
        curvature = self._compute_curvature(position, effective_heading, target, reverse)

        # Clamp curvature
        curvature = self._clamp_curvature(curvature)

        # Steering angle
        steering_angle = self._curvature_to_steering(curvature)

        return TrackingResult(
            success=True,
            lookahead_point=target,
            curvature=curvature,
            steering_angle=steering_angle,
            cross_track_error=cte,
            heading_error=heading_error,
            closest_point=closest_pt,
            closest_index=closest_idx,
            lookahead_distance=ld,
            distance_to_goal=dist_to_goal,
            is_done=is_done,
            reverse=reverse,
        )

    def reset(self, path_index: Optional[int] = None) -> None:
        """Reset the internal search index.

        Parameters
        ----------
        path_index : int | None
            Segment index to start from on the next update.  ``None`` resets to
            the value stored in ``options.initial_path_index``.
        """
        if path_index is None:
            path_index = self.options.initial_path_index
        self._last_closest_index = max(0, min(path_index, self.path.n_points - 2))

    def _compute_lookahead(self, cross_track_error: float, velocity: float) -> float:
        mode = self.options.lookahead_mode
        base = self.options.lookahead_distance
        if mode == LookaheadMode.FIXED:
            return base
        if mode == LookaheadMode.ADAPTIVE:
            return base + self.options.adaptive_gain * cross_track_error
        if mode == LookaheadMode.VELOCITY_SCALED:
            return base + self.options.velocity_scale_factor * abs(velocity)
        return base

    def _compute_curvature(
        self,
        position: np.ndarray,
        effective_heading: float,
        target: np.ndarray,
        reverse: bool,
    ) -> float:
        # Vector from vehicle to target in world frame
        dx = target[0] - position[0]
        dy = target[1] - position[1]
        if self.path.dim >= 2:
            # Rotate into vehicle frame
            sin_h = math.sin(effective_heading)
            cos_h = math.cos(effective_heading)
            local_x = dx * cos_h + dy * sin_h
            local_y = -dx * sin_h + dy * cos_h
        else:
            local_x = dx
            local_y = 0.0

        denom = local_x**2 + local_y**2
        if denom < 1e-18:
            return 0.0

        # Pure pursuit curvature: 2 * y / L^2
        curvature = 2.0 * local_y / denom
        if reverse:
            curvature = -curvature
        return curvature

    def _clamp_curvature(self, curvature: float) -> float:
        max_c = self.options.max_curvature
        if max_c is None and self.options.max_steering_angle is not None and self.options.wheelbase > 0.0:
            max_c = math.tan(self.options.max_steering_angle) / self.options.wheelbase
        if max_c is not None:
            curvature = max(-max_c, min(max_c, curvature))
        return curvature

    def _curvature_to_steering(self, curvature: float) -> float:
        wb = self.options.wheelbase
        if wb > 1e-9:
            delta = math.atan(curvature * wb)
            if self.options.max_steering_angle is not None:
                delta = max(
                    -self.options.max_steering_angle,
                    min(self.options.max_steering_angle, delta),
                )
            return delta
        return 0.0

    @staticmethod
    def _normalize_angle(angle: float) -> float:
        """Wrap ``angle`` to ``[-pi, pi]``."""
        while angle > math.pi:
            angle -= 2.0 * math.pi
        while angle < -math.pi:
            angle += 2.0 * math.pi
        return angle


def bicycle_steering_to_curvature(steering_angle: float, wheelbase: float) -> float:
    """Convert steering angle to curvature for a bicycle model.

    Parameters
    ----------
    steering_angle : float
        Steering angle in radians.
    wheelbase : float
        Wheelbase length (must be > 0).

    Returns
    -------
    float
        Curvature (1 / turning radius).
    """
    if wheelbase <= 0.0:
        raise ValueError("wheelbase must be positive")
    return math.tan(steering_angle) / wheelbase


def curvature_to_bicycle_steering(curvature: float, wheelbase: float) -> float:
    """Convert curvature to steering angle for a bicycle model.

    Parameters
    ----------
    curvature : float
        Path curvature.
    wheelbase : float
        Wheelbase length (must be > 0).

    Returns
    -------
    float
        Steering angle in radians.
    """
    if wheelbase <= 0.0:
        raise ValueError("wheelbase must be positive")
    return math.atan(curvature * wheelbase)
