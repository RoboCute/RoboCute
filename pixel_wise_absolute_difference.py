"""
Pixel-wise Absolute Difference implementation using NumPy.

This module provides functionality to compare two images (2D arrays) by computing
the absolute difference between corresponding pixels.
"""

import numpy as np
from typing import Union, Tuple


def pixel_wise_absolute_diff(
    image1: np.ndarray,
    image2: np.ndarray
) -> np.ndarray:
    """
    Compute pixel-wise absolute difference between two images.

    Args:
        image1: First image, can be 2D float array or 2D int array.
        image2: Second image, must have the same shape as image1.

    Returns:
        A 2D array of the same shape as inputs, containing absolute differences.
        If inputs are float arrays, output is float64.
        If inputs are int arrays, output is uint64 (to avoid overflow).

    Raises:
        ValueError: If image shapes don't match or arrays are not 2D.
        TypeError: If inputs are not numpy arrays.
    """
    # Input validation
    if not isinstance(image1, np.ndarray) or not isinstance(image2, np.ndarray):
        raise TypeError("Both inputs must be numpy arrays")
    
    if image1.ndim != 2 or image2.ndim != 2:
        raise ValueError("Both images must be 2D arrays")
    
    if image1.shape != image2.shape:
        raise ValueError(
            f"Image shapes must match: {image1.shape} vs {image2.shape}"
        )
    
    # Compute absolute difference
    # Use appropriate dtype to avoid overflow for integer arrays
    if np.issubdtype(image1.dtype, np.integer) and np.issubdtype(image2.dtype, np.integer):
        # Cast to larger integer type to avoid overflow during subtraction
        diff = np.abs(image1.astype(np.int64) - image2.astype(np.int64))
    else:
        # For float arrays, use float64
        diff = np.abs(image1.astype(np.float64) - image2.astype(np.float64))
    
    return diff


def get_difference_statistics(diff: np.ndarray) -> dict:
    """
    Get statistics about the pixel-wise difference.

    Args:
        diff: The difference array from pixel_wise_absolute_diff.

    Returns:
        A dictionary containing:
        - max_diff: Maximum absolute difference
        - min_diff: Minimum absolute difference  
        - mean_diff: Mean absolute difference
        - std_diff: Standard deviation of differences
        - total_diff: Sum of all absolute differences
    """
    return {
        'max_diff': np.max(diff),
        'min_diff': np.min(diff),
        'mean_diff': np.mean(diff),
        'std_diff': np.std(diff),
        'total_diff': np.sum(diff)
    }


def find_significant_differences(
    diff: np.ndarray,
    threshold: Union[int, float] = 10
) -> np.ndarray:
    """
    Find pixels where the absolute difference exceeds a threshold.

    Args:
        diff: The difference array from pixel_wise_absolute_diff.
        threshold: Threshold value for significant difference.

    Returns:
        A boolean mask array where True indicates significant difference.
    """
    return diff > threshold


def visualize_difference(
    image1: np.ndarray,
    image2: np.ndarray,
    diff: np.ndarray = None,
    cmap: str = 'hot'
) -> None:
    """
    Visualize the pixel-wise absolute difference (requires matplotlib).

    Args:
        image1: First image.
        image2: Second image.
        diff: Pre-computed difference array. If None, it will be computed.
        cmap: Colormap for displaying the difference.
    """
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is required for visualization. Install with: pip install matplotlib")
        return
    
    if diff is None:
        diff = pixel_wise_absolute_diff(image1, image2)
    
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    
    # Display original images and difference
    im1 = axes[0].imshow(image1, cmap='gray')
    axes[0].set_title('Image 1')
    axes[0].axis('off')
    plt.colorbar(im1, ax=axes[0], fraction=0.046, pad=0.04)
    
    im2 = axes[1].imshow(image2, cmap='gray')
    axes[1].set_title('Image 2')
    axes[1].axis('off')
    plt.colorbar(im2, ax=axes[1], fraction=0.046, pad=0.04)
    
    im3 = axes[2].imshow(diff, cmap=cmap)
    axes[2].set_title(f'Absolute Difference\n(mean={np.mean(diff):.2f}, max={np.max(diff):.2f})')
    axes[2].axis('off')
    plt.colorbar(im3, ax=axes[2], fraction=0.046, pad=0.04)
    
    plt.tight_layout()
    plt.savefig('pixel_wise_difference.png', dpi=150, bbox_inches='tight')
    plt.show()


# ============== Example Usage ==============

if __name__ == "__main__":
    print("=" * 60)
    print("Pixel-wise Absolute Difference Example")
    print("=" * 60)
    
    # Create sample 2D images
    np.random.seed(42)
    
    # Example 1: Integer arrays (e.g., grayscale images with values 0-255)
    print("\n--- Example 1: Integer Arrays (0-255) ---")
    img_int1 = np.random.randint(0, 256, size=(100, 100), dtype=np.uint8)
    # Create a modified version with some changes
    img_int2 = img_int1.copy()
    # Add some modifications
    img_int2[30:50, 40:60] = np.clip(img_int2[30:50, 40:60] + 50, 0, 255)
    img_int2[70:80, 20:30] = np.clip(img_int2[70:80, 20:30] - 30, 0, 255)
    
    diff_int = pixel_wise_absolute_diff(img_int1, img_int2)
    stats_int = get_difference_statistics(diff_int)
    
    print(f"Image shape: {img_int1.shape}")
    print(f"Image dtype: {img_int1.dtype}")
    print(f"Difference dtype: {diff_int.dtype}")
    print(f"\nDifference Statistics:")
    for key, value in stats_int.items():
        print(f"  {key}: {value:.2f}")
    
    # Find significant differences
    sig_mask = find_significant_differences(diff_int, threshold=20)
    print(f"\nPixels with difference > 20: {np.sum(sig_mask)} / {sig_mask.size}")
    
    # Example 2: Float arrays (e.g., normalized images 0.0-1.0)
    print("\n--- Example 2: Float Arrays (0.0-1.0) ---")
    img_float1 = np.random.rand(64, 64).astype(np.float32)
    img_float2 = img_float1 + np.random.randn(64, 64).astype(np.float32) * 0.1
    img_float2 = np.clip(img_float2, 0.0, 1.0)
    
    diff_float = pixel_wise_absolute_diff(img_float1, img_float2)
    stats_float = get_difference_statistics(diff_float)
    
    print(f"Image shape: {img_float1.shape}")
    print(f"Image dtype: {img_float1.dtype}")
    print(f"Difference dtype: {diff_float.dtype}")
    print(f"\nDifference Statistics:")
    for key, value in stats_float.items():
        print(f"  {key}: {value:.4f}")
    
    # Example 3: Edge cases
    print("\n--- Example 3: Edge Cases ---")
    
    # Same images - should have zero difference
    same_diff = pixel_wise_absolute_diff(img_int1, img_int1)
    print(f"Same image difference: max={np.max(same_diff)}, sum={np.sum(same_diff)}")
    
    # Empty difference at specific location
    specific_region = diff_int[30:35, 40:45]
    print(f"Specific region (modified area) mean diff: {np.mean(specific_region):.2f}")
    
    print("\n" + "=" * 60)
    print("Examples completed successfully!")
    print("=" * 60)
    
    # Uncomment to visualize (requires matplotlib):
    # visualize_difference(img_int1, img_int2, diff_int)
