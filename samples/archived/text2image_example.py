"""
Text2Image Example

Demonstrates how to use the Text2Image nodes to generate images from text prompts.
This example shows both the modular approach (using separate nodes) and the
simplified approach (using the all-in-one Text2Image node).
"""

import robocute as rbc
from robocute.graph import GraphDefinition, NodeDefinition, NodeConnection


def example_text2image_simple():
    """Example 1: Simple Text2Image using the all-in-one node"""
    print("\n" + "=" * 60)
    print("Example 1: Simple Text2Image")
    print("=" * 60)

    # Create graph with single Text2Image node
    graph_def = GraphDefinition(
        nodes=[
            NodeDefinition(
                node_id="text2image",
                node_type="text2image",
                inputs={
                    "prompt": "a beautiful sunset over mountains, highly detailed, 4k",
                    "width": 512,
                    "height": 512,
                    "steps": 20,
                    "cfg_scale": 7.0,
                    "seed": 42,
                },
            ),
        ],
        connections=[],
    )

    graph = rbc.NodeGraph.from_definition(graph_def)
    executor = rbc.GraphExecutor(graph)

    print("Executing graph...")
    result = executor.execute()

    print(f"Execution status: {result.status.value}")
    if result.status == rbc.ExecutionStatus.COMPLETED:
        outputs = result.get_node_outputs("text2image")
        if outputs:
            print(f"✓ Image generated successfully")
            print(f"  Seed: {outputs.get('seed', 'N/A')}")
            if "image_url" in outputs:
                print(f"  Image URL: {outputs['image_url'][:50]}...")
    else:
        print(f"✗ Execution failed: {result.error}")


def example_text2image_modular():
    """Example 2: Modular Text2Image using separate nodes (like ComfyUI)"""
    print("\n" + "=" * 60)
    print("Example 2: Modular Text2Image (ComfyUI-style)")
    print("=" * 60)

    # Create graph with separate nodes
    graph_def = GraphDefinition(
        nodes=[
            # CLIP Text Encode
            NodeDefinition(
                node_id="clip_encode",
                node_type="clip_text_encode",
                inputs={
                    "text": "a futuristic cityscape at night, neon lights, cyberpunk style",
                },
            ),
            # Empty Latent Image
            NodeDefinition(
                node_id="empty_latent",
                node_type="empty_latent_image",
                inputs={
                    "width": 768,
                    "height": 512,
                    "batch_size": 1,
                },
            ),
            # KSampler
            NodeDefinition(
                node_id="sampler",
                node_type="ksampler",
                inputs={
                    "steps": 25,
                    "cfg_scale": 8.0,
                    "seed": 123,
                },
            ),
            # VAE Decode
            NodeDefinition(
                node_id="vae_decode",
                node_type="vae_decode",
            ),
            # Save Image
            NodeDefinition(
                node_id="save",
                node_type="save_image",
                inputs={
                    "filename_prefix": "ComfyUI",
                },
            ),
        ],
        connections=[
            # Connect CLIP encode to sampler
            NodeConnection(
                from_node="clip_encode",
                from_output="conditioning",
                to_node="sampler",
                to_input="conditioning",
            ),
            # Connect empty latent to sampler
            NodeConnection(
                from_node="empty_latent",
                from_output="latent",
                to_node="sampler",
                to_input="latent",
            ),
            # Connect sampler to VAE decode
            NodeConnection(
                from_node="sampler",
                from_output="latent",
                to_node="vae_decode",
                to_input="latent",
            ),
            # Connect VAE decode to save
            NodeConnection(
                from_node="vae_decode",
                from_output="image",
                to_node="save",
                to_input="image",
            ),
        ],
    )

    graph = rbc.NodeGraph.from_definition(graph_def)
    executor = rbc.GraphExecutor(graph)

    print("Executing graph...")
    result = executor.execute()

    print(f"Execution status: {result.status.value}")
    if result.status == rbc.ExecutionStatus.COMPLETED:
        # Check outputs from save node
        save_outputs = result.get_node_outputs("save")
        if save_outputs:
            print(f"✓ Image saved successfully")
            print(f"  Path: {save_outputs.get('image_path', 'N/A')}")
    else:
        print(f"✗ Execution failed: {result.error}")


if __name__ == "__main__":
    print("=" * 60)
    print("Text2Image Node Examples")
    print("=" * 60)
    print("\nNote: This requires the 'diffusers' library to be installed:")
    print("  pip install diffusers transformers accelerate")
    print("\nAlso requires PyTorch:")
    print("  pip install torch torchvision")
    print()

    try:
        # Run simple example
        example_text2image_simple()
        
        # Run modular example
        # example_text2image_modular()
        
    except ImportError as e:
        print(f"\n✗ Missing dependencies: {e}")
        print("\nPlease install required packages:")
        print("  pip install diffusers transformers accelerate torch")
    except Exception as e:
        print(f"\n✗ Error: {e}")
        import traceback
        traceback.print_exc()

