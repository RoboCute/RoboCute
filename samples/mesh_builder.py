from typing import BinaryIO
import struct
from pathlib import Path
import numpy as np
import robocute.rbc_ext as re


class MeshBuilder:
    """
    Python implementation of rbc::MeshBuilder.
    
    Builds mesh data with positions, normals, tangents, UVs, and triangle indices.
    Uses mesh host buffers directly as numpy arrays for efficient data manipulation.
    """

    def __init__(
        self,
        vertex_count: int,
        triangle_count: int,
        submesh_offsets: np.ndarray | None = None,
        uv_count: int = 0,
        has_normal: bool = False,
        has_tangent: bool = False
    ):
        """
        Initialize mesh builder with specified size.
        
        Args:
            vertex_count: Number of vertices
            triangle_count: Number of triangles
            submesh_offsets: Submesh triangle offsets (empty for single submesh)
            uv_count: Number of UV sets
            has_normal: Whether mesh has normals
            has_tangent: Whether mesh has tangents
        """
        if vertex_count <= 0:
            raise ValueError(f"vertex_count must be positive, got {vertex_count}")
        if triangle_count < 0:
            raise ValueError(f"triangle_count must be non-negative, got {triangle_count}")
        if uv_count < 0:
            raise ValueError(f"uv_count must be non-negative, got {uv_count}")
        
        # Initialize submesh offsets
        if submesh_offsets is None or len(submesh_offsets) == 0:
            self._submesh_offsets = np.array([], dtype=np.uint32)
        else:
            self._submesh_offsets = np.array(submesh_offsets, dtype=np.uint32)
        
        # Store configuration
        self._vertex_count = vertex_count
        self._triangle_count = triangle_count
        self._uv_count = uv_count
        self._has_normal = has_normal
        self._has_tangent = has_tangent
        
        # Create mesh resource and allocate buffers
        self._mesh = re.world.MeshResource()
        self._mesh.create_empty(
            self._submesh_offsets,
            vertex_count,
            triangle_count,
            uv_count,
            has_normal,
            has_tangent
        )
        
        # Create numpy array views into mesh buffers
        self._init_buffer_views()

    def _init_buffer_views(self) -> None:
        """Initialize numpy array views into mesh host buffers."""
        # Position buffer (float4 per vertex)
        pos_buf = self._mesh.pos_buffer()
        if pos_buf is not None:
            self.position = np.ndarray(self._vertex_count * 4, dtype=np.float32, buffer=pos_buf)
            # Initialize to zero
            self.position[:] = 0.0
        else:
            self.position = np.zeros(self._vertex_count * 4, dtype=np.float32)
        
        # Normal buffer (float4 per vertex, optional)
        if self._has_normal:
            normal_buf = self._mesh.normal_buffer()
            if normal_buf is not None:
                self.normal = np.ndarray(self._vertex_count * 4, dtype=np.float32, buffer=normal_buf)
                self.normal[:] = 0.0
            else:
                self.normal = np.zeros(self._vertex_count * 4, dtype=np.float32)
        else:
            self.normal = np.array([], dtype=np.float32)
        
        # Tangent buffer (float4 per vertex, optional)
        if self._has_tangent:
            tangent_buf = self._mesh.tangent_buffer()
            if tangent_buf is not None:
                self.tangent = np.ndarray(self._vertex_count * 4, dtype=np.float32, buffer=tangent_buf)
                self.tangent[:] = 0.0
            else:
                self.tangent = np.zeros(self._vertex_count * 4, dtype=np.float32)
        else:
            self.tangent = np.array([], dtype=np.float32)
        
        # UV buffers (float2 per vertex per UV set)
        self.uvs: list[np.ndarray] = []
        for i in range(self._uv_count):
            uv_buf = self._mesh.uv_buffer(i)
            if uv_buf is not None:
                uv_array = np.ndarray(self._vertex_count * 2, dtype=np.float32, buffer=uv_buf)
                uv_array[:] = 0.0
            else:
                uv_array = np.zeros(self._vertex_count * 2, dtype=np.float32)
            self.uvs.append(uv_array)
        
        # Triangle indices buffer (uint32, 3 per triangle)
        indices_buf = self._mesh.triangle_indices_buffer()
        if indices_buf is not None:
            self.triangle_indices = np.ndarray(self._triangle_count * 3, dtype=np.uint32, buffer=indices_buf)
            self.triangle_indices[:] = 0
        else:
            self.triangle_indices = np.zeros(self._triangle_count * 3, dtype=np.uint32)

    def vertex_count(self) -> int:
        """Return the number of vertices."""
        return self._vertex_count

    def triangle_count(self) -> int:
        """Return the number of triangles."""
        return self._triangle_count

    def contained_normal(self) -> bool:
        """Return True if normal data is present."""
        return self._has_normal

    def contained_tangent(self) -> bool:
        """Return True if tangent data is present."""
        return self._has_tangent

    def uv_count(self) -> int:
        """Return the number of UV sets."""
        return self._uv_count

    def submesh_count(self) -> int:
        """Return the number of submeshes."""
        return max(len(self._submesh_offsets), 1)

    def indices_count(self) -> int:
        """Return the total number of indices (3 per triangle)."""
        return self._triangle_count * 3

    def get_mesh(self) -> re.world.MeshResource:
        """Get the underlying MeshResource."""
        return self._mesh

    def _check_vertex_index(self, index: int) -> None:
        """Check if vertex index is valid."""
        if index < 0 or index >= self._vertex_count:
            raise IndexError(f"Vertex index {index} out of range [0, {self._vertex_count})")

    def _check_triangle_index(self, index: int) -> None:
        """Check if triangle index is valid."""
        if index < 0 or index >= self._triangle_count:
            raise IndexError(f"Triangle index {index} out of range [0, {self._triangle_count})")

    def _check_uv_index(self, uv_index: int) -> None:
        """Check if UV set index is valid."""
        if uv_index < 0 or uv_index >= self._uv_count:
            raise IndexError(f"UV set index {uv_index} out of range [0, {self._uv_count})")

    def set_position(self, vertex_index: int, position: np.ndarray | tuple[float, float, float]) -> None:
        """
        Set position for a vertex.
        
        Args:
            vertex_index: Index of the vertex
            position: Position as (x, y, z) tuple or array
        """
        self._check_vertex_index(vertex_index)
        pos = np.array(position, dtype=np.float32).flatten()
        if pos.shape[0] != 3:
            raise ValueError(f"Position must have 3 components, got {pos.shape[0]}")
        
        idx = vertex_index * 4
        self.position[idx] = pos[0]
        self.position[idx + 1] = pos[1]
        self.position[idx + 2] = pos[2]
        self.position[idx + 3] = 0.0  # padding

    def set_normal(self, vertex_index: int, normal: np.ndarray | tuple[float, float, float]) -> None:
        """
        Set normal for a vertex.
        
        Args:
            vertex_index: Index of the vertex
            normal: Normal as (x, y, z) tuple or array
        """
        if not self._has_normal:
            raise RuntimeError("Mesh was not created with normals")
        
        self._check_vertex_index(vertex_index)
        n = np.array(normal, dtype=np.float32).flatten()
        if n.shape[0] != 3:
            raise ValueError(f"Normal must have 3 components, got {n.shape[0]}")
        
        idx = vertex_index * 4
        self.normal[idx] = n[0]
        self.normal[idx + 1] = n[1]
        self.normal[idx + 2] = n[2]
        self.normal[idx + 3] = 0.0  # padding

    def set_tangent(self, vertex_index: int, tangent: np.ndarray | tuple[float, float, float, float]) -> None:
        """
        Set tangent for a vertex.
        
        Args:
            vertex_index: Index of the vertex
            tangent: Tangent as (x, y, z, w) tuple or array
        """
        if not self._has_tangent:
            raise RuntimeError("Mesh was not created with tangents")
        
        self._check_vertex_index(vertex_index)
        t = np.array(tangent, dtype=np.float32).flatten()
        if t.shape[0] != 4:
            raise ValueError(f"Tangent must have 4 components, got {t.shape[0]}")
        
        idx = vertex_index * 4
        self.tangent[idx] = t[0]
        self.tangent[idx + 1] = t[1]
        self.tangent[idx + 2] = t[2]
        self.tangent[idx + 3] = t[3]

    def set_uv(self, vertex_index: int, uv_index: int, uv: np.ndarray | tuple[float, float]) -> None:
        """
        Set UV coordinates for a vertex.
        
        Args:
            vertex_index: Index of the vertex
            uv_index: Index of the UV set
            uv: UV coordinates as (u, v) tuple or array
        """
        self._check_uv_index(uv_index)
        self._check_vertex_index(vertex_index)
        
        uv_val = np.array(uv, dtype=np.float32).flatten()
        if uv_val.shape[0] != 2:
            raise ValueError(f"UV must have 2 components, got {uv_val.shape[0]}")
        
        idx = vertex_index * 2
        self.uvs[uv_index][idx] = uv_val[0]
        self.uvs[uv_index][idx + 1] = uv_val[1]

    def set_triangle(self, triangle_index: int, i0: int, i1: int, i2: int) -> None:
        """
        Set indices for a triangle.
        
        Args:
            triangle_index: Index of the triangle
            i0, i1, i2: Vertex indices of the triangle
        """
        self._check_triangle_index(triangle_index)
        self._check_vertex_index(i0)
        self._check_vertex_index(i1)
        self._check_vertex_index(i2)
        
        idx = triangle_index * 3
        self.triangle_indices[idx] = i0
        self.triangle_indices[idx + 1] = i1
        self.triangle_indices[idx + 2] = i2

    def set_positions(self, positions: np.ndarray) -> None:
        """
        Set all positions at once.
        
        Args:
            positions: Array of shape (vertex_count, 3) with positions
        """
        positions = np.array(positions, dtype=np.float32)
        if positions.shape != (self._vertex_count, 3):
            raise ValueError(f"Positions shape must be ({self._vertex_count}, 3), got {positions.shape}")
        
        self.position[0::4] = positions[:, 0]
        self.position[1::4] = positions[:, 1]
        self.position[2::4] = positions[:, 2]
        self.position[3::4] = 0.0

    def set_normals(self, normals: np.ndarray) -> None:
        """
        Set all normals at once.
        
        Args:
            normals: Array of shape (vertex_count, 3) with normals
        """
        if not self._has_normal:
            raise RuntimeError("Mesh was not created with normals")
        
        normals = np.array(normals, dtype=np.float32)
        if normals.shape != (self._vertex_count, 3):
            raise ValueError(f"Normals shape must be ({self._vertex_count}, 3), got {normals.shape}")
        
        self.normal[0::4] = normals[:, 0]
        self.normal[1::4] = normals[:, 1]
        self.normal[2::4] = normals[:, 2]
        self.normal[3::4] = 0.0

    def set_tangents(self, tangents: np.ndarray) -> None:
        """
        Set all tangents at once.
        
        Args:
            tangents: Array of shape (vertex_count, 4) with tangents
        """
        if not self._has_tangent:
            raise RuntimeError("Mesh was not created with tangents")
        
        tangents = np.array(tangents, dtype=np.float32)
        if tangents.shape != (self._vertex_count, 4):
            raise ValueError(f"Tangents shape must be ({self._vertex_count}, 4), got {tangents.shape}")
        
        self.tangent[0::4] = tangents[:, 0]
        self.tangent[1::4] = tangents[:, 1]
        self.tangent[2::4] = tangents[:, 2]
        self.tangent[3::4] = tangents[:, 3]

    def set_uvs(self, uv_index: int, uvs: np.ndarray) -> None:
        """
        Set all UVs for a UV set at once.
        
        Args:
            uv_index: Index of the UV set
            uvs: Array of shape (vertex_count, 2) with UVs
        """
        self._check_uv_index(uv_index)
        
        uvs = np.array(uvs, dtype=np.float32)
        if uvs.shape != (self._vertex_count, 2):
            raise ValueError(f"UVs shape must be ({self._vertex_count}, 2), got {uvs.shape}")
        
        self.uvs[uv_index][0::2] = uvs[:, 0]
        self.uvs[uv_index][1::2] = uvs[:, 1]

    def set_triangles(self, triangles: np.ndarray) -> None:
        """
        Set all triangle indices at once.
        
        Args:
            triangles: Array of shape (triangle_count, 3) with indices
        """
        triangles = np.array(triangles, dtype=np.uint32)
        if triangles.shape != (self._triangle_count, 3):
            raise ValueError(f"Triangles shape must be ({self._triangle_count}, 3), got {triangles.shape}")
        
        # Validate indices
        if np.any(triangles >= self._vertex_count):
            invalid = triangles[triangles >= self._vertex_count][0]
            raise ValueError(f"Triangle index {invalid} exceeds vertex count {self._vertex_count}")
        
        self.triangle_indices[:] = triangles.flatten()

    def check(self) -> str:
        """
        Validate mesh data and return error message if invalid.
        Returns empty string if valid.
        """
        errors = []
        
        if self._vertex_count == 0:
            errors.append("No vertices in mesh.")
        
        if self._triangle_count == 0:
            errors.append("No triangles in mesh.")
        
        # Check if any position is uninitialized (all zeros might indicate uninitialized)
        # This is a heuristic check
        
        # Check triangle indices are within range
        if self.triangle_indices is not None and len(self.triangle_indices) > 0:
            max_index = np.max(self.triangle_indices)
            if max_index >= self._vertex_count:
                errors.append(f"Max triangle index {max_index} exceeds vertex count {self._vertex_count}.")
        
        return "\n".join(errors)

    @staticmethod
    def calculate_tangent(
        positions: np.ndarray,
        uvs: np.ndarray,
        triangles: np.ndarray,
        tangent_w: float = 1.0
    ) -> np.ndarray:
        """
        Calculate tangent vectors for mesh vertices using the Mikktspace algorithm.
        
        Args:
            positions: Array of vertex positions (N, 3)
            uvs: Array of texture coordinates (N, 2)
            triangles: Array of triangle indices (M, 3)
            tangent_w: Tangent W component (handedness)
            
        Returns:
            Array of tangent vectors (N, 4) where w component is the handedness
        """
        n_vertices = len(positions)
        tangents = np.zeros((n_vertices, 3), dtype=np.float32)
        counts = np.zeros(n_vertices, dtype=np.int32)

        for tri in triangles:
            i0, i1, i2 = tri
            p0, p1, p2 = positions[i0], positions[i1], positions[i2]
            uv0, uv1, uv2 = uvs[i0], uvs[i1], uvs[i2]

            # Calculate edges and UV deltas
            edge1 = p1 - p0
            edge2 = p2 - p0
            delta_uv1 = uv1 - uv0
            delta_uv2 = uv2 - uv0

            # Calculate tangent
            f_outer_dot = delta_uv1[0] * delta_uv2[1] - delta_uv2[0] * delta_uv1[1]
            f_outer_dot = np.sign(f_outer_dot) * max(abs(f_outer_dot), 1e-5)
            f = 1.0 / f_outer_dot
            tangent = f * (delta_uv2[1] * edge1 - delta_uv1[1] * edge2)

            # Normalize
            tangent_len = np.linalg.norm(tangent)
            if tangent_len > 1e-5:
                tangent = tangent / tangent_len

            # Accumulate for each vertex of the triangle
            for idx in tri:
                tangents[idx] += tangent
                counts[idx] += 1

        # Average and normalize
        result = np.zeros((n_vertices, 4), dtype=np.float32)
        for i in range(n_vertices):
            if counts[i] > 0:
                tan = tangents[i] / counts[i]
                tan_len = np.linalg.norm(tan)
                if tan_len > 1e-5:
                    tan = tan / tan_len
                result[i, :3] = tan
                result[i, 3] = tangent_w

        return result

    def __repr__(self) -> str:
        return (
            f"MeshBuilder("
            f"vertices={self._vertex_count}, "
            f"triangles={self._triangle_count}, "
            f"submeshes={self.submesh_count()}, "
            f"normals={self._has_normal}, "
            f"tangents={self._has_tangent}, "
            f"uv_sets={self._uv_count}"
            f")"
        )
