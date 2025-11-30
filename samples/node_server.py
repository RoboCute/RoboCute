"""
主程序入口

启动 FastAPI 服务器并初始化节点系统。
"""

import argparse
import uvicorn
from robocute.api import app
from robocute.node_registry import get_registry

# 导入示例节点以触发注册
from robocute import example_nodes
import animation_nodes  # Import animation nodes for registration


def init_nodes():
    """初始化节点系统"""
    print("=" * 60)
    print("RBCNode System - ComfyUI Style Node Backend")
    print("=" * 60)

    registry = get_registry()
    print(f"\nRegistered {len(registry)} node types:")

    # 按类别组织节点
    nodes_by_category = {}
    for node_type in registry.get_all_node_types():
        metadata = registry.get_metadata(node_type)
        if metadata:
            category = metadata.category
            if category not in nodes_by_category:
                nodes_by_category[category] = []
            nodes_by_category[category].append(metadata)

    # 打印节点列表
    for category, nodes in sorted(nodes_by_category.items()):
        print(f"\n  [{category}]")
        for node in nodes:
            print(f"    - {node.display_name} ({node.node_type})")

    print("\n" + "=" * 60)


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description="RBCNode System - ComfyUI Style Node Backend"
    )
    parser.add_argument(
        "--host", type=str, default="127.0.0.1", help="Server host (default: 127.0.0.1)"
    )
    parser.add_argument(
        "--port", type=int, default=8000, help="Server port (default: 8000)"
    )
    parser.add_argument(
        "--reload", action="store_true", help="Enable auto-reload for development"
    )

    args = parser.parse_args()

    # 初始化节点
    init_nodes()

    # 启动服务器
    print(f"\nStarting server at http://{args.host}:{args.port}")
    print(f"API documentation: http://{args.host}:{args.port}/docs")
    print(f"Alternative docs: http://{args.host}:{args.port}/redoc")
    print("\nPress Ctrl+C to stop the server\n")

    uvicorn.run(
        app, host=args.host, port=args.port, reload=args.reload, log_level="info"
    )


if __name__ == "__main__":
    main()
