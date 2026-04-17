"""
RRT (Rapidly-exploring Random Tree) path planning implementation.

Features:
- Standard RRT and RRT* (optimal) variants
- KD-tree accelerated nearest neighbor queries
- Collision detection with obstacles
- Path smoothing and tracking
- Comprehensive configuration options
- Pure numpy data structures for performance
"""

from __future__ import annotations

import heapq
import math
from dataclasses import dataclass, field
from typing import Callable, List, Optional, Protocol, Tuple, Union

import numpy as np


@dataclass
class RRTNode:
    """Node in the RRT tree."""

    position: np.ndarray
    parent: Optional[RRTNode] = None
    cost: float = 0.0
    children: List[RRTNode] = field(default_factory=list)

    def __post_init__(self):
        if not isinstance(self.position, np.ndarray):
            self.position = np.array(self.position, dtype=np.float64)
        self.position = self.position.astype(np.float64, copy=False)


class CollisionChecker(Protocol):
    """Protocol for collision checking functions."""

    def __call__(self, point: np.ndarray) -> bool:
        """Return True if point is in collision, False otherwise."""
        ...


class KDTree:
    """Simple KD-tree for fast nearest neighbor queries in low dimensions."""

    def __init__(self, points: np.ndarray, indices: Optional[np.ndarray] = None):
        self.dim = points.shape[1]
        self.points = points
        self.indices = indices if indices is not None else np.arange(len(points))
        self.root = self._build(points, self.indices, 0)

    class Node:
        def __init__(self, point, idx, left, right, axis):
            self.point = point
            self.idx = idx
            self.left = left
            self.right = right
            self.axis = axis

    def _build(
        self, points: np.ndarray, indices: np.ndarray, depth: int
    ) -> Optional[KDTree.Node]:
        if len(points) == 0:
            return None
        axis = depth % self.dim
        sorted_idx = np.argsort(points[:, axis])
        points = points[sorted_idx]
        indices = indices[sorted_idx]
        mid = len(points) // 2
        return self.Node(
            point=points[mid].copy(),
            idx=indices[mid],
            left=self._build(points[:mid], indices[:mid], depth + 1),
            right=self._build(points[mid + 1 :], indices[mid + 1 :], depth + 1),
            axis=axis,
        )

    def nearest(self, query: np.ndarray) -> Tuple[int, float]:
        """Return (index, squared_distance) of nearest point."""
        best = [None, float("inf")]
        self._nearest(self.root, query, best)
        return best[0], best[1]

    def _nearest(self, node: Optional[KDTree.Node], query: np.ndarray, best: list):
        if node is None:
            return
        dist_sq = np.sum((node.point - query) ** 2)
        if dist_sq < best[1]:
            best[0] = node.idx
            best[1] = dist_sq
        axis = node.axis
        diff = query[axis] - node.point[axis]
        first, second = (node.left, node.right) if diff <= 0 else (node.right, node.left)
        self._nearest(first, query, best)
        if diff**2 < best[1]:
            self._nearest(second, query, best)

    def search_radius(self, query: np.ndarray, radius: float) -> List[Tuple[int, float]]:
        """Return list of (index, squared_distance) within radius."""
        result = []
        self._search_radius(self.root, query, radius**2, result)
        return result

    def _search_radius(
        self, node: Optional[KDTree.Node], query: np.ndarray, radius_sq: float, result: list
    ):
        if node is None:
            return
        dist_sq = np.sum((node.point - query) ** 2)
        if dist_sq <= radius_sq:
            result.append((node.idx, dist_sq))
        axis = node.axis
        diff = query[axis] - node.point[axis]
        self._search_radius(node.left, query, radius_sq, result)
        self._search_radius(node.right, query, radius_sq, result)


@dataclass
class RRTOptions:
    """Configuration options for RRT planners."""

    max_iterations: int = 10000
    step_size: float = 0.1
    goal_sample_rate: float = 0.05
    goal_tolerance: float = 0.1
    collision_check_resolution: float = 0.01
    enable_path_smoothing: bool = True
    smoothing_iterations: int = 150
    use_kd_tree: bool = True
    # RRT* specific
    use_rrt_star: bool = False
    rewire_radius: Optional[float] = None  # defaults to step_size * 5
    # Path tracking
    tracking_lookahead: float = 0.2
    tracking_gain: float = 1.0


class RRTPlanner:
    """Standard RRT path planner with optional RRT* optimization."""

    def __init__(
        self,
        start: np.ndarray,
        goal: np.ndarray,
        bounds: np.ndarray,
        collision_checker: CollisionChecker,
        options: Optional[RRTOptions] = None,
    ):
        """
        Args:
            start: Start position, shape (dim,).
            goal: Goal position, shape (dim,).
            bounds: Axis-aligned bounds, shape (dim, 2) where each row is [min, max].
            collision_checker: Function that returns True for collision.
            options: Planner configuration.
        """
        self.start = np.array(start, dtype=np.float64)
        self.goal = np.array(goal, dtype=np.float64)
        self.bounds = np.array(bounds, dtype=np.float64)
        self.dim = self.start.shape[0]
        self.collision_checker = collision_checker
        self.options = options or RRTOptions()
        if self.options.rewire_radius is None:
            self.options.rewire_radius = self.options.step_size * 5.0

        self.nodes: List[RRTNode] = [RRTNode(self.start.copy())]
        self.goal_node: Optional[RRTNode] = None
        self._node_positions = np.empty((0, self.dim), dtype=np.float64)
        self._kd_tree: Optional[KDTree] = None
        self._rebuild_kd_tree()

    def _rebuild_kd_tree(self):
        if len(self.nodes) > 0:
            self._node_positions = np.array([n.position for n in self.nodes], dtype=np.float64)
            if self.options.use_kd_tree:
                self._kd_tree = KDTree(self._node_positions)

    def _nearest_node(self, point: np.ndarray) -> RRTNode:
        if self._kd_tree is not None:
            idx, _ = self._kd_tree.nearest(point)
            return self.nodes[idx]
        # fallback to brute force
        dists = np.linalg.norm(self._node_positions - point, axis=1)
        return self.nodes[int(np.argmin(dists))]

    def _steer(self, from_pos: np.ndarray, to_pos: np.ndarray) -> np.ndarray:
        direction = to_pos - from_pos
        dist = float(np.linalg.norm(direction))
        if dist < 1e-9:
            return from_pos.copy()
        if dist <= self.options.step_size:
            return to_pos.copy()
        return from_pos + direction / dist * self.options.step_size

    def _is_collision_free(self, from_pos: np.ndarray, to_pos: np.ndarray) -> bool:
        if self.collision_checker(from_pos) or self.collision_checker(to_pos):
            return False
        direction = to_pos - from_pos
        dist = float(np.linalg.norm(direction))
        if dist < 1e-9:
            return True
        n_checks = max(1, int(dist / self.options.collision_check_resolution))
        for i in range(1, n_checks):
            t = i / n_checks
            point = from_pos + direction * t
            if self.collision_checker(point):
                return False
        return True

    def _random_point(self) -> np.ndarray:
        if np.random.random() < self.options.goal_sample_rate:
            return self.goal.copy()
        return np.random.uniform(self.bounds[:, 0], self.bounds[:, 1])

    def _near_nodes(self, point: np.ndarray, radius: float) -> List[RRTNode]:
        if self._kd_tree is not None:
            results = self._kd_tree.search_radius(point, radius)
            return [self.nodes[idx] for idx, _ in results]
        # brute force
        dists = np.linalg.norm(self._node_positions - point, axis=1)
        return [self.nodes[i] for i in range(len(self.nodes)) if dists[i] <= radius]

    def _choose_parent(self, point: np.ndarray, near_nodes: List[RRTNode]) -> Optional[RRTNode]:
        best_parent = None
        best_cost = float("inf")
        for node in near_nodes:
            dist = float(np.linalg.norm(node.position - point))
            cost = node.cost + dist
            if cost < best_cost and self._is_collision_free(node.position, point):
                best_parent = node
                best_cost = cost
        return best_parent

    def _rewire(self, new_node: RRTNode, near_nodes: List[RRTNode]):
        for node in near_nodes:
            if node is new_node.parent:
                continue
            dist = float(np.linalg.norm(new_node.position - node.position))
            new_cost = new_node.cost + dist
            if new_cost < node.cost and self._is_collision_free(new_node.position, node.position):
                if node.parent is not None:
                    node.parent.children = [c for c in node.parent.children if c is not node]
                node.parent = new_node
                node.cost = new_cost
                new_node.children.append(node)
                self._update_children_costs(node)

    def _update_children_costs(self, node: RRTNode):
        for child in node.children:
            dist = float(np.linalg.norm(node.position - child.position))
            child.cost = node.cost + dist
            self._update_children_costs(child)

    def plan(self) -> Optional[np.ndarray]:
        """Run RRT/RRT* and return path as (N, dim) numpy array if found."""
        for _ in range(self.options.max_iterations):
            rnd_point = self._random_point()
            nearest = self._nearest_node(rnd_point)
            new_pos = self._steer(nearest.position, rnd_point)

            if not self._is_collision_free(nearest.position, new_pos):
                continue

            if self.options.use_rrt_star:
                near_nodes = self._near_nodes(new_pos, self.options.rewire_radius)
                parent = self._choose_parent(new_pos, near_nodes)
                if parent is None:
                    parent = nearest
            else:
                parent = nearest
                near_nodes = []

            dist = float(np.linalg.norm(parent.position - new_pos))
            new_node = RRTNode(new_pos, parent=parent, cost=parent.cost + dist)
            parent.children.append(new_node)
            self.nodes.append(new_node)

            if self.options.use_rrt_star:
                self._rewire(new_node, near_nodes)

            self._rebuild_kd_tree()

            if float(np.linalg.norm(new_pos - self.goal)) <= self.options.goal_tolerance:
                if self._is_collision_free(new_pos, self.goal):
                    goal_dist = float(np.linalg.norm(new_pos - self.goal))
                    self.goal_node = RRTNode(self.goal.copy(), parent=new_node, cost=new_node.cost + goal_dist)
                    new_node.children.append(self.goal_node)
                    self.nodes.append(self.goal_node)
                    break

        if self.goal_node is None:
            return None

        path = self._extract_path(self.goal_node)
        if self.options.enable_path_smoothing:
            path = self._smooth_path(path)
        return path

    def _extract_path(self, node: RRTNode) -> np.ndarray:
        path = []
        current: Optional[RRTNode] = node
        while current is not None:
            path.append(current.position)
            current = current.parent
        return np.array(path[::-1], dtype=np.float64)

    def _smooth_path(self, path: np.ndarray) -> np.ndarray:
        if len(path) <= 2:
            return path
        for _ in range(self.options.smoothing_iterations):
            i = np.random.randint(0, len(path) - 1)
            j = np.random.randint(i + 1, len(path))
            if self._is_collision_free(path[i], path[j]):
                path = np.vstack([path[: i + 1], path[j:]])
        return path


class RRTStarPlanner(RRTPlanner):
    """Convenience wrapper that enables RRT* by default."""

    def __init__(
        self,
        start: np.ndarray,
        goal: np.ndarray,
        bounds: np.ndarray,
        collision_checker: CollisionChecker,
        options: Optional[RRTOptions] = None,
    ):
        if options is None:
            options = RRTOptions()
        options.use_rrt_star = True
        super().__init__(start, goal, bounds, collision_checker, options)


class PathTracker:
    """Track a reference path using pure-pursuit style lookahead."""

    def __init__(self, path: np.ndarray, options: Optional[RRTOptions] = None):
        """
        Args:
            path: Reference path as (N, dim) array.
            options: Tracking configuration.
        """
        self.path = np.array(path, dtype=np.float64)
        self.options = options or RRTOptions()
        self._build_lookup()

    def _build_lookup(self):
        if len(self.path) == 0:
            self.cumlen = np.array([0.0])
            return
        diffs = np.diff(self.path, axis=0)
        seg_lens = np.linalg.norm(diffs, axis=1)
        self.cumlen = np.concatenate(([0.0], np.cumsum(seg_lens)))
        self.total_length = float(self.cumlen[-1])

    def closest_point(self, position: np.ndarray) -> Tuple[np.ndarray, float, int]:
        """Return (closest_point_on_path, distance_along_path, segment_index)."""
        position = np.array(position, dtype=np.float64)
        if len(self.path) == 0:
            return position.copy(), 0.0, 0
        if len(self.path) == 1:
            return self.path[0].copy(), 0.0, 0
        diffs = self.path[1:] - self.path[:-1]
        t = np.sum((position - self.path[:-1]) * diffs, axis=1) / (
            np.sum(diffs**2, axis=1) + 1e-12
        )
        t = np.clip(t, 0.0, 1.0)
        projections = self.path[:-1] + t[:, None] * diffs
        dists = np.linalg.norm(projections - position, axis=1)
        seg_idx = int(np.argmin(dists))
        closest = projections[seg_idx]
        s = self.cumlen[seg_idx] + t[seg_idx] * float(np.linalg.norm(diffs[seg_idx]))
        return closest, s, seg_idx

    def lookahead_point(self, position: np.ndarray) -> np.ndarray:
        """Return lookahead target point on path."""
        _, s, _ = self.closest_point(position)
        target_s = min(s + self.options.tracking_lookahead, self.total_length)
        # Find segment containing target_s
        seg_idx = int(np.searchsorted(self.cumlen, target_s) - 1)
        seg_idx = max(0, min(seg_idx, len(self.path) - 2))
        seg_start = self.cumlen[seg_idx]
        seg_end = self.cumlen[seg_idx + 1]
        seg_len = seg_end - seg_start
        if seg_len < 1e-9:
            return self.path[seg_idx].copy()
        alpha = (target_s - seg_start) / seg_len
        return self.path[seg_idx] + alpha * (self.path[seg_idx + 1] - self.path[seg_idx])

    def tracking_error(self, position: np.ndarray) -> float:
        """Return cross-track distance to path."""
        closest, _, _ = self.closest_point(position)
        return float(np.linalg.norm(closest - position))

    def control_direction(self, position: np.ndarray) -> np.ndarray:
        """Return normalized direction vector towards lookahead point."""
        target = self.lookahead_point(position)
        direction = target - position
        norm = float(np.linalg.norm(direction))
        if norm < 1e-9:
            return np.zeros(self.path.shape[1], dtype=np.float64)
        return direction / norm

    def is_done(self, position: np.ndarray, tolerance: float = 0.1) -> bool:
        """Check if position is within tolerance of path end."""
        return float(np.linalg.norm(self.path[-1] - position)) <= tolerance
