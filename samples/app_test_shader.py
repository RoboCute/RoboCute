import os
import time
from pathlib import Path
import numpy as np
import math
import argparse

import robocute as rbc
import robocute.rbc_ext.luisa as lc
import robocute.rbc_ext as re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-b",
        "--backend",
        type=str,
        default="dx",
        help="graphics backend api type, dx/vk",
    )
    args = parser.parse_args()

    app = rbc.app.App()  # rbc app singleton
    app.init(args.backend, None, '.')
    if not app.ctx:
        print("Context not Valid!")
        return
    src_image = lc.Image2D.empty(1024, 1024, 4, float)
    dst_image = lc.Image2D.empty(1024, 1024, 4, float)
    result_buffer = lc.Buffer(1024, float)
    loss_shader = lc.Shader('texture_process/normal_loss.bin')

    # Create source array with value [0, 1, 0, 0]
    src_arr = np.zeros((1024, 1024, 4), dtype=np.float32)
    src_arr[:, :, 0] = 0.0
    src_arr[:, :, 1] = 1.0
    src_arr[:, :, 2] = 0.0
    src_arr[:, :, 3] = 0.0

    # Create destination array with value [0.5, 0.5, 0.5, 0]
    dst_arr = np.zeros((1024, 1024, 4), dtype=np.float32)
    dst_arr[:, :, 0] = 0.0
    dst_arr[:, :, 1] = 0.9
    dst_arr[:, :, 2] = 0.2
    dst_arr[:, :, 3] = 0.0

    src_image.copy_from(src_arr)
    dst_image.copy_from(dst_arr)

    result_arr = np.empty(1024, dtype=np.float32)

    loss_shader(
        src_image,
        dst_image,
        result_buffer,
        lc.uint2(1024, 1024),
        dispatch_size=(1024 * 1024, )
    )
    result_buffer.copy_to(result_arr)
    # Calculate result_arr average
    average = np.mean(result_arr)
    print(f"Result array average: {average}")


def get_regression(
    loss_func,
    max_iteration_count: int,
    initial_argument: float = 0.5,
    learning_rate: float = 0.01,
    epsilon: float = 1e-6
) -> float:
    """Use gradient descent to find the argument that minimizes the loss.

    Args:
        loss_func: A callable that takes a float argument and returns a loss value.
        max_iteration_count: Maximum number of iterations for the descent.
        initial_argument: Starting value for the argument.
        learning_rate: Step size for each iteration.
        epsilon: Minimum change threshold to stop early.

    Returns:
        The argument value that results in the lowest loss.
    """
    assert callable(loss_func)

    current_arg = initial_argument
    best_arg = current_arg
    best_loss = loss_func(current_arg)

    for _ in range(max_iteration_count):
        # Compute numerical gradient
        delta = 1e-5
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

    return best_arg 

if __name__ == "__main__":
    main()
