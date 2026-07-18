#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ROS2 Mock Module - 模拟 ROS2 接口用于测试

当系统未安装 ROS2 时，此模块提供基本的 mock 实现，
允许测试脚本运行并展示其功能结构。
"""

import sys
import time
from typing import Callable, Any

# Mock rclpy module
class MockRclpy:
    """模拟 rclpy 模块"""
    
    _initialized = False
    _nodes = []
    
    __version__ = "mock-1.0.0"
    
    @classmethod
    def init(cls, args=None):
        """初始化 ROS2 (mock)"""
        if cls._initialized:
            return
        cls._initialized = True
        print("[Mock] ROS2 init success")
    
    @classmethod
    def shutdown(cls):
        """关闭 ROS2 (mock)"""
        cls._initialized = False
        cls._nodes.clear()
        print("[Mock] ROS2 shutdown")
    
    @classmethod
    def ok(cls):
        """检查 ROS2 是否运行中"""
        return cls._initialized
    
    @classmethod
    def create_node(cls, name: str, **kwargs):
        """创建节点"""
        node = MockNode(name, **kwargs)
        cls._nodes.append(node)
        return node
    
    @classmethod
    def spin_once(cls, node, timeout_sec: float = 0.1):
        """执行一次 spin (mock - 仅休眠)"""
        time.sleep(timeout_sec)


# Mock Node class
class MockNode:
    """模拟 ROS2 节点"""
    
    def __init__(self, name: str, **kwargs):
        self._name = name
        self._namespace = kwargs.get('namespace', '/')
        self._publishers = {}
        self._subscriptions = {}
        self._timers = []
        self._logger = MockLogger(name)
        print(f"[Mock] Node created: {name}")
    
    def get_name(self):
        return self._name
    
    def get_namespace(self):
        return self._namespace
    
    def create_publisher(self, msg_type, topic: str, qos: int = 10):
        """创建发布者"""
        pub = MockPublisher(msg_type, topic, qos)
        self._publishers[topic] = pub
        return pub
    
    def create_subscription(self, msg_type, topic: str, callback: Callable, qos: int = 10):
        """创建订阅者"""
        sub = MockSubscription(msg_type, topic, callback, qos)
        self._subscriptions[topic] = sub
        return sub
    
    def create_timer(self, period_sec: float, callback: Callable):
        """创建定时器"""
        timer = MockTimer(period_sec, callback)
        self._timers.append(timer)
        timer.start()
        return timer
    
    def get_logger(self):
        return self._logger
    
    def destroy_node(self):
        """销毁节点"""
        for timer in self._timers:
            timer.stop()
        self._timers.clear()
        print(f"[Mock] Node destroyed: {self._name}")


# Mock Logger
class MockLogger:
    """模拟 ROS2 日志记录器"""
    
    def __init__(self, node_name: str):
        self._node_name = node_name
    
    def info(self, msg: str):
        print(f"[INFO] [{self._node_name}]: {msg}")
    
    def warn(self, msg: str):
        print(f"[WARN] [{self._node_name}]: {msg}")
    
    def error(self, msg: str):
        print(f"[ERROR] [{self._node_name}]: {msg}")
    
    def debug(self, msg: str):
        print(f"[DEBUG] [{self._node_name}]: {msg}")


# Mock Publisher
class MockPublisher:
    """模拟 ROS2 发布者"""
    
    def __init__(self, msg_type, topic: str, qos: int):
        self._msg_type = msg_type
        self._topic = topic
        self._qos = qos
    
    def publish(self, msg):
        """发布消息 (mock - 仅打印)"""
        print(f"[Mock] Publish to /{self._topic}: {msg.data}")


# Mock Subscription
class MockSubscription:
    """模拟 ROS2 订阅者"""
    
    def __init__(self, msg_type, topic: str, callback: Callable, qos: int):
        self._msg_type = msg_type
        self._topic = topic
        self._callback = callback
        self._qos = qos


# Mock Timer
class MockTimer:
    """模拟 ROS2 定时器"""
    
    def __init__(self, period_sec: float, callback: Callable):
        self._period = period_sec
        self._callback = callback
        self._running = False
        self._last_call = 0
    
    def start(self):
        self._running = True
        self._last_call = time.time()
    
    def stop(self):
        self._running = False
    
    def check(self):
        """检查是否应该触发回调"""
        if not self._running:
            return
        if time.time() - self._last_call >= self._period:
            self._callback()
            self._last_call = time.time()


# Mock std_msgs
class MockString:
    """模拟 std_msgs/msg/String"""
    
    def __init__(self):
        self.data = ""
    
    def __repr__(self):
        return f"String(data='{self.data}')"


# 创建模块结构
class MockStdMsgs:
    """模拟 std_msgs 模块"""
    
    class msg:
        """模拟 std_msgs.msg"""
        String = MockString


def install_mock():
    """
    安装 mock 模块到 sys.modules
    
    这会替换真实的 rclpy 和 std_msgs，使得未安装 ROS2 时
    测试脚本也能运行。
    """
    # 创建 mock 模块实例
    rclpy_mock = MockRclpy()
    
    # 将 mock 添加到 sys.modules
    sys.modules['rclpy'] = rclpy_mock
    sys.modules['rclpy.node'] = type(sys)('rclpy.node')
    sys.modules['rclpy.node'].Node = MockNode
    
    sys.modules['std_msgs'] = MockStdMsgs()
    sys.modules['std_msgs.msg'] = MockStdMsgs.msg
    
    print("=" * 50)
    print("[Mock Mode] ROS2 Mock 模块已安装")
    print("=" * 50)
    print("注意: 当前运行的是模拟模式，没有真实的 ROS2 功能")
    print("      如需完整功能，请安装 ROS2 Humble:")
    print("      https://docs.ros.org/en/humble/Installation.html")
    print("=" * 50)
    print()


# 如果直接运行此文件，显示帮助信息
if __name__ == '__main__':
    print("""
ROS2 Mock Module

使用方法:
    from samples.ros2_mock import install_mock
    install_mock()  # 在安装真实 ROS2 之前调用
    
    # 然后正常导入 ROS2 模块
    import rclpy
    from rclpy.node import Node
    from std_msgs.msg import String
""")
