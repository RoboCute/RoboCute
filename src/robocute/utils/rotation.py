import numpy as np
import math

def degrees_to_radians(degrees: float) -> float:
    """Convert degrees to radians.

    Args:
        degrees: Angle in degrees.

    Returns:
        Angle in radians.
    """
    import math

    return degrees * math.pi / 180

def euler_to_quaternion(euler_x: float, euler_y: float, euler_z: float) -> tuple:
    """
    Convert euler angles (in radians) to quaternion.
    Rotation order: ZYX (intrinsic rotations, equivalent to XYZ extrinsic).

    Args:
        euler_x: Rotation around X axis in radians.
        euler_y: Rotation around Y axis in radians.
        euler_z: Rotation around Z axis in radians.

    Returns:
        Quaternion as numpy array [x, y, z, w].
    """
    cx = math.cos(euler_x * 0.5)
    sx = math.sin(euler_x * 0.5)
    cy = math.cos(euler_y * 0.5)
    sy = math.sin(euler_y * 0.5)
    cz = math.cos(euler_z * 0.5)
    sz = math.sin(euler_z * 0.5)

    x = sx * cy * cz - cx * sy * sz
    y = cx * sy * cz + sx * cy * sz
    z = cx * cy * sz - sx * sy * cz
    w = cx * cy * cz + sx * sy * sz

    return (x, y, z, w)


def quaternion_multiply(q1: tuple, q2: tuple) -> tuple:
    """
    Multiply two quaternions (Hamilton product).
    Quaternion format: q = (x, y, z, w)

    Args:
        q1: First quaternion (x, y, z, w)
        q2: Second quaternion (x, y, z, w)

    Returns:
        Quaternion product q1 * q2 as tuple (x, y, z, w)
    """
    x1, y1, z1, w1 = q1
    x2, y2, z2, w2 = q2

    x = w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2
    y = w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2
    z = w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2
    w = w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2

    return (x, y, z, w)


def quaternion_conjugate(q: tuple) -> tuple:
    """
    Compute the conjugate of a quaternion.
    Quaternion format: q = (x, y, z, w)

    Args:
        q: Quaternion (x, y, z, w)

    Returns:
        Conjugate quaternion (-x, -y, -z, w)
    """
    x, y, z, w = q
    return (-x, -y, -z, w)


def quaternion_normalize(q: tuple) -> tuple:
    """
    Normalize a quaternion to unit length.
    Quaternion format: q = (x, y, z, w)

    Args:
        q: Quaternion (x, y, z, w)

    Returns:
        Normalized quaternion as tuple (x, y, z, w)
    """
    x, y, z, w = q
    norm = math.sqrt(x * x + y * y + z * z + w * w)
    if norm < 1e-10:
        return (0.0, 0.0, 0.0, 1.0)
    return (x / norm, y / norm, z / norm, w / norm)


def quaternion_rotate_vector(q: tuple, v: tuple) -> tuple:
    """
    Rotate a 3D vector by a quaternion.
    Quaternion format: q = (x, y, z, w)

    Args:
        q: Quaternion (x, y, z, w)
        v: Vector (vx, vy, vz)

    Returns:
        Rotated vector as tuple (vx', vy', vz')
    """
    # Convert vector to pure quaternion (x, y, z, 0)
    v_quat = (v[0], v[1], v[2], 0.0)
    q_conj = quaternion_conjugate(q)
    # q * v * q_conj
    temp = quaternion_multiply(q, v_quat)
    result = quaternion_multiply(temp, q_conj)
    return (result[0], result[1], result[2])


def q2R33(q):
    """
    将四元数转换为3x3旋转矩阵
    四元数格式: q = [w, x, y, z]

    Args:
        q: 四元数 [w, x, y, z]

    Returns:
        3x3旋转矩阵
    """
    w, x, y, z = q[0], q[1], q[2], q[3]
    return np.array(
        [
            [1 - 2 * (y**2 + z**2), 2 * (x * y - w * z), 2 * (x * z + w * y)],
            [2 * (x * y + w * z), 1 - 2 * (x**2 + z**2), 2 * (y * z - w * x)],
            [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x**2 + y**2)],
        ]
    )


def R332q(R):
    """
    将3x3旋转矩阵转换为四元数
    使用Shepperd's方法，数值稳定

    Args:
        R: 3x3旋转矩阵

    Returns:
        四元数 [w, x, y, z]
    """
    R = np.asarray(R)
    trace = R[0, 0] + R[1, 1] + R[2, 2]

    if trace > 0:
        s = np.sqrt(trace + 1.0) * 2.0  # s = 4 * w
        w = 0.25 * s
        x = (R[2, 1] - R[1, 2]) / s
        y = (R[0, 2] - R[2, 0]) / s
        z = (R[1, 0] - R[0, 1]) / s
    else:
        if R[0, 0] > R[1, 1] and R[0, 0] > R[2, 2]:
            s = np.sqrt(1.0 + R[0, 0] - R[1, 1] - R[2, 2]) * 2.0  # s = 4 * x
            w = (R[2, 1] - R[1, 2]) / s
            x = 0.25 * s
            y = (R[0, 1] + R[1, 0]) / s
            z = (R[0, 2] + R[2, 0]) / s
        elif R[1, 1] > R[2, 2]:
            s = np.sqrt(1.0 + R[1, 1] - R[0, 0] - R[2, 2]) * 2.0  # s = 4 * y
            w = (R[0, 2] - R[2, 0]) / s
            x = (R[0, 1] + R[1, 0]) / s
            y = 0.25 * s
            z = (R[1, 2] + R[2, 1]) / s
        else:
            s = np.sqrt(1.0 + R[2, 2] - R[0, 0] - R[1, 1]) * 2.0  # s = 4 * z
            w = (R[1, 0] - R[0, 1]) / s
            x = (R[0, 2] + R[2, 0]) / s
            y = (R[1, 2] + R[2, 1]) / s
            z = 0.25 * s

    return np.array([w, x, y, z])


def TRS2R33(t, r, s):
    """
    将平移、旋转、缩放组合成4x4变换矩阵
    变换顺序：先缩放，再旋转，最后平移

    Args:
        t: 平移向量 [x, y, z]
        r: 旋转四元数 [w, x, y, z]
        s: 缩放向量 [sx, sy, sz]

    Returns:
        4x4变换矩阵
    """
    R = q2R33(r)  # 3x3旋转矩阵
    S = np.diag(s)  # 3x3缩放矩阵

    # 构建4x4矩阵：先缩放，再旋转，最后平移
    M = np.eye(4)
    M[:3, :3] = S @ R  # 缩放和旋转的组合
    M[:3, 3] = t  # 平移
    return M


def R332TRS(A_):
    t = A_[:3, 3]
    N = A_[:3, :3]
    R = N
    max_iteration_count = 100
    for i in range(max_iteration_count):
        R_it = np.linalg.inv(R.T)
        R_next = 0.5 * (R + R_it)
        diff = R - R_next
        R = R_next
        n = np.sum(np.abs(diff))
        if n < 1e-4:
            break

    # print(f"Iteration count: {i}")
    S = np.linalg.inv(R) @ N
    if np.any(S != S):
        print("Non-zero entries found in decomposed scaling matrix")

    s = np.array([S[0, 0], S[1, 1], S[2, 2]])
    q = R332q(R)
    return t, q, s
