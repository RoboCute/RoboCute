import os
import time
from pathlib import Path
import numpy as np
import math
import argparse

import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re


def load_height_image(path: str, size) -> np.ndarray:
    """Load a height texture (1-channel) as float32 numpy array."""
    from PIL import Image
    img = Image.open(path).convert('L')  # Convert to grayscale
    assert size == img.size, f"Size mismatch: expected {size}, got {img.size}"
    arr = np.array(img, dtype=np.float32) / 255.0  # Normalize to [0, 1]
    return arr


def load_normal_image(path: str, size) -> np.ndarray:
    """Load a normal texture (4-channel) as float32 numpy array."""
    from PIL import Image
    img = Image.open(path).convert('RGBA')  # Convert to RGBA
    assert size == img.size, f"Size mismatch: expected {size}, got {img.size}"
    arr = np.array(img, dtype=np.float32) / 255.0  # Normalize to [0, 1]
    return arr


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-b",
        "--backend",
        type=str,
        default="dx",
        help="graphics backend api type, dx/vk",
    )
    parser.add_argument(
        "--height",
        type=str,
        required=True,
        help="path to height texture (1-channel grayscale image)",
    )
    parser.add_argument(
        "--normal",
        type=str,
        required=True,
        help="path to normal texture (4-channel RGBA image)",
    )
    args = parser.parse_args()

    app = rbc.app.App()  # rbc app singleton
    app.init(args.backend, None, '.')
    if not app.ctx:
        print("Context not Valid!")
        return
    # shader
    height_to_normal = lc.Shader('texture_process/height_to_normal.bin')
    loss_shader = lc.Shader('texture_process/normal_loss.bin')
    # image
    size = (4096, 4096)
    # Load height texture as 1-channel float32 numpy array
    height_arr = load_height_image(args.height, size)
    # Load normal texture as 4-channel float32 numpy array
    dst_arr = load_normal_image(args.normal, size)
    height_img = lc.Image2D.empty(size[0], size[1], 1, float)
    src_image = lc.Image2D.empty(size[0], size[1], 4, float)
    dst_image = lc.Image2D.empty(size[0], size[1], 4, float)
    block_size = (size[0] // 32, size[1] // 32)
    result_buffer = lc.Buffer(block_size[0] * block_size[1], float)
    height_img.copy_from(height_arr)
    dst_image.copy_from(dst_arr)
    result_arr = np.empty(block_size[0] * block_size[1], dtype=np.float32)
    def compute_loss(scale: float):
        scale = float(scale)
        # Convert height to normal using shader
        assert height_img is not None, src_image is not None
        height_to_normal(
            height_img,
            src_image,
            scale,
            dispatch_size=(size[0], size[1], 1)
        )

        # Result array size matches the number of blocks used in loss calculation
        loss_shader(
            src_image,
            dst_image,
            result_buffer,
            lc.uint2(size[0], size[1]),
            dispatch_size=(block_size[0] * block_size[1],)
        )
        result_buffer.copy_to(result_arr)
        # Calculate result_arr average
        average = np.mean(result_arr)
        return average

    # Run gradient descent to find optimal scale
    print("Starting gradient descent optimization...")
    best_scale = 5.0
    for i in [40, 30, 20, 10., 5., 2., 1., 0.1]:
        best_scale = get_regression(
            compute_loss,
            max_iteration_count=100,
            initial_argument=best_scale,
            learning_rate=i,
            epsilon=1e-8
        )
    print(f"Optimization complete. Best scale: {best_scale:.8f}")


def get_regression(
    loss_func,
    max_iteration_count: int,
    initial_argument: float = 1.0,
    learning_rate: float = 0.01,
    epsilon: float = 1e-6
) -> float:
    """Use gradient descent to find the argument that minimizes the loss.

    Args:
        loss_func: A callable that takes a float argument and returns a loss value.
        max_iteration_count: Maximum number of iterations for the descent.
        initial_argument: Starting value for the argument.
        learning_rate: Step size for each iteration.
        # epsilon: Minimum change threshold to stop early.

    Returns:
        The argument value that results in the lowest loss.
    """
    assert callable(loss_func)

    current_arg = initial_argument
    best_arg = current_arg
    best_loss = loss_func(current_arg)

    for _ in range(max_iteration_count):
        # Compute numerical gradient
        delta = 1e-2
        loss_plus = loss_func(current_arg + delta)
        loss_minus = loss_func(current_arg - delta)
        gradient = (loss_plus - loss_minus) / (2 * delta)

        # Update argument using gradient descent
        new_arg = current_arg - learning_rate * gradient

        # Evaluate loss at new argument
        current_loss = loss_func(new_arg)

        # Track best result
        if current_loss < best_loss:
            best_loss = current_loss
            best_arg = new_arg

        # Check for convergence
        if abs(new_arg - current_arg) < epsilon:
            break

        current_arg = new_arg
        print(f"  Iteration: arg={current_arg:.8f}, loss={current_loss:.8f}")

    print(f"Best result: arg={best_arg:.8f}, loss={best_loss:.8f}")
    return best_arg


if __name__ == "__main__":
    main()
