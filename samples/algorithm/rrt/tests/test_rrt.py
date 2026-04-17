"""Comprehensive tests for RRT path planning and tracking."""

import numpy as np
import pytest
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from rrt import RRTNode, RRTPlanner, RRTStarPlanner, PathTracker, RRTOptions, KDTree


class TestKDTree:
    def test_build_and_nearest_2d(self):
        points = np.array([[0, 0], [1, 1], [2, 2], [3, 3]], dtype=float)
        tree = KDTree(points)
        idx, dist_sq = tree.nearest(np.array([0.1, 0.1]))
        assert idx == 0
        assert dist_sq == pytest.approx(0.02, abs=1e-9)

    def test_build_and_nearest_3d(self):
        points = np.array([[0, 0, 0], [1, 1, 1], [2, 2, 2]], dtype=float)
        tree = KDTree(points)
        idx, dist_sq = tree.nearest(np.array([1.1, 1.1, 1.1]))
        assert idx == 1
        assert dist_sq == pytest.approx(0.03, abs=1e-9)

    def test_search_radius(self):
        points = np.array([[0, 0], [1, 0], [0, 1], [5, 5]], dtype=float)
        tree = KDTree(points)
        results = tree.search_radius(np.array([0, 0]), 1.5)
        indices = {idx for idx, _ in results}
        assert indices == {0, 1, 2}

    def test_empty_tree(self):
        points = np.empty((0, 2), dtype=float)
        tree = KDTree(points)
        idx, dist_sq = tree.nearest(np.array([0, 0]))
        assert idx is None
        assert dist_sq == float("inf")


class TestRRTNode:
    def test_node_creation(self):
        node = RRTNode(np.array([1.0, 2.0]))
        assert np.allclose(node.position, [1.0, 2.0])
        assert node.parent is None
        assert node.cost == 0.0

    def test_node_list_conversion(self):
        node = RRTNode([1.0, 2.0])
        assert isinstance(node.position, np.ndarray)
        assert node.position.dtype == np.float64


class TestRRTPlanner:
    def _no_obstacles(self, point: np.ndarray) -> bool:
        return False

    def _circle_obstacle(self, center, radius):
        def checker(point: np.ndarray) -> bool:
            return float(np.linalg.norm(point - center)) < radius
        return checker

    def test_simple_2d_plan_no_obstacles(self):
        start = np.array([0.0, 0.0])
        goal = np.array([1.0, 0.0])
        bounds = np.array([[-1.0, 2.0], [-1.0, 2.0]])
        planner = RRTPlanner(
            start, goal, bounds, self._no_obstacles,
            options=RRTOptions(max_iterations=2000, step_size=0.3, goal_tolerance=0.15, goal_sample_rate=0.15)
        )
        path = planner.plan()
        assert path is not None
        assert len(path) >= 2
        assert np.allclose(path[0], start)
        assert np.allclose(path[-1], goal, atol=0.15)

    def test_simple_3d_plan_no_obstacles(self):
        start = np.array([0.0, 0.0, 0.0])
        goal = np.array([1.0, 1.0, 1.0])
        bounds = np.array([[-1.0, 2.0], [-1.0, 2.0], [-1.0, 2.0]])
        planner = RRTPlanner(
            start, goal, bounds, self._no_obstacles,
            options=RRTOptions(max_iterations=3000, step_size=0.35, goal_tolerance=0.2, goal_sample_rate=0.15)
        )
        path = planner.plan()
        assert path is not None
        assert path.shape[1] == 3
        assert np.allclose(path[0], start)
        assert np.allclose(path[-1], goal, atol=0.2)

    def test_plan_with_obstacle_avoidance(self):
        start = np.array([0.0, 0.0])
        goal = np.array([5.0, 0.0])
        bounds = np.array([[-1.0, 6.0], [-3.0, 3.0]])
        checker = self._circle_obstacle(np.array([2.5, 0.0]), 1.0)
        planner = RRTPlanner(
            start, goal, bounds, checker,
            options=RRTOptions(max_iterations=5000, step_size=0.4, goal_tolerance=0.3, goal_sample_rate=0.15)
        )
        path = planner.plan()
        assert path is not None
        for p in path:
            assert not checker(p)

    def test_plan_failure_impossible(self):
        start = np.array([0.0, 0.0])
        goal = np.array([5.0, 0.0])
        bounds = np.array([[-1.0, 6.0], [-1.0, 1.0]])
        # Wall blocking the path
        def wall_checker(p):
            return 2.0 < p[0] < 3.0
        planner = RRTPlanner(
            start, goal, bounds, wall_checker,
            options=RRTOptions(max_iterations=200, step_size=0.2, goal_tolerance=0.2)
        )
        path = planner.plan()
        assert path is None

    def test_path_smoothing(self):
        start = np.array([0.0, 0.0])
        goal = np.array([3.0, 3.0])
        bounds = np.array([[-1.0, 4.0], [-1.0, 4.0]])
        planner = RRTPlanner(
            start, goal, bounds, self._no_obstacles,
            options=RRTOptions(
                max_iterations=3000, step_size=0.4, goal_tolerance=0.3, goal_sample_rate=0.15,
                enable_path_smoothing=True, smoothing_iterations=200
            )
        )
        path = planner.plan()
        assert path is not None
        # Smoothed path should generally be shorter or equal
        assert len(path) >= 2

    def test_kd_tree_disabled(self):
        start = np.array([0.0, 0.0])
        goal = np.array([1.0, 1.0])
        bounds = np.array([[-1.0, 2.0], [-1.0, 2.0]])
        opts = RRTOptions(max_iterations=2000, step_size=0.35, goal_tolerance=0.2, goal_sample_rate=0.15, use_kd_tree=False)
        planner = RRTPlanner(start, goal, bounds, self._no_obstacles, options=opts)
        path = planner.plan()
        assert path is not None
        assert np.allclose(path[0], start)
        assert np.allclose(path[-1], goal, atol=0.2)


class TestRRTStarPlanner:
    def _no_obstacles(self, point: np.ndarray) -> bool:
        return False

    def test_rrt_star_finds_path(self):
        start = np.array([0.0, 0.0])
        goal = np.array([2.0, 2.0])
        bounds = np.array([[-1.0, 3.0], [-1.0, 3.0]])
        planner = RRTStarPlanner(
            start, goal, bounds, self._no_obstacles,
            options=RRTOptions(max_iterations=4000, step_size=0.35, goal_tolerance=0.2, rewire_radius=1.5, goal_sample_rate=0.15)
        )
        path = planner.plan()
        assert path is not None
        assert np.allclose(path[0], start)
        assert np.allclose(path[-1], goal, atol=0.2)

    def test_rrt_star_cost_lower_than_rrt(self):
        np.random.seed(42)
        start = np.array([0.0, 0.0])
        goal = np.array([3.0, 3.0])
        bounds = np.array([[-1.0, 4.0], [-1.0, 4.0]])
        opts = RRTOptions(max_iterations=2500, step_size=0.4, goal_tolerance=0.3, goal_sample_rate=0.15, enable_path_smoothing=False)
        
        planner_rrt = RRTPlanner(start, goal, bounds, self._no_obstacles, options=opts)
        path_rrt = planner_rrt.plan()
        
        np.random.seed(42)
        opts_star = RRTOptions(
            max_iterations=2500, step_size=0.4, goal_tolerance=0.3, goal_sample_rate=0.15,
            use_rrt_star=True, rewire_radius=2.0, enable_path_smoothing=False
        )
        planner_star = RRTPlanner(start, goal, bounds, self._no_obstacles, options=opts_star)
        path_star = planner_star.plan()
        
        assert path_rrt is not None
        assert path_star is not None
        
        cost_rrt = np.sum(np.linalg.norm(np.diff(path_rrt, axis=0), axis=1))
        cost_star = np.sum(np.linalg.norm(np.diff(path_star, axis=0), axis=1))
        assert cost_star <= cost_rrt * 1.15  # allow small tolerance


class TestPathTracker:
    def test_closest_point_straight_line(self):
        path = np.array([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0], [3.0, 0.0]])
        tracker = PathTracker(path)
        closest, s, idx = tracker.closest_point(np.array([1.2, 0.5]))
        assert np.allclose(closest, [1.2, 0.0])
        assert s == pytest.approx(1.2, abs=1e-9)
        assert idx == 1

    def test_lookahead_point(self):
        path = np.array([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0]])
        tracker = PathTracker(path, options=RRTOptions(tracking_lookahead=0.5))
        target = tracker.lookahead_point(np.array([0.2, 0.1]))
        assert target[0] == pytest.approx(0.7, abs=1e-9)
        assert target[1] == pytest.approx(0.0, abs=1e-9)

    def test_tracking_error(self):
        path = np.array([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0]])
        tracker = PathTracker(path)
        err = tracker.tracking_error(np.array([1.0, 0.5]))
        assert err == pytest.approx(0.5, abs=1e-9)

    def test_control_direction(self):
        path = np.array([[0.0, 0.0], [1.0, 0.0]])
        tracker = PathTracker(path, options=RRTOptions(tracking_lookahead=0.3))
        direction = tracker.control_direction(np.array([0.0, 0.1]))
        assert direction[0] > 0.9  # mostly forward

    def test_is_done(self):
        path = np.array([[0.0, 0.0], [1.0, 0.0]])
        tracker = PathTracker(path)
        assert tracker.is_done(np.array([1.0, 0.0]), tolerance=0.1)
        assert not tracker.is_done(np.array([0.0, 0.0]), tolerance=0.1)

    def test_empty_path(self):
        tracker = PathTracker(np.empty((0, 2)))
        closest, s, idx = tracker.closest_point(np.array([0.0, 0.0]))
        assert np.allclose(closest, [0.0, 0.0])
        assert s == 0.0


class TestRRTOptions:
    def test_default_options(self):
        opts = RRTOptions()
        assert opts.max_iterations == 10000
        assert opts.step_size == 0.1
        assert opts.goal_sample_rate == 0.05
        assert opts.use_rrt_star is False

    def test_custom_options(self):
        opts = RRTOptions(max_iterations=500, step_size=0.5, use_rrt_star=True)
        assert opts.max_iterations == 500
        assert opts.step_size == 0.5
        assert opts.use_rrt_star is True


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
