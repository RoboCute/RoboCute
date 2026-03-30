from typing import BinaryIO
import struct
from pathlib import Path
import numpy as np
import robocute.rbc_ext as re


class MeshBuilder:
    """
    Python implementation of rbc::MeshBuilder.
    
    Builds mesh data with positions, normals, tangents, UVs, and triangle indices.
    Supports multiple submeshes and multiple UV sets.
    """

    def __init__(self):
        # Vertex data
        self.position: np.ndarray = np.zeros((0, 3), dtype=np.float32)  # float3
        self.normal: np.ndarray = np.zeros((0, 3), dtype=np.float32)    # float3
        self.tangent: np.ndarray = np.zeros((0, 4), dtype=np.float32)   # float4
        self.uvs: list[np.ndarray] = []                                 # list of float2 arrays
        
        # Index data - each submesh has its own index array
        self.triangle_indices: list[np.ndarray] = []                    # list of uint arrays

    def vertex_count(self) -> int:
        """Return the number of vertices."""
        return self.position.shape[0]

    def contained_normal(self) -> bool:
        """Return True if normal data is present."""
        return self.normal.shape[0] > 0

    def contained_tangent(self) -> bool:
        """Return True if tangent data is present."""
        return self.tangent.shape[0] > 0

    def uv_count(self) -> int:
        """Return the number of UV sets."""
        return len(self.uvs)

    def submesh_count(self) -> int:
        """Return the number of submeshes."""
        return len(self.triangle_indices)

    def indices_count(self) -> int:
        """Return the total number of indices across all submeshes."""
        return sum(indices.shape[0] for indices in self.triangle_indices)

    def get_submesh_indices(self, submesh_index: int) -> np.ndarray:
        """Get the triangle indices for a specific submesh."""
        if submesh_index < 0 or submesh_index >= len(self.triangle_indices):
            raise IndexError(f"Submesh index {submesh_index} out of range [0, {len(self.triangle_indices)})")
        return self.triangle_indices[submesh_index].copy()

    def check(self) -> str:
        """
        Validate mesh data and return error message if invalid.
        Returns empty string if valid.
        """
        errors = []
        vertex_count = self.position.shape[0]

        if vertex_count == 0:
            errors.append("No vertices in mesh.")

        # Check normal size matches position size
        if self.normal.shape[0] > 0 and self.normal.shape[0] != vertex_count:
            errors.append(f"Normal size {self.normal.shape[0]} does not match position size {vertex_count}.")

        # Check tangent size matches position size
        if self.tangent.shape[0] > 0 and self.tangent.shape[0] != vertex_count:
            errors.append(f"Tangent size {self.tangent.shape[0]} does not match position size {vertex_count}.")

        # Check UV sizes match position size
        for idx, uv in enumerate(self.uvs):
            if uv.shape[0] > 0 and uv.shape[0] != vertex_count:
                errors.append(f"UV{idx} size {uv.shape[0]} does not match position size {vertex_count}.")

        # Check triangle indices
        for idx, indices in enumerate(self.triangle_indices):
            # Check that index count is divisible by 3 (complete triangles)
            if indices.shape[0] % 3 != 0:
                errors.append(f"Submesh {idx} size {indices.shape[0]} is not divisible by 3.")
                continue

            # Check that all indices are within vertex range
            if np.any(indices >= vertex_count):
                invalid_idx = indices[indices >= vertex_count][0]
                errors.append(f"Index {invalid_idx} in submesh {idx} is out of vertex range {vertex_count}.")

        return "\n".join(errors)

    def _gen_submesh_offsets(self) -> np.ndarray:
        """
        Generate submesh offsets (triangle count offset for each submesh).
        Returns an empty array for single submesh, or array of offsets for multiple submeshes.
        """
        if len(self.triangle_indices) <= 1:
            return np.array([], dtype=np.uint32)

        offsets = []
        offset = 0
        for indices in self.triangle_indices:
            offsets.append(offset)
            offset += indices.shape[0] // 3  # Number of triangles in this submesh
        return np.array(offsets, dtype=np.uint32)
    # return submesh offsets
    def write_to(self, dst_path: Path | str | bytearray) -> np.ndarray:
        """
        Write mesh data to a file path or append to a bytearray.
        
        Args:
            dst_path: Path to write to, or bytearray to append to
        
        Returns:
            numpy array of submesh triangle offsets
        """
        error_msg = self.check()
        if error_msg:
            raise ValueError(f"Mesh validation failed:\n{error_msg}")

        # Convert data to bytes
        data_bytes = self._to_bytes()

        if isinstance(dst_path, (str, Path)):
            # Write to file
            with open(dst_path, 'wb') as f:
                f.write(data_bytes)
        elif isinstance(dst_path, bytearray):
            # Append to bytearray
            dst_path.extend(data_bytes)
        else:
            raise TypeError(f"dst_path must be a Path, str, or bytearray, got {type(dst_path)}")

        # Generate submesh offsets
        return self._gen_submesh_offsets()
    # return submesh offsets\
    def write_to_mesh(self) -> re.world.MeshResource:
        """Write mesh data to a MeshResource.
        
        Returns:
            MeshResource with the mesh data filled in.
        """
        error_msg = self.check()
        if error_msg:
            raise ValueError(f"Mesh validation failed:\n{error_msg}")

        # Calculate submesh offsets (triangle count offset for each submesh)
        submesh_offsets = self._gen_submesh_offsets()
        
        # Calculate total triangle count
        total_triangles = sum(indices.shape[0] // 3 for indices in self.triangle_indices)
        
        # Create mesh resource
        mesh = re.world.MeshResource()
        
        # Create empty mesh with proper parameters
        mesh.create_empty(
            submesh_offsets,
            self.vertex_count(),
            total_triangles,
            self.uv_count(),
            self.contained_normal(),
            self.contained_tangent()
        )
        # Copy buffers to mesh
        vertex_count = self.vertex_count()
        
        # Fill positions (aligned as float4)
        if vertex_count > 0:
            buffer = mesh.pos_buffer()
            assert buffer is not None
            pos_buffer = np.ndarray(vertex_count * 4, dtype=np.float32, buffer=buffer)
            pos_buffer[0::4] = self.position[:, 0]
            pos_buffer[1::4] = self.position[:, 1]
            pos_buffer[2::4] = self.position[:, 2]
            pos_buffer[3::4] = 0.0  # padding for alignment
    
        # Fill normals if present (aligned as float4)
        if self.contained_normal():
            buffer = mesh.normal_buffer()
            if buffer is not None:
                normal_buffer = np.ndarray(vertex_count * 4, dtype=np.float32, buffer=buffer)
                normal_buffer[0::4] = self.normal[:, 0]
                normal_buffer[1::4] = self.normal[:, 1]
                normal_buffer[2::4] = self.normal[:, 2]
                normal_buffer[3::4] = 0.0  # padding for alignment
        
        # Fill tangents if present (already float4)
        if self.contained_tangent():
            buffer = mesh.tangent_buffer()
            if buffer is not None:
                tangent_buffer = np.ndarray(vertex_count * 4, dtype=np.float32, buffer=buffer)
                tangent_buffer[0::4] = self.tangent[:, 0]
                tangent_buffer[1::4] = self.tangent[:, 1]
                tangent_buffer[2::4] = self.tangent[:, 2]
                tangent_buffer[3::4] = self.tangent[:, 3]
        
        # Fill UVs
        for i, uv_set in enumerate(self.uvs):
            if uv_set.shape[0] > 0:
                buffer = mesh.uv_buffer(i)
                if buffer is not None:
                    uv_buffer = np.ndarray(vertex_count * 2, dtype=np.float32, buffer=buffer)
                    uv_buffer[0::2] = uv_set[:, 0]
                    uv_buffer[1::2] = uv_set[:, 1]
        
        # Fill triangle indices
        buffer = mesh.triangle_indices_buffer()
        if buffer is not None:
            all_indices = np.concatenate(self.triangle_indices) if self.triangle_indices else np.array([], dtype=np.uint32)
            index_buffer = np.ndarray(all_indices.shape[0], dtype=np.uint32, buffer=buffer)
            index_buffer[:] = all_indices
        
        return mesh 

    def _to_bytes(self) -> bytes:
        """Convert all mesh data to bytes in the correct format."""
        buffer = bytearray()
        vertex_count = self.vertex_count()

        # Write positions (float3 padded to 16 bytes per vertex)
        if vertex_count > 0:
            pos_padded = np.zeros((vertex_count, 4), dtype=np.float32)
            pos_padded[:, :3] = self.position
            buffer.extend(pos_padded.tobytes())

        # Write normals (float3 padded to 16 bytes per vertex)
        if self.contained_normal():
            normal_padded = np.zeros((vertex_count, 4), dtype=np.float32)
            normal_padded[:, :3] = self.normal
            buffer.extend(normal_padded.tobytes())

        # Write tangents (float4 = 16 bytes per vertex)
        buffer.extend(self.tangent.tobytes())

        # Write UVs (float2 = 8 bytes per vertex per UV set)
        for uv_set in self.uvs:
            buffer.extend(uv_set.tobytes())

        # Write triangle indices (uint32 = 4 bytes per index)
        for indices in self.triangle_indices:
            buffer.extend(indices.astype(np.uint32).tobytes())

        return bytes(buffer)

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

    def add_vertex(self, position: np.ndarray | tuple[float, float, float]) -> int:
        """
        Add a new vertex with the given position.
        Returns the index of the added vertex.
        """
        pos_array = np.array(position, dtype=np.float32).reshape(1, 3)
        self.position = np.vstack([self.position, pos_array])
        return self.position.shape[0] - 1

    def add_submesh(self) -> int:
        """
        Add a new empty submesh.
        Returns the index of the added submesh.
        """
        self.triangle_indices.append(np.zeros(0, dtype=np.uint32))
        return len(self.triangle_indices) - 1

    def add_triangle(self, submesh_index: int, i0: int, i1: int, i2: int) -> None:
        """
        Add a triangle to the specified submesh.
        
        Args:
            submesh_index: Index of the submesh to add to
            i0, i1, i2: Vertex indices of the triangle
        """
        if submesh_index < 0 or submesh_index >= len(self.triangle_indices):
            raise IndexError(f"Submesh index {submesh_index} out of range [0, {len(self.triangle_indices)})")
        new_indices = np.array([i0, i1, i2], dtype=np.uint32)
        self.triangle_indices[submesh_index] = np.concatenate([self.triangle_indices[submesh_index], new_indices])

    def add_uv_set(self) -> int:
        """
        Add a new empty UV set.
        Returns the index of the added UV set.
        """
        self.uvs.append(np.zeros((0, 2), dtype=np.float32))
        return len(self.uvs) - 1

    def __repr__(self) -> str:
        return (
            f"MeshBuilder("
            f"vertices={self.vertex_count()}, "
            f"submeshes={self.submesh_count()}, "
            f"indices={self.indices_count()}, "
            f"normals={self.contained_normal()}, "
            f"tangents={self.contained_tangent()}, "
            f"uv_sets={self.uv_count()}"
            f")"
        )
