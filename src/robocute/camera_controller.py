"""
Camera controller module for managing camera position and rotation.
"""

import numpy as np
from typing import Optional, Tuple


class CameraController:
    """
    Manages camera position and quaternion rotation.
    
    Attributes:
        position: Camera position as 3D numpy array (x, y, z)
        rotation: Camera rotation as unit quaternion numpy array (w, x, y, z)
    """
    
    def __init__(
        self,
        position: Optional[np.ndarray] = None,
        rotation: Optional[np.ndarray] = None
    ):
        """
        Initialize a CameraController.
        
        Args:
            position: Initial camera position as 3D vector (x, y, z).
                     Defaults to origin (0, 0, 0).
            rotation: Initial camera rotation as quaternion (w, x, y, z).
                     Defaults to identity quaternion (1, 0, 0, 0).
        """
        if position is not None:
            self.position = np.asarray(position, dtype=np.float64).flatten()[:3]
        else:
            self.position = np.zeros(3, dtype=np.float64)
        
        if rotation is not None:
            self.rotation = np.asarray(rotation, dtype=np.float64).flatten()[:4]
            self.rotation = self._normalize_quaternion(self.rotation)
        else:
            self.rotation = np.array([1.0, 0.0, 0.0, 0.0], dtype=np.float64)
    
    def __repr__(self) -> str:
        return f"CameraController(position={self.position}, rotation={self.rotation})"
    
    def get_position(self) -> np.ndarray:
        """Return a copy of the camera position."""
        return self.position.copy()
    
    def set_position(self, position: np.ndarray) -> None:
        """Set the camera position."""
        self.position = np.asarray(position, dtype=np.float64).flatten()[:3]
    
    def get_rotation(self) -> np.ndarray:
        """Return a copy of the camera rotation quaternion."""
        return self.rotation.copy()
    
    def set_rotation(self, rotation: np.ndarray) -> None:
        """Set the camera rotation quaternion (will be normalized)."""
        self.rotation = np.asarray(rotation, dtype=np.float64).flatten()[:4]
        self.rotation = self._normalize_quaternion(self.rotation)
    
    def _normalize_quaternion(self, q: np.ndarray) -> np.ndarray:
        """
        Normalize a quaternion to unit length.
        
        Args:
            q: Quaternion as (w, x, y, z)
            
        Returns:
            Normalized quaternion
        """
        norm = np.linalg.norm(q)
        if norm > 1e-10:
            return q / norm
        return np.array([1.0, 0.0, 0.0, 0.0], dtype=np.float64)
    
    def translate(self, offset: np.ndarray) -> None:
        """
        Translate the camera position by an offset.
        
        Args:
            offset: Translation vector (x, y, z)
        """
        offset = np.asarray(offset, dtype=np.float64).flatten()[:3]
        self.position += offset
    
    def translate_local(self, offset: np.ndarray) -> None:
        """
        Translate the camera position in local (camera) space.
        
        Args:
            offset: Translation vector in local space (forward, right, up)
        """
        offset = np.asarray(offset, dtype=np.float64).flatten()[:3]
        # Transform local offset to world space using rotation
        world_offset = self._rotate_vector_by_quaternion(offset, self.rotation)
        self.position += world_offset
    
    def rotate(self, axis: np.ndarray, angle: float) -> None:
        """
        Rotate the camera around an axis by a given angle.
        
        Args:
            axis: Rotation axis as 3D vector (will be normalized)
            angle: Rotation angle in radians
        """
        axis = np.asarray(axis, dtype=np.float64).flatten()[:3]
        axis_norm = np.linalg.norm(axis)
        if axis_norm < 1e-10:
            return
        axis = axis / axis_norm
        
        # Create quaternion from axis-angle
        half_angle = angle / 2.0
        sin_half = np.sin(half_angle)
        q = np.array([
            np.cos(half_angle),
            axis[0] * sin_half,
            axis[1] * sin_half,
            axis[2] * sin_half
        ], dtype=np.float64)
        
        # Apply rotation: new_rotation = q * current_rotation
        self.rotation = self._quaternion_multiply(q, self.rotation)
        self.rotation = self._normalize_quaternion(self.rotation)
    
    def rotate_euler(self, pitch: float, yaw: float, roll: float) -> None:
        """
        Rotate the camera using Euler angles (in radians).
        
        Applies rotations in order: roll (Z), pitch (X), yaw (Y).
        
        Args:
            pitch: Rotation around X-axis in radians
            yaw: Rotation around Y-axis in radians
            roll: Rotation around Z-axis in radians
        """
        # Create quaternions for each axis
        qx = self._axis_angle_to_quaternion(np.array([1, 0, 0]), pitch)
        qy = self._axis_angle_to_quaternion(np.array([0, 1, 0]), yaw)
        qz = self._axis_angle_to_quaternion(np.array([0, 0, 1]), roll)
        
        # Combine rotations: roll * pitch * yaw (applied right to left)
        q = self._quaternion_multiply(qz, self._quaternion_multiply(qx, qy))
        self.rotation = self._quaternion_multiply(q, self.rotation)
        self.rotation = self._normalize_quaternion(self.rotation)
    
    def look_at(self, target: np.ndarray, up: Optional[np.ndarray] = None) -> None:
        """
        Set camera rotation to look at a target point.
        
        Args:
            target: Target position to look at
            up: Up vector (defaults to world up (0, 1, 0))
        """
        target = np.asarray(target, dtype=np.float64).flatten()[:3]
        if up is None:
            up = np.array([0.0, 1.0, 0.0], dtype=np.float64)
        else:
            up = np.asarray(up, dtype=np.float64).flatten()[:3]
        
        # Calculate forward vector (from camera to target)
        forward = target - self.position
        forward_norm = np.linalg.norm(forward)
        if forward_norm < 1e-10:
            return
        forward = forward / forward_norm
        
        # Calculate right vector
        right = np.cross(forward, up)
        right_norm = np.linalg.norm(right)
        if right_norm < 1e-10:
            # Forward is parallel to up, use a different up vector
            up = np.array([1.0, 0.0, 0.0], dtype=np.float64)
            right = np.cross(forward, up)
            right_norm = np.linalg.norm(right)
            if right_norm < 1e-10:
                up = np.array([0.0, 0.0, 1.0], dtype=np.float64)
                right = np.cross(forward, up)
                right_norm = np.linalg.norm(right)
        right = right / right_norm
        
        # Recalculate up vector
        up = np.cross(right, forward)
        up = up / np.linalg.norm(up)
        
        # Create rotation matrix
        rotation_matrix = np.array([
            [right[0], up[0], -forward[0]],
            [right[1], up[1], -forward[1]],
            [right[2], up[2], -forward[2]]
        ], dtype=np.float64)
        
        # Convert rotation matrix to quaternion
        self.rotation = self._rotation_matrix_to_quaternion(rotation_matrix)
    
    def face_forward(self, pos: np.ndarray) -> None:
        """
        Change camera rotation so that the camera's z-axis faces forward to a position.
        
        This is similar to look_at, but the camera's +Z axis points toward the target
        instead of the -Z axis.
        
        Args:
            pos: Target position to face toward
        """
        target = np.asarray(pos, dtype=np.float64).flatten()[:3]
        
        # Calculate forward vector (from camera to target, for +Z axis)
        forward = target - self.position
        forward_norm = np.linalg.norm(forward)
        if forward_norm < 1e-10:
            return
        forward = forward / forward_norm
        
        # Use world up vector
        up = np.array([0.0, 1.0, 0.0], dtype=np.float64)
        
        # Calculate right vector
        right = np.cross(up, forward)
        right_norm = np.linalg.norm(right)
        if right_norm < 1e-10:
            # Forward is parallel to up, use a different up vector
            up = np.array([1.0, 0.0, 0.0], dtype=np.float64)
            right = np.cross(up, forward)
            right_norm = np.linalg.norm(right)
            if right_norm < 1e-10:
                up = np.array([0.0, 0.0, 1.0], dtype=np.float64)
                right = np.cross(up, forward)
                right_norm = np.linalg.norm(right)
        right = right / right_norm
        
        # Recalculate up vector
        up = np.cross(forward, right)
        up = up / np.linalg.norm(up)
        
        # Create rotation matrix (forward is now +Z)
        rotation_matrix = np.array([
            [right[0], up[0], forward[0]],
            [right[1], up[1], forward[1]],
            [right[2], up[2], forward[2]]
        ], dtype=np.float64)
        
        # Convert rotation matrix to quaternion
        self.rotation = self._rotation_matrix_to_quaternion(rotation_matrix)
    
    def get_forward_vector(self) -> np.ndarray:
        """Get the camera's forward direction vector."""
        # Default forward is -Z, rotate by quaternion
        forward = np.array([0.0, 0.0, -1.0], dtype=np.float64)
        return self._rotate_vector_by_quaternion(forward, self.rotation)
    
    def get_right_vector(self) -> np.ndarray:
        """Get the camera's right direction vector."""
        right = np.array([1.0, 0.0, 0.0], dtype=np.float64)
        return self._rotate_vector_by_quaternion(right, self.rotation)
    
    def get_up_vector(self) -> np.ndarray:
        """Get the camera's up direction vector."""
        up = np.array([0.0, 1.0, 0.0], dtype=np.float64)
        return self._rotate_vector_by_quaternion(up, self.rotation)
    
    def get_transform_matrix(self) -> np.ndarray:
        """
        Get the 4x4 transformation matrix representing camera pose.
        
        Returns:
            4x4 transformation matrix
        """
        rotation_matrix = self._quaternion_to_rotation_matrix(self.rotation)
        transform = np.eye(4, dtype=np.float64)
        transform[:3, :3] = rotation_matrix
        transform[:3, 3] = self.position
        return transform
    
    def get_view_matrix(self) -> np.ndarray:
        """
        Get the 4x4 view matrix for rendering.
        
        Returns:
            4x4 view matrix (inverse of transform matrix)
        """
        rotation_matrix = self._quaternion_to_rotation_matrix(self.rotation)
        view = np.eye(4, dtype=np.float64)
        view[:3, :3] = rotation_matrix.T
        view[:3, 3] = -rotation_matrix.T @ self.position
        return view
    
    def to_array(self) -> np.ndarray:
        """
        Serialize camera state to a numpy array.
        
        Returns:
            Array of shape (7,) containing [px, py, pz, qw, qx, qy, qz]
        """
        return np.concatenate([self.position, self.rotation])
    
    @classmethod
    def from_array(cls, data: np.ndarray) -> "CameraController":
        """
        Deserialize camera state from a numpy array.
        
        Args:
            data: Array of shape (7,) containing [px, py, pz, qw, qx, qy, qz]
            
        Returns:
            CameraController instance
        """
        data = np.asarray(data, dtype=np.float64).flatten()
        if len(data) < 7:
            raise ValueError("Data array must have at least 7 elements")
        position = data[:3]
        rotation = data[3:7]
        return cls(position=position, rotation=rotation)
    
    def copy(self) -> "CameraController":
        """Create a copy of this camera controller."""
        return CameraController(
            position=self.position.copy(),
            rotation=self.rotation.copy()
        )
    
    # Utility functions for quaternion operations
    
    def _quaternion_multiply(self, q1: np.ndarray, q2: np.ndarray) -> np.ndarray:
        """
        Multiply two quaternions.
        
        Args:
            q1: First quaternion (w, x, y, z)
            q2: Second quaternion (w, x, y, z)
            
        Returns:
            Product quaternion q1 * q2
        """
        w1, x1, y1, z1 = q1
        w2, x2, y2, z2 = q2
        return np.array([
            w1*w2 - x1*x2 - y1*y2 - z1*z2,
            w1*x2 + x1*w2 + y1*z2 - z1*y2,
            w1*y2 - x1*z2 + y1*w2 + z1*x2,
            w1*z2 + x1*y2 - y1*x2 + z1*w2
        ], dtype=np.float64)
    
    def _quaternion_conjugate(self, q: np.ndarray) -> np.ndarray:
        """
        Get the conjugate of a quaternion.
        
        Args:
            q: Quaternion (w, x, y, z)
            
        Returns:
            Conjugate quaternion (w, -x, -y, -z)
        """
        return np.array([q[0], -q[1], -q[2], -q[3]], dtype=np.float64)
    
    def _rotate_vector_by_quaternion(self, v: np.ndarray, q: np.ndarray) -> np.ndarray:
        """
        Rotate a vector by a quaternion.
        
        Args:
            v: 3D vector to rotate
            q: Rotation quaternion (w, x, y, z)
            
        Returns:
            Rotated vector
        """
        v = np.asarray(v, dtype=np.float64).flatten()[:3]
        # Convert vector to quaternion (0, x, y, z)
        vq = np.array([0.0, v[0], v[1], v[2]], dtype=np.float64)
        # Rotate: q * v * q^-1
        q_conj = self._quaternion_conjugate(q)
        result = self._quaternion_multiply(self._quaternion_multiply(q, vq), q_conj)
        return result[1:]
    
    def _axis_angle_to_quaternion(self, axis: np.ndarray, angle: float) -> np.ndarray:
        """
        Convert axis-angle to quaternion.
        
        Args:
            axis: Rotation axis (will be normalized)
            angle: Rotation angle in radians
            
        Returns:
            Quaternion (w, x, y, z)
        """
        axis = np.asarray(axis, dtype=np.float64).flatten()[:3]
        axis_norm = np.linalg.norm(axis)
        if axis_norm < 1e-10:
            return np.array([1.0, 0.0, 0.0, 0.0], dtype=np.float64)
        axis = axis / axis_norm
        half_angle = angle / 2.0
        sin_half = np.sin(half_angle)
        return np.array([
            np.cos(half_angle),
            axis[0] * sin_half,
            axis[1] * sin_half,
            axis[2] * sin_half
        ], dtype=np.float64)
    
    def _quaternion_to_rotation_matrix(self, q: np.ndarray) -> np.ndarray:
        """
        Convert quaternion to 3x3 rotation matrix.
        
        Args:
            q: Quaternion (w, x, y, z)
            
        Returns:
            3x3 rotation matrix
        """
        w, x, y, z = q
        # Normalize quaternion
        norm = np.linalg.norm(q)
        if norm > 1e-10:
            w, x, y, z = q / norm
        
        return np.array([
            [1 - 2*(y*y + z*z), 2*(x*y - w*z), 2*(x*z + w*y)],
            [2*(x*y + w*z), 1 - 2*(x*x + z*z), 2*(y*z - w*x)],
            [2*(x*z - w*y), 2*(y*z + w*x), 1 - 2*(x*x + y*y)]
        ], dtype=np.float64)
    
    def _rotation_matrix_to_quaternion(self, m: np.ndarray) -> np.ndarray:
        """
        Convert 3x3 rotation matrix to quaternion.
        
        Args:
            m: 3x3 rotation matrix
            
        Returns:
            Quaternion (w, x, y, z)
        """
        trace = m[0, 0] + m[1, 1] + m[2, 2]
        
        if trace > 0:
            s = 0.5 / np.sqrt(trace + 1.0)
            w = 0.25 / s
            x = (m[2, 1] - m[1, 2]) * s
            y = (m[0, 2] - m[2, 0]) * s
            z = (m[1, 0] - m[0, 1]) * s
        elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
            s = 2.0 * np.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2])
            w = (m[2, 1] - m[1, 2]) / s
            x = 0.25 * s
            y = (m[0, 1] + m[1, 0]) / s
            z = (m[0, 2] + m[2, 0]) / s
        elif m[1, 1] > m[2, 2]:
            s = 2.0 * np.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2])
            w = (m[0, 2] - m[2, 0]) / s
            x = (m[0, 1] + m[1, 0]) / s
            y = 0.25 * s
            z = (m[1, 2] + m[2, 1]) / s
        else:
            s = 2.0 * np.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1])
            w = (m[1, 0] - m[0, 1]) / s
            x = (m[0, 2] + m[2, 0]) / s
            y = (m[1, 2] + m[2, 1]) / s
            z = 0.25 * s
        
        return self._normalize_quaternion(np.array([w, x, y, z], dtype=np.float64))


# Utility functions for camera interpolation

def interpolate_cameras(
    cam1: CameraController,
    cam2: CameraController,
    t: float
) -> CameraController:
    """
    Linearly interpolate between two camera states.
    
    Args:
        cam1: Starting camera state
        cam2: Ending camera state
        t: Interpolation parameter in range [0, 1]
        
    Returns:
        Interpolated CameraController
    """
    t = np.clip(float(t), 0.0, 1.0)
    
    # Linear interpolation for position
    position = (1 - t) * cam1.position + t * cam2.position
    
    # Spherical linear interpolation (slerp) for rotation
    rotation = slerp(cam1.rotation, cam2.rotation, t)
    
    return CameraController(position=position, rotation=rotation)


def slerp(q1: np.ndarray, q2: np.ndarray, t: float) -> np.ndarray:
    """
    Spherical linear interpolation between two quaternions.
    
    Args:
        q1: Start quaternion (w, x, y, z)
        q2: End quaternion (w, x, y, z)
        t: Interpolation parameter in range [0, 1]
        
    Returns:
        Interpolated quaternion
    """
    t = np.clip(float(t), 0.0, 1.0)
    q1 = np.asarray(q1, dtype=np.float64)
    q2 = np.asarray(q2, dtype=np.float64)
    
    # Normalize quaternions
    q1 = q1 / np.linalg.norm(q1)
    q2 = q2 / np.linalg.norm(q2)
    
    # Calculate dot product
    dot = np.dot(q1, q2)
    
    # If the dot product is negative, negate one quaternion to take shorter path
    if dot < 0.0:
        q2 = -q2
        dot = -dot
    
    # Clamp dot product to stay within domain of acos
    dot = np.clip(dot, -1.0, 1.0)
    
    # If quaternions are very close, use linear interpolation
    if dot > 0.9995:
        result = q1 + t * (q2 - q1)
        return result / np.linalg.norm(result)
    
    # Calculate spherical interpolation
    theta_0 = np.arccos(dot)
    theta = theta_0 * t
    
    sin_theta = np.sin(theta)
    sin_theta_0 = np.sin(theta_0)
    
    s1 = np.cos(theta) - dot * sin_theta / sin_theta_0
    s2 = sin_theta / sin_theta_0
    
    result = s1 * q1 + s2 * q2
    return result / np.linalg.norm(result)


def quaternion_to_euler(q: np.ndarray) -> Tuple[float, float, float]:
    """
    Convert quaternion to Euler angles (pitch, yaw, roll) in radians.
    
    Uses ZYX rotation order (yaw, then pitch, then roll).
    
    Args:
        q: Quaternion (w, x, y, z)
        
    Returns:
        Tuple of (pitch, yaw, roll) in radians
    """
    q = np.asarray(q, dtype=np.float64)
    norm = np.linalg.norm(q)
    if norm > 1e-10:
        q = q / norm
    
    w, x, y, z = q
    
    # ZYX rotation order extraction (standard aerospace sequence)
    # Roll (Z-axis rotation)
    sinr_cosp = 2.0 * (w * z + x * y)
    cosr_cosp = 1.0 - 2.0 * (z * z + x * x)
    roll = np.arctan2(sinr_cosp, cosr_cosp)
    
    # Pitch (X-axis rotation)
    sinp = 2.0 * (w * x - y * z)
    if np.abs(sinp) >= 1:
        pitch = np.copysign(np.pi / 2, sinp)
    else:
        pitch = np.arcsin(sinp)
    
    # Yaw (Y-axis rotation)
    siny_cosp = 2.0 * (w * y + z * x)
    cosy_cosp = 1.0 - 2.0 * (x * x + y * y)
    yaw = np.arctan2(siny_cosp, cosy_cosp)
    
    return pitch, yaw, roll


def euler_to_quaternion(pitch: float, yaw: float, roll: float) -> np.ndarray:
    """
    Convert Euler angles (pitch, yaw, roll) to quaternion.
    
    Uses ZYX rotation order (yaw, then pitch, then roll).
    
    Args:
        pitch: Rotation around X-axis in radians
        yaw: Rotation around Y-axis in radians
        roll: Rotation around Z-axis in radians
        
    Returns:
        Quaternion (w, x, y, z)
    """
    cy = np.cos(yaw * 0.5)
    sy = np.sin(yaw * 0.5)
    cp = np.cos(pitch * 0.5)
    sp = np.sin(pitch * 0.5)
    cr = np.cos(roll * 0.5)
    sr = np.sin(roll * 0.5)
    
    # ZYX rotation order (standard aerospace sequence): q = qz * qy * qx
    w = cr * cy * cp + sr * sy * sp
    x = cr * cy * sp - sr * sy * cp
    y = cr * sy * cp + sr * cy * sp
    z = sr * cy * cp - cr * sy * sp
    
    q = np.array([w, x, y, z], dtype=np.float64)
    return q / np.linalg.norm(q)
