#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ROS2 基础测试脚本

本脚本演示 ROS2 的基本功能:
1. 初始化 ROS2 节点
2. 创建发布者 (Publisher) 发布话题
3. 创建订阅者 (Subscriber) 接收话题
4. 使用定时器发布消息

依赖安装:
    # 确保已安装 ROS2 (Humble/Iron/Jazzy 等)
    # 如果使用虚拟环境，需要 source ROS2 环境:
    source /opt/ros/humble/setup.bash

运行示例:
    # 终端 1 - 运行本脚本
    python samples/test_ros2.py
    
    # 终端 2 - 查看话题列表
    ros2 topic list
    
    # 终端 3 - 手动发布测试消息
    ros2 topic pub /test_topic std_msgs/msg/String "{data: 'hello'}"
    
    # 查看节点图
    ros2 run rqt_graph rqt_graph
    
    # Mock 模式 (无需安装 ROS2)
    python samples/test_ros2.py --mock
"""
import sys
import time
import os

# 检查是否使用 mock 模式
_use_mock = '--mock' in sys.argv or os.environ.get('ROS2_MOCK', '').lower() in ('1', 'true', 'yes')

if _use_mock:
    # 从 mock 模块导入
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from samples.ros2_mock import install_mock, MockRclpy as rclpy, MockNode as Node, MockString as String
    install_mock()
else:
    # 尝试导入真实的 ROS2 库
    try:
        import rclpy
        from rclpy.node import Node
        from std_msgs.msg import String
    except ImportError:
        print("=" * 60)
        print("错误: 未找到 ROS2 库。请确保已安装 ROS2 并 source 环境:")
        print("")
        print("  Linux:")
        print("    source /opt/ros/humble/setup.bash")
        print("")
        print("  Windows:")
        print(r"    call C:\dev\ros2_humble\local_setup.bat")
        print("")
        print("  或者使用 Mock 模式运行 (无需 ROS2):")
        print("    uv run samples/test_ros2.py --mock")
        print("    # 或")
        print("    ROS2_MOCK=1 uv run samples/test_ros2.py")
        print("=" * 60)
        sys.exit(1)


class TestNode(Node):
    """测试用的 ROS2 节点"""

    def __init__(self):
        super().__init__('test_ros2_node')
        
        # 创建发布者，发布到 /test_topic 话题
        self.publisher_ = self.create_publisher(String, 'test_topic', 10)
        
        # 创建订阅者，订阅 /test_topic 话题
        self.subscription = self.create_subscription(
            String,
            'test_topic',
            self.listener_callback,
            10)
        
        # 创建定时器，每秒发布一次消息
        self.timer = self.create_timer(1.0, self.timer_callback)
        
        self.msg_count = 0
        self.get_logger().info('Test ROS2 节点已启动!')
        self.get_logger().info('发布话题: /test_topic')
        self.get_logger().info('订阅话题: /test_topic')

    def timer_callback(self):
        """定时器回调 - 发布消息"""
        msg = String()
        msg.data = f'Hello ROS2! Message #{self.msg_count}'
        self.publisher_.publish(msg)
        self.get_logger().info(f'发布: "{msg.data}"')
        self.msg_count += 1

    def listener_callback(self, msg):
        """订阅回调 - 接收消息"""
        self.get_logger().info(f'收到: "{msg.data}"')


def test_ros2_installation():
    """测试 ROS2 是否安装正确"""
    print("=" * 50)
    print("ROS2 安装测试")
    print("=" * 50)
    
    try:
        # 获取 ROS2 版本
        import rclpy
        version = getattr(rclpy, '__version__', 'unknown')
        print(f"[OK] rclpy 版本: {version}")
        
        if 'mock' in str(version).lower():
            print("  (Running in Mock Mode)")
        
        # 检查是否可以使用 ROS2
        rclpy.init()
        print("[OK] ROS2 初始化成功")
        
        node = rclpy.create_node('test_installation')
        print(f"[OK] 节点创建成功")
        print(f"  节点名称: {node.get_name()}")
        print(f"  命名空间: {node.get_namespace()}")
        
        node.destroy_node()
        rclpy.shutdown()
        print("[OK] ROS2 关闭成功")
        
        print("\n[PASS] ROS2 环境检查通过!")
        return True
        
    except Exception as e:
        print(f"[FAIL] 错误: {e}")
        return False


def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="ROS2 基础测试",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s                    # 运行完整测试（节点 + 发布/订阅）
  %(prog)s --check-only       # 仅检查 ROS2 安装
  %(prog)s --duration 10      # 运行 10 秒后退出
  %(prog)s --mock             # 使用 Mock 模式运行 (无需 ROS2)
        """
    )
    parser.add_argument(
        '-c', '--check-only',
        action='store_true',
        help='仅检查 ROS2 安装，不运行节点'
    )
    parser.add_argument(
        '-d', '--duration',
        type=float,
        default=0,
        help='运行指定秒数后退出 (0 = 一直运行，默认: 0)'
    )
    parser.add_argument(
        '--mock',
        action='store_true',
        help='使用 Mock 模式运行 (无需安装 ROS2)'
    )
    args = parser.parse_args()
    
    # 仅检查安装
    if args.check_only:
        success = test_ros2_installation()
        sys.exit(0 if success else 1)
    
    # 检查安装
    if not test_ros2_installation():
        sys.exit(1)
    
    print("\n" + "=" * 50)
    print("启动 ROS2 测试节点")
    print("=" * 50)
    print("按 Ctrl+C 退出\n")
    
    # 初始化 ROS2
    rclpy.init()
    
    # 创建节点
    node = TestNode()
    
    # 记录开始时间
    start_time = time.time()
    
    try:
        # 主循环
        while rclpy.ok():
            rclpy.spin_once(node, timeout_sec=0.1)
            
            # 检查运行时间
            if args.duration > 0 and (time.time() - start_time) >= args.duration:
                node.get_logger().info(f'运行时间达到 {args.duration} 秒，正在退出...')
                break
                
    except KeyboardInterrupt:
        node.get_logger().info('用户中断')
    finally:
        # 清理
        node.destroy_node()
        rclpy.shutdown()
        print("\nROS2 测试节点已关闭")


if __name__ == '__main__':
    main()
