# Auto-import custom nodes
try:
    from . import animation_nodes
    # from . import text2image_nodes
except ImportError as e:
    print(f"Warning: Could not import some custom nodes: {e}")
