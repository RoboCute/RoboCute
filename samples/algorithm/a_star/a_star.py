"""Optimized A* pathfinding using numpy data structures.

This module provides a high-performance grid-based A* implementation with
configurable heuristics, tie-breaking strategies, diagonal movement, and
optional path smoothing.
"""

from __future__ import annotations

import heapq
import math
import time
from dataclasses import dataclass
from enum import Enum, auto
from typing import Callable, Optional, Tuple, Union

import numpy as np


class Heuristic(Enum):
    """Supported heuristic functions for A*."""

    MANHATTAN = auto()
    EUCLIDEAN = auto()
    CHEBYSHEV = auto()
    OCTILE = auto()
    DIAGONAL = auto()
    ZERO = auto()


class TieBreaker(Enum):
    """Tie-breaking strategies when f-scores are equal."""

    NONE = auto()
    CROSS_PRODUCT = auto()
    SMALLER_G = auto()
    PREFER_STRAIGHT = auto()


@dataclass(frozen=True, slots=True)
class SearchStats:
    """Statistics collected during an A* search."""

    nodes_expanded: int
    nodes_opened: int
    closed_set_size: int
    runtime_ms: float
    path_length: int
    path_cost: float
    start: Tuple[int, int]
    goal: Tuple[int, int]


@dataclass(frozen=True, slots=True)
class PathResult:
    """Result of an A* search."""

    path: np.ndarray
    cost: float
    success: bool
    stats: SearchStats

    def __bool__(self) -> bool:
        return self.success


class AStar:
    """Grid-based A* pathfinder backed by numpy arrays.

    Parameters
    ----------
    grid : np.ndarray
        2-D cost map.  Values <= 0 are treated as obstacles;
        positive values are movement-cost multipliers for the cell.
    heuristic : Heuristic | Callable, optional
        Heuristic function or preset enum value.  Defaults to ``EUCLIDEAN``.
    tie_breaker : TieBreaker, optional
        Strategy to break ties in the priority queue.  Defaults to ``NONE``.
    allow_diagonal : bool, optional
        If True, 8-connectivity is used; otherwise 4-connectivity.
    diagonal_cost : float, optional
        Cost of a diagonal step.  Defaults to ``sqrt(2)``.
    weight : float, optional
        Weighted-A* factor (``w >= 1``).  Larger values search faster but
        may return sub-optimal paths.  Defaults to ``1.0`` (optimal A*).
    time_limit_ms : float | None, optional
        Maximum search time in milliseconds.  ``None`` means no limit.
    """

    def __init__(
        self,
        grid: np.ndarray,
        heuristic: Union[Heuristic, Callable[[Tuple[int, int], Tuple[int, int]], float]] = Heuristic.EUCLIDEAN,
        tie_breaker: TieBreaker = TieBreaker.NONE,
        allow_diagonal: bool = True,
        diagonal_cost: float = math.sqrt(2.0),
        weight: float = 1.0,
        time_limit_ms: Optional[float] = None,
    ) -> None:
        if grid.ndim != 2:
            raise ValueError("grid must be a 2-D array")

        self._grid = np.asarray(grid, dtype=np.float64)
        self._rows, self._cols = self._grid.shape
        self._allow_diagonal = bool(allow_diagonal)
        self._diagonal_cost = float(diagonal_cost)
        self._weight = max(1.0, float(weight))
        self._time_limit_ms = time_limit_ms
        self._tie_breaker = tie_breaker

        # Pre-compute neighbour offsets and step costs
        if self._allow_diagonal:
            self._neigh_dr = np.array([-1, -1, -1, 0, 0, 1, 1, 1], dtype=np.int32)
            self._neigh_dc = np.array([-1, 0, 1, -1, 1, -1, 0, 1], dtype=np.int32)
            self._neigh_cost = np.array(
                [self._diagonal_cost, 1.0, self._diagonal_cost,
                 1.0, 1.0,
                 self._diagonal_cost, 1.0, self._diagonal_cost],
                dtype=np.float64,
            )
        else:
            self._neigh_dr = np.array([-1, 0, 0, 1], dtype=np.int32)
            self._neigh_dc = np.array([0, -1, 1, 0], dtype=np.int32)
            self._neigh_cost = np.array([1.0, 1.0, 1.0, 1.0], dtype=np.float64)

        # Cache for heuristic function
        if callable(heuristic):
            self._h_func = heuristic
        else:
            self._h_func = self._resolve_heuristic(heuristic)

        # Pre-allocate numpy buffers (re-used across searches)
        n_cells = self._rows * self._cols
        self._g = np.full(n_cells, np.inf, dtype=np.float64)
        self._f = np.full(n_cells, np.inf, dtype=np.float64)
        self._came_from = np.full(n_cells, -1, dtype=np.int64)
        self._visited = np.zeros(n_cells, dtype=np.bool_)
        self._in_open = np.zeros(n_cells, dtype=np.bool_)

    # --------------------------------------------------------------------- #
    # Public API
    # --------------------------------------------------------------------- #

    def search(
        self,
        start: Tuple[int, int],
        goal: Tuple[int, int],
        smooth: bool = False,
    ) -> PathResult:
        """Run A* from *start* to *goal*.

        Parameters
        ----------
        start : (int, int)
            Starting grid coordinate ``(row, col)``.
        goal : (int, int)
            Target grid coordinate ``(row, col)``.
        smooth : bool, optional
            If True, apply a lightweight post-processing step that removes
            unnecessary zig-zags while keeping the path collision-free.

        Returns
        -------
        PathResult
        """
        t0 = time.perf_counter()

        sr, sc = int(start[0]), int(start[1])
        gr, gc = int(goal[0]), int(goal[1])

        if not self._in_bounds(sr, sc):
            raise ValueError(f"start {start} is out of bounds")
        if not self._in_bounds(gr, gc):
            raise ValueError(f"goal {goal} is out of bounds")
        if not self._is_free(sr, sc) or not self._is_free(gr, gc):
            return self._empty_result(sr, sc, gr, gc, t0)

        # Reset buffers
        self._g.fill(np.inf)
        self._f.fill(np.inf)
        self._came_from.fill(-1)
        self._visited.fill(False)
        self._in_open.fill(False)

        start_idx = self._idx(sr, sc)
        goal_idx = self._idx(gr, gc)

        self._g[start_idx] = 0.0
        self._f[start_idx] = self._weight * self._h_func(start, goal)
        self._in_open[start_idx] = True

        # Priority queue stores tuples:
        #   (f, tie_break, idx)
        # tie_break is chosen so that ``<`` ordering favours the desired node.
        counter = 0
        open_heap: list[Tuple[float, float, int, int]] = [
            (self._f[start_idx], 0.0, counter, start_idx)
        ]

        nodes_expanded = 0
        nodes_opened = 1
        closed_set_size = 0
        time_limit_sec = (
            self._time_limit_ms / 1000.0 if self._time_limit_ms is not None else None
        )

        # Local aliases for speed
        g_buf = self._g
        f_buf = self._f
        came_buf = self._came_from
        visited = self._visited
        in_open = self._in_open
        grid = self._grid
        rows = self._rows
        cols = self._cols
        neigh_dr = self._neigh_dr
        neigh_dc = self._neigh_dc
        neigh_cost = self._neigh_cost
        h_func = self._h_func
        weight = self._weight
        tie_breaker = self._tie_breaker

        while open_heap:
            if time_limit_sec is not None:
                if time.perf_counter() - t0 > time_limit_sec:
                    break

            f_val, tie_val, _, current_idx = heapq.heappop(open_heap)

            # Skip stale entries
            if visited[current_idx]:
                continue
            if f_val != f_buf[current_idx]:
                continue

            visited[current_idx] = True
            in_open[current_idx] = False
            closed_set_size += 1
            nodes_expanded += 1

            if current_idx == goal_idx:
                path = self._reconstruct_path(goal_idx)
                if smooth:
                    path = self._smooth_path(path)
                cost = g_buf[goal_idx]
                elapsed = (time.perf_counter() - t0) * 1000.0
                stats = SearchStats(
                    nodes_expanded=nodes_expanded,
                    nodes_opened=nodes_opened,
                    closed_set_size=closed_set_size,
                    runtime_ms=elapsed,
                    path_length=len(path),
                    path_cost=cost,
                    start=(sr, sc),
                    goal=(gr, gc),
                )
                return PathResult(path=path, cost=cost, success=True, stats=stats)

            cr = current_idx // cols
            cc = current_idx % cols

            for i in range(neigh_dr.shape[0]):
                nr = cr + int(neigh_dr[i])
                nc = cc + int(neigh_dc[i])

                if not (0 <= nr < rows and 0 <= nc < cols):
                    continue

                n_idx = nr * cols + nc
                if grid[nr, nc] <= 0.0:
                    continue

                # Corner-cutting check for diagonal moves
                if neigh_cost[i] != 1.0:
                    if grid[cr, nc] <= 0.0 or grid[nr, cc] <= 0.0:
                        continue

                move_cost = neigh_cost[i] * grid[nr, nc]
                tentative_g = g_buf[current_idx] + move_cost

                if tentative_g < g_buf[n_idx]:
                    came_buf[n_idx] = current_idx
                    g_buf[n_idx] = tentative_g
                    h = h_func((nr, nc), (gr, gc))
                    f_buf[n_idx] = tentative_g + weight * h

                    tie = 0.0
                    if tie_breaker == TieBreaker.CROSS_PRODUCT:
                        # Prefer nodes closer to the straight line start->goal
                        dx1 = nr - gr
                        dy1 = nc - gc
                        dx2 = sr - gr
                        dy2 = sc - gc
                        tie = abs(dx1 * dy2 - dx2 * dy1) * 0.001
                    elif tie_breaker == TieBreaker.SMALLER_G:
                        tie = -tentative_g
                    elif tie_breaker == TieBreaker.PREFER_STRAIGHT:
                        # Slightly penalise direction changes
                        if came_buf[current_idx] >= 0:
                            pr = came_buf[current_idx] // cols
                            pc = came_buf[current_idx] % cols
                            if (nr - cr) != (cr - pr) or (nc - cc) != (cc - pc):
                                tie = 1e-6

                    counter += 1
                    heapq.heappush(open_heap, (f_buf[n_idx], tie, counter, n_idx))
                    if not in_open[n_idx]:
                        in_open[n_idx] = True
                        nodes_opened += 1

        elapsed = (time.perf_counter() - t0) * 1000.0
        return PathResult(
            path=np.empty((0, 2), dtype=np.int32),
            cost=np.inf,
            success=False,
            stats=SearchStats(
                nodes_expanded=nodes_expanded,
                nodes_opened=nodes_opened,
                closed_set_size=closed_set_size,
                runtime_ms=elapsed,
                path_length=0,
                path_cost=np.inf,
                start=(sr, sc),
                goal=(gr, gc),
            ),
        )

    # --------------------------------------------------------------------- #
    # Helpers
    # --------------------------------------------------------------------- #

    @staticmethod
    def _resolve_heuristic(
        heuristic: Heuristic,
    ) -> Callable[[Tuple[int, int], Tuple[int, int]], float]:
        def manhattan(a: Tuple[int, int], b: Tuple[int, int]) -> float:
            return abs(a[0] - b[0]) + abs(a[1] - b[1])

        def euclidean(a: Tuple[int, int], b: Tuple[int, int]) -> float:
            return math.hypot(a[0] - b[0], a[1] - b[1])

        def chebyshev(a: Tuple[int, int], b: Tuple[int, int]) -> float:
            return max(abs(a[0] - b[0]), abs(a[1] - b[1]))

        def octile(a: Tuple[int, int], b: Tuple[int, int]) -> float:
            dx = abs(a[0] - b[0])
            dy = abs(a[1] - b[1])
            return max(dx, dy) + (math.sqrt(2.0) - 1.0) * min(dx, dy)

        def diagonal(a: Tuple[int, int], b: Tuple[int, int]) -> float:
            dx = abs(a[0] - b[0])
            dy = abs(a[1] - b[1])
            return dx + dy + (math.sqrt(2.0) - 2.0) * min(dx, dy)

        def zero(_a: Tuple[int, int], _b: Tuple[int, int]) -> float:
            return 0.0

        mapping = {
            Heuristic.MANHATTAN: manhattan,
            Heuristic.EUCLIDEAN: euclidean,
            Heuristic.CHEBYSHEV: chebyshev,
            Heuristic.OCTILE: octile,
            Heuristic.DIAGONAL: diagonal,
            Heuristic.ZERO: zero,
        }
        return mapping[heuristic]

    def _in_bounds(self, r: int, c: int) -> bool:
        return 0 <= r < self._rows and 0 <= c < self._cols

    def _is_free(self, r: int, c: int) -> bool:
        return self._grid[r, c] > 0.0

    def _idx(self, r: int, c: int) -> int:
        return r * self._cols + c

    def _reconstruct_path(self, goal_idx: int) -> np.ndarray:
        path = []
        idx = goal_idx
        while idx >= 0:
            r = idx // self._cols
            c = idx % self._cols
            path.append((r, c))
            idx = int(self._came_from[idx])
        path.reverse()
        return np.array(path, dtype=np.int32)

    def _empty_result(
        self,
        sr: int,
        sc: int,
        gr: int,
        gc: int,
        t0: float,
    ) -> PathResult:
        elapsed = (time.perf_counter() - t0) * 1000.0
        return PathResult(
            path=np.empty((0, 2), dtype=np.int32),
            cost=np.inf,
            success=False,
            stats=SearchStats(
                nodes_expanded=0,
                nodes_opened=0,
                closed_set_size=0,
                runtime_ms=elapsed,
                path_length=0,
                path_cost=np.inf,
                start=(sr, sc),
                goal=(gr, gc),
            ),
        )

    def _smooth_path(self, path: np.ndarray) -> np.ndarray:
        """Lightweight line-of-sight smoothing.

        Repeatedly tries to skip intermediate way-points while keeping the
        straight line between anchor points free of obstacles.
        """
        if len(path) <= 2:
            return path

        # Walk the path and greedily keep anchor points.
        smoothed = [path[0]]
        i = 0
        n = len(path)
        while i < n - 1:
            j = n - 1
            while j > i + 1:
                if self._line_of_sight(path[i], path[j]):
                    break
                j -= 1
            smoothed.append(path[j])
            i = j

        return np.array(smoothed, dtype=np.int32)

    def _line_of_sight(
        self,
        a: np.ndarray,
        b: np.ndarray,
    ) -> bool:
        """Bresenham-like integer line-of-sight test."""
        x0, y0 = int(a[0]), int(a[1])
        x1, y1 = int(b[0]), int(b[1])
        dx = abs(x1 - x0)
        dy = abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx - dy

        while True:
            if self._grid[x0, y0] <= 0.0:
                return False
            if x0 == x1 and y0 == y1:
                return True
            e2 = 2 * err
            if e2 > -dy:
                err -= dy
                x0 += sx
            if e2 < dx:
                err += dx
                y0 += sy
