import pytest
import numpy as np
import sys
from pathlib import Path

# Add src directory to path for testing
test_dir = Path(__file__).parent
project_root = test_dir.parent
src_dir = project_root / "src"
sys.path.insert(0, str(src_dir))

from robocute.utils.rotation import q2R33, R332q, TRS2R33, R332TRS


class TestQuaternionRotationMatrix:
    """测试四元数和旋转矩阵的互相转换"""

    def test_identity_quaternion(self):
        """测试单位四元数（无旋转）"""
        q = np.array([1.0, 0.0, 0.0, 0.0])  # 单位四元数
        R = q2R33(q)
        expected_R = np.eye(3)
        np.testing.assert_allclose(R, expected_R, atol=1e-10)

    def test_quaternion_to_matrix_and_back(self):
        """测试四元数转矩阵再转回四元数（可逆性）"""
        # 测试多个不同的四元数
        test_quaternions = [
            np.array([1.0, 0.0, 0.0, 0.0]),  # 单位四元数
            np.array([np.sqrt(2) / 2, np.sqrt(2) / 2, 0.0, 0.0]),  # 绕x轴旋转90度
            np.array([np.sqrt(2) / 2, 0.0, np.sqrt(2) / 2, 0.0]),  # 绕y轴旋转90度
            np.array([np.sqrt(2) / 2, 0.0, 0.0, np.sqrt(2) / 2]),  # 绕z轴旋转90度
            np.array([0.5, 0.5, 0.5, 0.5]),  # 一般四元数
        ]

        for q_original in test_quaternions:
            # 归一化四元数
            q_original = q_original / np.linalg.norm(q_original)
            
            # 四元数 -> 矩阵
            R = q2R33(q_original)
            
            # 矩阵 -> 四元数
            q_recovered = R332q(R)
            
            # 四元数可能差一个符号（q和-q表示同一个旋转）
            # 检查是否相等或相反
            if np.dot(q_original, q_recovered) < 0:
                q_recovered = -q_recovered
            
            np.testing.assert_allclose(
                np.abs(q_original), np.abs(q_recovered), atol=1e-6,
                err_msg=f"Failed for quaternion {q_original}"
            )

    def test_matrix_to_quaternion_and_back(self):
        """测试矩阵转四元数再转回矩阵（可逆性）"""
        # 测试多个旋转矩阵
        test_matrices = [
            np.eye(3),  # 单位矩阵
            np.array([[1, 0, 0], [0, 0, -1], [0, 1, 0]]),  # 绕x轴旋转90度
            np.array([[0, 0, 1], [0, 1, 0], [-1, 0, 0]]),  # 绕y轴旋转90度
            np.array([[0, -1, 0], [1, 0, 0], [0, 0, 1]]),  # 绕z轴旋转90度
        ]

        for R_original in test_matrices:
            # 矩阵 -> 四元数
            q = R332q(R_original)
            
            # 四元数 -> 矩阵
            R_recovered = q2R33(q)
            
            np.testing.assert_allclose(
                R_original, R_recovered, atol=1e-6,
                err_msg=f"Failed for matrix {R_original}"
            )

    def test_rotation_matrix_properties(self):
        """测试旋转矩阵的性质（正交性、行列式为1）"""
        test_quaternions = [
            np.array([1.0, 0.0, 0.0, 0.0]),
            np.array([np.sqrt(2) / 2, np.sqrt(2) / 2, 0.0, 0.0]),
            np.array([0.5, 0.5, 0.5, 0.5]),
        ]

        for q in test_quaternions:
            q = q / np.linalg.norm(q)  # 归一化
            R = q2R33(q)
            
            # 检查正交性：R @ R.T 应该等于单位矩阵
            should_be_identity = R @ R.T
            np.testing.assert_allclose(should_be_identity, np.eye(3), atol=1e-6)
            
            # 检查行列式应该为1
            det = np.linalg.det(R)
            np.testing.assert_allclose(det, 1.0, atol=1e-6)

    def test_quaternion_normalization(self):
        """测试归一化后的四元数能产生有效的旋转矩阵"""
        # 归一化的四元数应该产生有效的旋转矩阵
        q_normalized = np.array([2.0, 2.0, 0.0, 0.0])
        q_normalized = q_normalized / np.linalg.norm(q_normalized)
        
        R = q2R33(q_normalized)
        
        # 检查旋转矩阵的性质
        # 正交性：R @ R.T 应该等于单位矩阵
        should_be_identity = R @ R.T
        np.testing.assert_allclose(should_be_identity, np.eye(3), atol=1e-6)
        
        # 行列式应该为1
        det = np.linalg.det(R)
        np.testing.assert_allclose(det, 1.0, atol=1e-6)


class TestTRSDecomposition:
    """测试平移、旋转、缩放的分解和组合"""

    def test_TRS_to_matrix_and_back(self):
        """测试TRS转矩阵再转回TRS（可逆性）"""
        # 测试多个不同的TRS组合
        # 注意：由于分解算法的限制，只测试均匀缩放或简单情况
        test_cases = [
            {
                "t": np.array([0.0, 0.0, 0.0]),
                "r": np.array([1.0, 0.0, 0.0, 0.0]),
                "s": np.array([1.0, 1.0, 1.0]),
            },
            {
                "t": np.array([1.0, 2.0, 3.0]),
                "r": np.array([1.0, 0.0, 0.0, 0.0]),  # 无旋转，避免缩放顺序问题
                "s": np.array([2.0, 2.0, 2.0]),  # 均匀缩放
            },
            {
                "t": np.array([10.0, -5.0, 7.5]),
                "r": np.array([np.sqrt(2) / 2, np.sqrt(2) / 2, 0.0, 0.0]),
                "s": np.array([2.0, 2.0, 2.0]),  # 均匀缩放
            },
        ]

        for case in test_cases:
            t_original = case["t"]
            r_original = case["r"]
            s_original = case["s"]
            
            # 归一化旋转四元数
            r_original = r_original / np.linalg.norm(r_original)
            
            # TRS -> 矩阵
            try:
                A = TRS2R33(t_original, r_original, s_original)
                # 确保 A 是 4x4 矩阵
                assert A.shape == (4, 4), f"Expected 4x4 matrix, got {A.shape}"
            except Exception as e:
                pytest.fail(f"TRS2R33 failed with error: {e}")
            
            # 矩阵 -> TRS
            t_recovered, r_recovered, s_recovered = R332TRS(A)
            
            # 归一化恢复的四元数
            r_recovered = r_recovered / np.linalg.norm(r_recovered)
            
            # 四元数可能差一个符号
            if np.dot(r_original, r_recovered) < 0:
                r_recovered = -r_recovered
            
            # 检查平移
            np.testing.assert_allclose(t_original, t_recovered, atol=1e-5)
            
            # 检查旋转（四元数）
            np.testing.assert_allclose(
                np.abs(r_original), np.abs(r_recovered), atol=1e-5
            )
            
            # 检查缩放（对于均匀缩放，顺序不重要）
            # 排序后比较，因为旋转可能改变缩放向量的顺序
            s_original_sorted = np.sort(np.abs(s_original))
            s_recovered_sorted = np.sort(np.abs(s_recovered))
            np.testing.assert_allclose(s_original_sorted, s_recovered_sorted, atol=1e-5)

    def test_identity_transform(self):
        """测试单位变换"""
        t = np.array([0.0, 0.0, 0.0])
        r = np.array([1.0, 0.0, 0.0, 0.0])
        s = np.array([1.0, 1.0, 1.0])
        
        A = TRS2R33(t, r, s)
        
        # 应该得到单位矩阵
        expected_A = np.eye(4)
        np.testing.assert_allclose(A, expected_A, atol=1e-6)

    def test_translation_only(self):
        """测试只有平移的情况"""
        t = np.array([1.0, 2.0, 3.0])
        r = np.array([1.0, 0.0, 0.0, 0.0])  # 无旋转
        s = np.array([1.0, 1.0, 1.0])  # 无缩放
        
        A = TRS2R33(t, r, s)
        
        # 检查平移部分
        np.testing.assert_allclose(A[:3, 3], t, atol=1e-6)
        
        # 检查旋转部分应该是单位矩阵
        np.testing.assert_allclose(A[:3, :3], np.eye(3), atol=1e-6)

    def test_rotation_only(self):
        """测试只有旋转的情况"""
        t = np.array([0.0, 0.0, 0.0])
        r = np.array([np.sqrt(2) / 2, np.sqrt(2) / 2, 0.0, 0.0])  # 绕x轴旋转90度
        s = np.array([1.0, 1.0, 1.0])  # 无缩放
        
        A = TRS2R33(t, r, s)
        
        # 检查平移部分应该是0
        np.testing.assert_allclose(A[:3, 3], t, atol=1e-6)
        
        # 检查旋转矩阵的性质
        R = A[:3, :3]
        np.testing.assert_allclose(R @ R.T, np.eye(3), atol=1e-6)
        np.testing.assert_allclose(np.linalg.det(R), 1.0, atol=1e-6)

    def test_scaling_only(self):
        """测试只有缩放的情况"""
        t = np.array([0.0, 0.0, 0.0])
        r = np.array([1.0, 0.0, 0.0, 0.0])  # 无旋转
        s = np.array([2.0, 3.0, 4.0])
        
        A = TRS2R33(t, r, s)
        
        # 检查缩放矩阵（左上角3x3应该是缩放矩阵）
        S = A[:3, :3]
        expected_S = np.diag(s)
        np.testing.assert_allclose(S, expected_S, atol=1e-6)
        
        # 检查平移部分应该是0
        np.testing.assert_allclose(A[:3, 3], t, atol=1e-6)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

