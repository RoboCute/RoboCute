"""Comprehensive tests for the numpy-backed A* implementation."""

import math

import numpy as np
import pytest

from samples.algorithm.a_star.a_star import AStar, Heuristic, PathResult, SearchStats, TieBreaker


class TestBasicFunctionality:
    """Smoke tests for the core search routine."""

    def test_straight_line_4way(self) -> None:
        grid = np.ones((5, 5), dtype=np.float64)
        solver = AStar(grid, allow_diagonal=False)
        result = solver.search(start=(0, 0), goal=(0, 4))
        assert result.success
        expected = np.array([[0, 0], [0, 1], [0, 2], [0, 3], [0, 4]], dtype=np.int32)
        np.testing.assert_array_equal(result.path, expected)
        assert math.isclose(result.cost, 4.0)

    def test_straight_line_8way(self) -> None:
        grid = np.ones((5, 5), dtype=np.float64)
        solver = AStar(grid, allow_diagonal=True)
        result = solver.search(start=(0, 0), goal=(4, 4))
        assert result.success
        # Diagonal should give exactly 4 diagonal steps
        assert len(result.path) == 5
        assert math.isclose(result.cost, 4.0 * math.sqrt(2.0), rel_tol=1e-9)

    def test_simple_obstacle_avoidance(self) -> None:
        grid = np.ones((5, 5), dtype=np.float64)
        grid[1:4, 2] = 0.0  # vertical wall in the middle
        solver = AStar(grid, allow_diagonal=False)
        result = solver.search(start=(1, 1), goal=(1, 3))
        assert result.success
        # Path must go around the wall (either over or under)
        assert all(grid[r, c] > 0.0 for r, c in result.path)
        assert tuple(result.path[0]) == (1, 1)
        assert tuple(result.path[-1]) == (1, 3)

    def test_no_path_blocked_goal(self) -> None:
        grid = np.ones((3, 3), dtype=np.float64)
        grid[1, 1] = 0.0
        solver = AStar(grid)
        result = solver.search(start=(0, 0), goal=(1, 1))
        assert not result.success
        assert result.path.size == 0
        assert math.isinf(result.cost)

    def test_no_path_isolated(self) -> None:
        grid = np.ones((3, 3), dtype=np.float64)
        grid[0, 1] = 0.0
        grid[1, 0] = 0.0
        solver = AStar(grid, allow_diagonal=False)
        result = solver.search(start=(0, 0), goal=(2, 2))
        assert not result.success

    def test_start_equals_goal(self) -> None:
        grid = np.ones((3, 3), dtype=np.float64)
        solver = AStar(grid)
        result = solver.search(start=(1, 1), goal=(1, 1))
        assert result.success
        np.testing.assert_array_equal(result.path, np.array([[1, 1]], dtype=np.int32))
        assert result.cost == 0.0


class TestHeuristics:
    """Ensure every built-in heuristic can produce a valid path."""

    @pytest.mark.parametrize(
        "heuristic",
        [
            Heuristic.MANHATTAN,
            Heuristic.EUCLIDEAN,
            Heuristic.CHEBYSHEV,
            Heuristic.OCTILE,
            Heuristic.DIAGONAL,
            Heuristic.ZERO,
        ],
    )
    def test_all_heuristics_find_path(self, heuristic: Heuristic) -> None:
        grid = np.ones((7, 7), dtype=np.float64)
        grid[2:5, 3] = 0.0
        solver = AStar(grid, heuristic=heuristic, allow_diagonal=True)
        result = solver.search(start=(1, 1), goal=(5, 5))
        assert result.success
        assert tuple(result.path[0]) == (1, 1)
        assert tuple(result.path[-1]) == (5, 5)

    def test_custom_heuristic(self) -> None:
        grid = np.ones((5, 5), dtype=np.float64)
        called = []

        def custom_h(a, b):
            called.append((a, b))
            return abs(a[0] - b[0]) + abs(a[1] - b[1])

        solver = AStar(grid, heuristic=custom_h, allow_diagonal=False)
        result = solver.search(start=(0, 0), goal=(4, 4))
        assert result.success
        assert len(called) > 0


class TestTieBreakers:
    """Tie-breaking should not break correctness."""

    @pytest.mark.parametrize(
        "tie_breaker",
        [
            TieBreaker.NONE,
            TieBreaker.CROSS_PRODUCT,
            TieBreaker.SMALLER_G,
            TieBreaker.PREFER_STRAIGHT,
        ],
    )
    def test_tie_breakers(self, tie_breaker: TieBreaker) -> None:
        grid = np.ones((10, 10), dtype=np.float64)
        grid[4, 1:9] = 0.0
        solver = AStar(grid, tie_breaker=tie_breaker, allow_diagonal=True)
        result = solver.search(start=(2, 2), goal=(7, 7))
        assert result.success
        assert tuple(result.path[0]) == (2, 2)
        assert tuple(result.path[-1]) == (7, 7)


class TestWeightedGrid:
    """Grids with cell-cost multipliers."""

    def test_weighted_grid_prefers_cheaper_cells(self) -> None:
        grid = np.ones((3, 5), dtype=np.float64)
        grid[1, 1:4] = 5.0  # expensive middle row
        solver = AStar(grid, allow_diagonal=False)
        result = solver.search(start=(1, 0), goal=(1, 4))
        assert result.success
        # Path should go above or below the expensive strip
        rows = result.path[:, 0]
        assert not np.all(rows == 1)

    def test_weighted_diagonal_avoids_expensive_cell(self) -> None:
        grid = np.ones((5, 5), dtype=np.float64)
        grid[2, 2] = 3.0
        solver = AStar(grid, allow_diagonal=True)
        result = solver.search(start=(0, 0), goal=(4, 4))
        assert result.success
        # Path should avoid the expensive cell (2, 2)
        assert not any((r, c) == (2, 2) for r, c in result.path)
        # Actual shortest path around: 3 diagonals + 2 cardinals
        expected = 3.0 * math.sqrt(2.0) + 2.0
        assert math.isclose(result.cost, expected, rel_tol=1e-9)


class TestWeightedAStar:
    """w > 1 trades optimality for speed."""

    def test_weighted_a_star_faster(self) -> None:
        size = 100
        grid = np.ones((size, size), dtype=np.float64)
        # Build a maze-like wall
        grid[20:80, 50] = 0.0
        grid[20, 20:50] = 0.0
        grid[79, 50:80] = 0.0

        solver_opt = AStar(grid, weight=1.0)
        solver_fast = AStar(grid, weight=2.5)

        res_opt = solver_opt.search(start=(10, 10), goal=(90, 90))
        res_fast = solver_fast.search(start=(10, 10), goal=(90, 90))

        assert res_opt.success
        assert res_fast.success
        # Weighted A* should generally expand fewer or equal nodes
        assert res_fast.stats.nodes_expanded <= res_opt.stats.nodes_expanded * 1.5 + 10


class TestSmoothing:
    """Path smoothing post-processing."""

    def test_smooth_reduces_waypoints(self) -> None:
        grid = np.ones((5, 5), dtype=np.float64)
        grid[1:4, 2] = 0.0  # wall in middle
        solver = AStar(grid, allow_diagonal=False)
        raw = solver.search(start=(1, 1), goal=(1, 3), smooth=False)
        smooth = solver.search(start=(1, 1), goal=(1, 3), smooth=True)
        assert raw.success and smooth.success
        assert len(smooth.path) <= len(raw.path)

    def test_smooth_preserves_validity(self) -> None:
        grid = np.ones((10, 10), dtype=np.float64)
        grid[4, 2:8] = 0.0
        solver = AStar(grid, allow_diagonal=True)
        smooth = solver.search(start=(2, 2), goal=(7, 7), smooth=True)
        assert smooth.success
        for r, c in smooth.path:
            assert grid[r, c] > 0.0


class TestConnectivity:
    """4-way vs 8-way movement."""

    def test_diagonal_not_allowed(self) -> None:
        grid = np.ones((3, 3), dtype=np.float64)
        solver = AStar(grid, allow_diagonal=False)
        result = solver.search(start=(0, 0), goal=(2, 2))
        assert result.success
        # Manhattan distance = 4 steps
        assert len(result.path) == 5
        assert math.isclose(result.cost, 4.0)

    def test_diagonal_allowed(self) -> None:
        grid = np.ones((3, 3), dtype=np.float64)
        solver = AStar(grid, allow_diagonal=True)
        result = solver.search(start=(0, 0), goal=(2, 2))
        assert result.success
        assert len(result.path) == 3
        assert math.isclose(result.cost, 2.0 * math.sqrt(2.0), rel_tol=1e-9)

    def test_diagonal_corner_cutting_blocked(self) -> None:
        grid = np.ones((2, 2), dtype=np.float64)
        grid[0, 1] = 0.0
        grid[1, 0] = 0.0
        solver = AStar(grid, allow_diagonal=True)
        result = solver.search(start=(0, 0), goal=(1, 1))
        assert not result.success


class TestTimeLimit:
    """Search should respect time limits."""

    def test_time_limit_may_fail(self) -> None:
        size = 400
        grid = np.ones((size, size), dtype=np.float64)
        # Make a long corridor to force lots of exploration
        grid[:, 100] = 0.0
        grid[:, 300] = 0.0
        grid[200, :] = 0.0
        grid[0, :] = 0.0
        grid[-1, :] = 0.0

        solver = AStar(grid, time_limit_ms=0.001)
        result = solver.search(start=(1, 1), goal=(size - 2, 250))
        # With an extremely tight time limit it should usually time out.
        # We only assert that it does not crash and respects the limit.
        assert result.stats.runtime_ms < 10.0  # generous upper bound


class TestEdgeCases:
    """Boundary conditions and invalid inputs."""

    def test_out_of_bounds_start_raises(self) -> None:
        grid = np.ones((3, 3), dtype=np.float64)
        solver = AStar(grid)
        with pytest.raises(ValueError, match="out of bounds"):
            solver.search(start=(-1, 0), goal=(2, 2))

    def test_out_of_bounds_goal_raises(self) -> None:
        grid = np.ones((3, 3), dtype=np.float64)
        solver = AStar(grid)
        with pytest.raises(ValueError, match="out of bounds"):
            solver.search(start=(0, 0), goal=(3, 2))

    def test_non_2d_grid_raises(self) -> None:
        with pytest.raises(ValueError, match="2-D"):
            AStar(np.ones((3, 3, 3)))

    def test_single_cell_grid(self) -> None:
        grid = np.ones((1, 1), dtype=np.float64)
        solver = AStar(grid)
        result = solver.search(start=(0, 0), goal=(0, 0))
        assert result.success
        np.testing.assert_array_equal(result.path, np.array([[0, 0]], dtype=np.int32))

    def test_stats_fields_populated(self) -> None:
        grid = np.ones((5, 5), dtype=np.float64)
        solver = AStar(grid)
        result = solver.search(start=(0, 0), goal=(4, 4))
        assert isinstance(result.stats, SearchStats)
        assert result.stats.nodes_expanded >= 0
        assert result.stats.nodes_opened >= 1
        assert result.stats.closed_set_size >= 0
        assert result.stats.runtime_ms >= 0.0
        assert result.stats.path_length == len(result.path)
        assert math.isclose(result.stats.path_cost, result.cost, rel_tol=1e-12)
        assert result.stats.start == (0, 0)
        assert result.stats.goal == (4, 4)

    def test_result_bool(self) -> None:
        success = PathResult(
            path=np.array([[0, 0]], dtype=np.int32),
            cost=0.0,
            success=True,
            stats=SearchStats(0, 0, 0, 0.0, 1, 0.0, (0, 0), (0, 0)),
        )
        failure = PathResult(
            path=np.empty((0, 2), dtype=np.int32),
            cost=np.inf,
            success=False,
            stats=SearchStats(0, 0, 0, 0.0, 0, np.inf, (0, 0), (0, 0)),
        )
        assert bool(success) is True
        assert bool(failure) is False


class TestReusability:
    """The same solver instance should be usable for multiple queries."""

    def test_multiple_searches_same_instance(self) -> None:
        grid = np.ones((10, 10), dtype=np.float64)
        grid[4, :] = 0.0
        solver = AStar(grid, allow_diagonal=False)

        res1 = solver.search(start=(0, 0), goal=(3, 9))
        res2 = solver.search(start=(5, 0), goal=(9, 9))
        res3 = solver.search(start=(0, 0), goal=(9, 9))

        assert res1.success
        assert res2.success
        assert not res3.success  # wall blocks

        # Ensure buffers were properly reset between calls
        assert tuple(res1.path[0]) == (0, 0)
        assert tuple(res1.path[-1]) == (3, 9)
        assert tuple(res2.path[0]) == (5, 0)
        assert tuple(res2.path[-1]) == (9, 9)


class TestPerformance:
    """Sanity checks on large grids."""

    def test_large_open_grid(self) -> None:
        size = 200
        grid = np.ones((size, size), dtype=np.float64)
        solver = AStar(grid, allow_diagonal=True)
        result = solver.search(start=(0, 0), goal=(size - 1, size - 1))
        assert result.success
        assert len(result.path) == size
        assert math.isclose(
            result.cost, (size - 1) * math.sqrt(2.0), rel_tol=1e-9
        )
