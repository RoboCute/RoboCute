import numpy as np


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
