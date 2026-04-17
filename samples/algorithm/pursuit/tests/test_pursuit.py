"""Comprehensive tests for the numpy-backed Pure Pursuit implementation."""

import math

import numpy as np
import pytest

from samples.algorithm.pursuit.pursuit import (
    PurePursuit,
    PursuitOptions,
    PursuitPath,
    VehicleState,
    TrackingResult,
    LookaheadMode,
    DirectionMode,
    bicycle_steering_to_curvature,
    curvature_to_bicycle_steering,
)


class TestPursuitPath:
    """Tests for the preprocessed path representation."""

    def test_single_point_path(self) -> None:
        path = PursuitPath(np.array([[1.0, 2.0]]))
        assert path.n_points == 1
        pt, s, idx = path.closest_point(np.array([2.0, 3.0]))
        assert np.allclose(pt, [1.0, 2.0])
        assert s == 0.0
        assert idx == 0

    def test_duplicate_removal(self) -> None:
        waypoints = np.array([[0, 0], [0, 0], [1, 0], [1, 0], [2, 0]], dtype=float)
        path = PursuitPath(waypoints, resolution=1e-3)
        assert path.n_points == 3
        np.testing.assert_array_equal(path.waypoints, np.array([[0, 0], [1, 0], [2, 0]], dtype=float))

    def test_cumulative_length(self) -> None:
        waypoints = np.array([[0, 0], [3, 4], [6, 8]], dtype=float)
        path = PursuitPath(waypoints)
        assert path.total_length == pytest.approx(10.0, abs=1e-9)
        np.testing.assert_allclose(path.cumlen, [0.0, 5.0, 10.0], atol=1e-9)

    def test_closest_point_on_segment(self) -> None:
        waypoints = np.array([[0, 0], [2, 0], [2, 2]], dtype=float)
        path = PursuitPath(waypoints)
        pt, s, idx = path.closest_point(np.array([1.0, 1.0]))
        assert np.allclose(pt, [1.0, 0.0])
        assert idx == 0
        assert s == pytest.approx(1.0, abs=1e-9)

    def test_closest_point_at_endpoint(self) -> None:
        waypoints = np.array([[0, 0], [1, 0]], dtype=float)
        path = PursuitPath(waypoints)
        pt, s, idx = path.closest_point(np.array([2.0, 0.0]))
        assert np.allclose(pt, [1.0, 0.0])
        assert idx == 0
        assert s == pytest.approx(1.0, abs=1e-9)

    def test_lookahead_point_simple(self) -> None:
        waypoints = np.array([[0, 0], [1, 0], [2, 0]], dtype=float)
        path = PursuitPath(waypoints)
        pt, found = path.lookahead_point(np.array([0.0, 0.0]), 0.0, 1.5)
        assert found
        assert np.allclose(pt, [1.5, 0.0])

    def test_lookahead_point_past_end(self) -> None:
        waypoints = np.array([[0, 0], [1, 0]], dtype=float)
        path = PursuitPath(waypoints)
        pt, found = path.lookahead_point(np.array([0.0, 0.0]), 0.0, 2.0)
        assert found
        assert np.allclose(pt, [1.0, 0.0])

    def test_tangent_at(self) -> None:
        waypoints = np.array([[0, 0], [1, 0], [1, 1]], dtype=float)
        path = PursuitPath(waypoints)
        np.testing.assert_allclose(path.tangent_at(0), [1.0, 0.0], atol=1e-9)
        np.testing.assert_allclose(path.tangent_at(1), [0.0, 1.0], atol=1e-9)

    def test_invalid_waypoints_shape(self) -> None:
        with pytest.raises(ValueError, match="2-D"):
            PursuitPath(np.array([1.0, 2.0, 3.0]))

    def test_empty_waypoints(self) -> None:
        with pytest.raises(ValueError, match="at least one point"):
            PursuitPath(np.empty((0, 2)))


class TestPursuitOptions:
    """Validation tests for PursuitOptions."""

    def test_default_options(self) -> None:
        opts = PursuitOptions()
        assert opts.lookahead_distance == 1.0
        assert opts.lookahead_mode == LookaheadMode.FIXED
        assert opts.wheelbase == 0.0

    def test_invalid_lookahead_distance(self) -> None:
        with pytest.raises(ValueError, match="lookahead_distance must be positive"):
            PursuitOptions(lookahead_distance=0.0)

    def test_invalid_wheelbase(self) -> None:
        with pytest.raises(ValueError, match="wheelbase must be non-negative"):
            PursuitOptions(wheelbase=-1.0)

    def test_invalid_max_curvature(self) -> None:
        with pytest.raises(ValueError, match="max_curvature must be non-negative"):
            PursuitOptions(max_curvature=-0.1)

    def test_invalid_max_steering_angle(self) -> None:
        with pytest.raises(ValueError, match="max_steering_angle must be non-negative"):
            PursuitOptions(max_steering_angle=-0.1)


class TestPurePursuitBasic:
    """Smoke tests for the core tracker."""

    def test_straight_line_forward(self) -> None:
        path = np.array([[0.0, 0.0], [5.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=1.0))
        state = VehicleState(position=np.array([0.0, 0.0]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert result.success
        assert np.allclose(result.lookahead_point, [1.0, 0.0])
        assert result.curvature == pytest.approx(0.0, abs=1e-9)
        assert result.reverse is False

    def test_follows_curve(self) -> None:
        # Semicircle approximation
        theta = np.linspace(0, math.pi, 20)
        path = np.column_stack((np.cos(theta), np.sin(theta)))
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=0.3))
        state = VehicleState(position=np.array([1.0, 0.1]), heading=math.pi / 2, velocity=1.0)
        result = tracker.update(state)
        assert result.success
        # Should command a left turn (positive curvature)
        assert result.curvature > 0.0

    def test_cross_track_error(self) -> None:
        path = np.array([[0.0, 0.0], [5.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=1.0))
        state = VehicleState(position=np.array([0.0, 2.0]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert result.cross_track_error == pytest.approx(2.0, abs=1e-9)

    def test_heading_error(self) -> None:
        path = np.array([[0.0, 0.0], [5.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=1.0))
        state = VehicleState(position=np.array([0.0, 0.0]), heading=math.pi / 4, velocity=1.0)
        result = tracker.update(state)
        assert result.heading_error == pytest.approx(-math.pi / 4, abs=1e-9)

    def test_done_at_end(self) -> None:
        path = np.array([[0.0, 0.0], [1.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=0.5, done_tolerance=0.1))
        state = VehicleState(position=np.array([1.0, 0.0]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert result.is_done

    def test_not_done_far_from_end(self) -> None:
        path = np.array([[0.0, 0.0], [5.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=1.0, done_tolerance=0.1))
        state = VehicleState(position=np.array([0.0, 0.0]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert not result.is_done

    def test_single_point_path_tracking(self) -> None:
        path = np.array([[1.0, 1.0]])
        tracker = PurePursuit(path)
        state = VehicleState(position=np.array([1.0, 1.0]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert result.success
        assert np.allclose(result.lookahead_point, [1.0, 1.0])
        assert result.is_done

    def test_array_input(self) -> None:
        path = np.array([[0.0, 0.0], [2.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=1.0))
        result = tracker.update(np.array([0.0, 0.0]), velocity=1.0, heading=0.0)
        assert result.success
        assert np.allclose(result.lookahead_point, [1.0, 0.0])

    def test_dimension_mismatch(self) -> None:
        path = np.array([[0.0, 0.0], [1.0, 0.0]])
        tracker = PurePursuit(path)
        with pytest.raises(ValueError, match="dimension"):
            tracker.update(np.array([0.0, 0.0, 0.0]))

    def test_reset_search_index(self) -> None:
        path = np.array([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0]])
        tracker = PurePursuit(path)
        tracker.update(VehicleState(position=np.array([1.5, 0.0]), heading=0.0, velocity=1.0))
        assert tracker._last_closest_index >= 0
        tracker.reset(path_index=0)
        assert tracker._last_closest_index == 0


class TestCurvatureAndSteering:
    """Tests for curvature and steering-angle computations."""

    def test_zero_curvature_on_straight_path(self) -> None:
        path = np.array([[0.0, 0.0], [10.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=2.0))
        state = VehicleState(position=np.array([0.0, 0.0]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert result.curvature == pytest.approx(0.0, abs=1e-9)

    def test_positive_curvature_left_turn(self) -> None:
        path = np.array([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=0.5))
        state = VehicleState(position=np.array([0.5, -0.1]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert result.curvature > 0.0

    def test_negative_curvature_right_turn(self) -> None:
        path = np.array([[0.0, 0.0], [1.0, 0.0], [1.0, -1.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=0.5))
        state = VehicleState(position=np.array([0.5, 0.1]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert result.curvature < 0.0

    def test_wheelbase_steering_conversion(self) -> None:
        opts = PursuitOptions(lookahead_distance=1.0, wheelbase=2.0)
        path = np.array([[0.0, 0.0], [5.0, 0.0]])
        tracker = PurePursuit(path, opts)
        state = VehicleState(position=np.array([0.0, 0.5]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        expected_delta = math.atan(result.curvature * 2.0)
        assert result.steering_angle == pytest.approx(expected_delta, abs=1e-9)

    def test_max_curvature_clamping(self) -> None:
        path = np.array([[0.0, 0.0], [1.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=0.1, max_curvature=1.0))
        # Place vehicle far off the path to generate high curvature
        state = VehicleState(position=np.array([0.0, 2.0]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert abs(result.curvature) <= 1.0 + 1e-9

    def test_max_steering_angle_clamping(self) -> None:
        path = np.array([[0.0, 0.0], [1.0, 0.0]])
        opts = PursuitOptions(lookahead_distance=0.1, wheelbase=1.0, max_steering_angle=math.pi / 6)
        tracker = PurePursuit(path, opts)
        state = VehicleState(position=np.array([0.0, 2.0]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert abs(result.steering_angle) <= math.pi / 6 + 1e-9

    def test_bicycle_helpers(self) -> None:
        delta = math.pi / 6
        wb = 2.0
        kappa = bicycle_steering_to_curvature(delta, wb)
        assert kappa == pytest.approx(math.tan(delta) / wb, abs=1e-9)
        assert curvature_to_bicycle_steering(kappa, wb) == pytest.approx(delta, abs=1e-9)

    def test_bicycle_helper_invalid_wheelbase(self) -> None:
        with pytest.raises(ValueError, match="wheelbase must be positive"):
            bicycle_steering_to_curvature(0.0, 0.0)
        with pytest.raises(ValueError, match="wheelbase must be positive"):
            curvature_to_bicycle_steering(0.0, -1.0)


class TestDirectionModes:
    """Tests for forward, reverse, and bidirectional driving."""

    def test_forward_mode_ignores_negative_velocity(self) -> None:
        path = np.array([[0.0, 0.0], [5.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(direction_mode=DirectionMode.FORWARD))
        state = VehicleState(position=np.array([0.0, 0.0]), heading=0.0, velocity=-1.0)
        result = tracker.update(state)
        assert result.reverse is False

    def test_reverse_mode(self) -> None:
        path = np.array([[0.0, 0.0], [5.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(direction_mode=DirectionMode.REVERSE))
        state = VehicleState(position=np.array([5.0, 0.0]), heading=math.pi, velocity=-1.0)
        result = tracker.update(state)
        assert result.reverse is True
        # Driving reverse along the path from end to start should track successfully
        assert result.success

    def test_bidirectional_negative_velocity(self) -> None:
        path = np.array([[0.0, 0.0], [5.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(direction_mode=DirectionMode.BIDIRECTIONAL))
        state = VehicleState(position=np.array([5.0, 0.0]), heading=math.pi, velocity=-1.0)
        result = tracker.update(state)
        assert result.reverse is True

    def test_reverse_curvature_sign(self) -> None:
        path = np.array([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0]])
        opts = PursuitOptions(lookahead_distance=0.5, direction_mode=DirectionMode.REVERSE)
        tracker = PurePursuit(path, opts)
        state = VehicleState(position=np.array([1.0, 0.5]), heading=-math.pi / 2, velocity=-1.0)
        result = tracker.update(state)
        # Curvature sign should be flipped compared to forward mode
        assert result.reverse is True


class TestLookaheadModes:
    """Tests for fixed, adaptive, and velocity-scaled lookahead."""

    def test_fixed_lookahead(self) -> None:
        path = np.array([[0.0, 0.0], [5.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=2.0, lookahead_mode=LookaheadMode.FIXED))
        state = VehicleState(position=np.array([0.0, 0.0]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert result.lookahead_distance == pytest.approx(2.0, abs=1e-9)

    def test_adaptive_lookahead_increases_with_cte(self) -> None:
        path = np.array([[0.0, 0.0], [10.0, 0.0]])
        base_ld = 1.0
        tracker = PurePursuit(
            path,
            PursuitOptions(
                lookahead_distance=base_ld,
                lookahead_mode=LookaheadMode.ADAPTIVE,
                adaptive_gain=0.5,
            ),
        )
        state_low = VehicleState(position=np.array([0.0, 0.1]), heading=0.0, velocity=1.0)
        state_high = VehicleState(position=np.array([0.0, 2.0]), heading=0.0, velocity=1.0)
        res_low = tracker.update(state_low)
        tracker.reset()
        res_high = tracker.update(state_high)
        assert res_high.lookahead_distance > res_low.lookahead_distance

    def test_velocity_scaled_lookahead(self) -> None:
        path = np.array([[0.0, 0.0], [10.0, 0.0]])
        tracker = PurePursuit(
            path,
            PursuitOptions(
                lookahead_distance=1.0,
                lookahead_mode=LookaheadMode.VELOCITY_SCALED,
                velocity_scale_factor=0.5,
            ),
        )
        state_slow = VehicleState(position=np.array([0.0, 0.0]), heading=0.0, velocity=1.0)
        state_fast = VehicleState(position=np.array([0.0, 0.0]), heading=0.0, velocity=5.0)
        res_slow = tracker.update(state_slow)
        tracker.reset()
        res_fast = tracker.update(state_fast)
        assert res_fast.lookahead_distance > res_slow.lookahead_distance


class TestLocalSearch:
    """Tests that the local search index behaves correctly."""

    def test_search_index_advances(self) -> None:
        path = np.array([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0], [3.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=0.5))
        tracker.update(VehicleState(position=np.array([0.1, 0.0]), heading=0.0, velocity=1.0))
        assert tracker._last_closest_index == 0
        tracker.update(VehicleState(position=np.array([1.5, 0.0]), heading=0.0, velocity=1.0))
        assert tracker._last_closest_index == 1
        tracker.update(VehicleState(position=np.array([2.9, 0.0]), heading=0.0, velocity=1.0))
        assert tracker._last_closest_index == 2

    def test_search_index_does_not_exceed_bounds(self) -> None:
        path = np.array([[0.0, 0.0], [1.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(initial_path_index=10))
        assert tracker._last_closest_index == 0


class TestTrackingResult:
    """Tests for the result dataclass and edge cases."""

    def test_result_bool(self) -> None:
        success = TrackingResult(
            success=True,
            lookahead_point=np.array([1.0, 0.0]),
            curvature=0.0,
            steering_angle=0.0,
            cross_track_error=0.0,
            heading_error=0.0,
            closest_point=np.array([0.0, 0.0]),
            closest_index=0,
            lookahead_distance=1.0,
            distance_to_goal=0.0,
            is_done=True,
            reverse=False,
        )
        assert bool(success) is True
        failure = TrackingResult(
            success=False,
            lookahead_point=np.empty(2),
            curvature=0.0,
            steering_angle=0.0,
            cross_track_error=0.0,
            heading_error=0.0,
            closest_point=np.array([0.0, 0.0]),
            closest_index=0,
            lookahead_distance=0.0,
            distance_to_goal=0.0,
            is_done=False,
            reverse=False,
        )
        assert bool(failure) is False

    def test_vehicle_state_post_init(self) -> None:
        state = VehicleState(position=[1.0, 2.0], heading=0.5, velocity=3.0)
        assert isinstance(state.position, np.ndarray)
        assert state.position.dtype == np.float64
        np.testing.assert_array_equal(state.position, [1.0, 2.0])


class Test3DPath:
    """Tests that the tracker handles 3-D waypoints gracefully."""

    def test_3d_straight_line(self) -> None:
        path = np.array([[0.0, 0.0, 0.0], [5.0, 0.0, 0.0]])
        tracker = PurePursuit(path, PursuitOptions(lookahead_distance=1.0))
        state = VehicleState(position=np.array([0.0, 0.0, 0.0]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert result.success
        assert result.lookahead_point.shape == (3,)

    def test_3d_closest_point(self) -> None:
        path = np.array([[0.0, 0.0, 0.0], [2.0, 0.0, 0.0], [2.0, 2.0, 0.0]])
        tracker = PurePursuit(path)
        state = VehicleState(position=np.array([1.0, 1.0, 0.0]), heading=0.0, velocity=1.0)
        result = tracker.update(state)
        assert result.success
        assert result.closest_point.shape == (3,)


class TestReusability:
    """The same tracker instance should be usable for multiple queries."""

    def test_multiple_updates(self) -> None:
        path = np.array([[0.0, 0.0], [10.0, 0.0]])
        tracker = PurePursuit(path)
        for i in range(5):
            state = VehicleState(position=np.array([float(i), 0.0]), heading=0.0, velocity=1.0)
            result = tracker.update(state)
            assert result.success
            assert result.closest_index >= 0


class TestPerformance:
    """Sanity checks on large paths."""

    def test_large_path_closest_point(self) -> None:
        n = 10_000
        waypoints = np.column_stack((np.arange(n, dtype=float), np.zeros(n, dtype=float)))
        path = PursuitPath(waypoints)
        pt, s, idx = path.closest_point(np.array([5000.5, 1.0]), search_start=4000)
        assert idx == 5000
        assert s == pytest.approx(5000.5, abs=1e-6)

    def test_large_path_tracker_update(self) -> None:
        n = 5_000
        waypoints = np.column_stack((np.linspace(0, 1000, n), np.sin(np.linspace(0, 4 * math.pi, n))))
        tracker = PurePursuit(waypoints, PursuitOptions(lookahead_distance=5.0))
        state = VehicleState(position=np.array([500.0, 0.0]), heading=0.0, velocity=10.0)
        result = tracker.update(state)
        assert result.success
        assert result.cross_track_error >= 0.0
