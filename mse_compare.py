"""
MSE (Mean Squared Error) image comparison using numpy.
Supports 2D float arrays or 2D int arrays.
"""

import numpy as np


def mse(image1: np.ndarray, image2: np.ndarray) -> float:
    """
    Calculate Mean Squared Error (MSE) between two images.
    
    Parameters:
        image1: First image (2D float array or 2D int array)
        image2: Second image (2D float array or 2D int array)
    
    Returns:
        Mean Squared Error value (float)
    
    Raises:
        ValueError: If image shapes don't match
    """
    if image1.shape != image2.shape:
        raise ValueError(f"Image shapes don't match: {image1.shape} vs {image2.shape}")
    
    # Calculate MSE: mean of squared differences
    return np.mean((image1.astype(np.float64) - image2.astype(np.float64)) ** 2)


def main():
    """Example usage of MSE comparison."""
    # Example 1: Compare two 2D int arrays (e.g., grayscale images with values 0-255)
    print("=" * 50)
    print("Example 1: 2D Int Arrays (Grayscale Images)")
    print("=" * 50)
    
    img_int1 = np.array([
        [100, 150, 200],
        [50, 100, 150],
        [200, 250, 100]
    ], dtype=np.int32)
    
    img_int2 = np.array([
        [105, 155, 195],
        [55, 105, 145],
        [205, 245, 105]
    ], dtype=np.int32)
    
    print(f"Image 1:\n{img_int1}")
    print(f"\nImage 2:\n{img_int2}")
    print(f"\nMSE: {mse(img_int1, img_int2):.4f}")
    
    # Example 2: Compare two 2D float arrays (e.g., normalized images 0.0-1.0)
    print("\n" + "=" * 50)
    print("Example 2: 2D Float Arrays (Normalized Images)")
    print("=" * 50)
    
    img_float1 = np.array([
        [0.1, 0.5, 0.9],
        [0.2, 0.6, 0.8],
        [0.3, 0.7, 0.4]
    ], dtype=np.float32)
    
    img_float2 = np.array([
        [0.15, 0.55, 0.85],
        [0.25, 0.65, 0.75],
        [0.35, 0.75, 0.45]
    ], dtype=np.float32)
    
    print(f"Image 1:\n{img_float1}")
    print(f"\nImage 2:\n{img_float2}")
    print(f"\nMSE: {mse(img_float1, img_float2):.6f}")
    
    # Example 3: Identical images (MSE should be 0)
    print("\n" + "=" * 50)
    print("Example 3: Identical Images (MSE = 0)")
    print("=" * 50)
    
    img_copy = img_int1.copy()
    print(f"MSE of identical images: {mse(img_int1, img_copy):.4f}")
    
    # Example 4: Completely different images
    print("\n" + "=" * 50)
    print("Example 4: Completely Different Images")
    print("=" * 50)
    
    img_black = np.zeros((3, 3), dtype=np.uint8)
    img_white = np.ones((3, 3), dtype=np.uint8) * 255
    
    print(f"Black image:\n{img_black}")
    print(f"\nWhite image:\n{img_white}")
    print(f"\nMSE: {mse(img_black, img_white):.4f}")


if __name__ == "__main__":
    main()
