"""
SSIM (Structural Similarity Index Measure) implementation using pure NumPy.

Reference:
    Wang, Z., Bovik, A. C., Sheikh, H. R., & Simoncelli, E. P. (2004).
    Image quality assessment: from error visibility to structural similarity.
    IEEE transactions on image processing, 13(4), 600-612.
"""

import numpy as np
from typing import Union


def _gaussian_window(window_size: int, sigma: float) -> np.ndarray:
    """
    Create a 2D Gaussian kernel.

    Args:
        window_size: Size of the window (must be odd).
        sigma: Standard deviation of the Gaussian.

    Returns:
        2D Gaussian kernel normalized to sum to 1.
    """
    coords = np.arange(window_size, dtype=np.float64) - (window_size - 1) / 2
    g = np.exp(-(coords ** 2) / (2 * sigma ** 2))
    g = g / np.sum(g)
    
    # Create 2D Gaussian by outer product
    window = np.outer(g, g)
    return window


def _uniform_filter(img: np.ndarray, size: int) -> np.ndarray:
    """
    Apply uniform (box) filter using cumulative sum for efficiency.
    This is a pure NumPy implementation.

    Args:
        img: Input image (2D array).
        size: Size of the filter window (must be odd).

    Returns:
        Filtered image.
    """
    pad = size // 2
    
    # Pad the image using edge values
    padded = np.pad(img, pad, mode='edge')
    
    # Compute cumulative sum along rows
    cumsum = np.cumsum(padded, axis=0)
    # Compute sliding sum along rows
    rowsum = cumsum[size:, :] - cumsum[:-size, :]
    
    # Compute cumulative sum along columns
    cumsum = np.cumsum(rowsum, axis=1)
    # Compute sliding sum along columns
    result = cumsum[:, size:] - cumsum[:, :-size]
    
    return result / (size * size)


def _convolve2d(img: np.ndarray, kernel: np.ndarray) -> np.ndarray:
    """
    2D convolution using FFT.

    Args:
        img: Input image (2D array).
        kernel: Convolution kernel (2D array).

    Returns:
        Convolved image.
    """
    from numpy.fft import fft2, ifft2
    
    # Get dimensions
    h, w = img.shape
    kh, kw = kernel.shape
    
    # Pad kernel to image size
    kernel_padded = np.zeros((h, w), dtype=np.float64)
    kernel_padded[:kh, :kw] = kernel
    
    # Center the kernel
    kernel_padded = np.fft.ifftshift(kernel_padded)
    
    # FFT convolution
    img_fft = fft2(img)
    kernel_fft = fft2(kernel_padded)
    result = ifft2(img_fft * kernel_fft).real
    
    return result


def ssim(
    img1: np.ndarray,
    img2: np.ndarray,
    window_size: int = 11,
    k1: float = 0.01,
    k2: float = 0.03,
) -> float:
    """
    Calculate SSIM (Structural Similarity Index Measure) between two images.
    Pure NumPy implementation.

    Args:
        img1: First image, 2D array (float or int).
        img2: Second image, 2D array (float or int).
        window_size: Size of the sliding window (default: 11, should be odd).
        k1: First stability constant (default: 0.01).
        k2: Second stability constant (default: 0.03).

    Returns:
        SSIM value in range [-1, 1], where 1 indicates perfect similarity.

    Raises:
        ValueError: If inputs are not valid.
    """
    # Validate inputs
    if img1.shape != img2.shape:
        raise ValueError(f"Shape mismatch: {img1.shape} vs {img2.shape}")
    
    if img1.ndim != 2:
        raise ValueError(f"Expected 2D arrays, got {img1.ndim}D")
    
    if window_size % 2 == 0:
        raise ValueError(f"Window size must be odd, got {window_size}")
    
    # Convert to float64 for numerical stability
    img1 = img1.astype(np.float64)
    img2 = img2.astype(np.float64)
    
    # Dynamic range (for normalization)
    data_range = max(img1.max(), img2.max()) - min(img1.min(), img2.min())
    if data_range == 0:
        data_range = 255.0  # Assume 8-bit image if no variation
    
    # Stability constants
    c1 = (k1 * data_range) ** 2
    c2 = (k2 * data_range) ** 2
    
    # Compute local means using uniform filter
    mu1 = _uniform_filter(img1, window_size)
    mu2 = _uniform_filter(img2, window_size)
    
    # Compute squares and cross term
    mu1_sq = mu1 ** 2
    mu2_sq = mu2 ** 2
    mu1_mu2 = mu1 * mu2
    
    # Compute variances and covariance using E[X^2] - E[X]^2
    sigma1_sq = _uniform_filter(img1 ** 2, window_size) - mu1_sq
    sigma2_sq = _uniform_filter(img2 ** 2, window_size) - mu2_sq
    sigma12 = _uniform_filter(img1 * img2, window_size) - mu1_mu2
    
    # SSIM formula
    numerator1 = 2 * mu1_mu2 + c1
    numerator2 = 2 * sigma12 + c2
    denominator1 = mu1_sq + mu2_sq + c1
    denominator2 = sigma1_sq + sigma2_sq + c2
    
    ssim_map = (numerator1 * numerator2) / (denominator1 * denominator2)
    
    # Return mean SSIM
    return float(ssim_map.mean())


def ssim_map(
    img1: np.ndarray,
    img2: np.ndarray,
    window_size: int = 11,
    k1: float = 0.01,
    k2: float = 0.03,
) -> np.ndarray:
    """
    Calculate SSIM map (pixel-wise SSIM values) between two images.
    Pure NumPy implementation.

    Args:
        img1: First image, 2D array (float or int).
        img2: Second image, 2D array (float or int).
        window_size: Size of the sliding window (default: 11, should be odd).
        k1: First stability constant (default: 0.01).
        k2: Second stability constant (default: 0.03).

    Returns:
        SSIM map (2D array) with values in range [-1, 1].
    """
    # Validate inputs
    if img1.shape != img2.shape:
        raise ValueError(f"Shape mismatch: {img1.shape} vs {img2.shape}")
    
    if img1.ndim != 2:
        raise ValueError(f"Expected 2D arrays, got {img1.ndim}D")
    
    if window_size % 2 == 0:
        raise ValueError(f"Window size must be odd, got {window_size}")
    
    # Convert to float64 for numerical stability
    img1 = img1.astype(np.float64)
    img2 = img2.astype(np.float64)
    
    # Dynamic range
    data_range = max(img1.max(), img2.max()) - min(img1.min(), img2.min())
    if data_range == 0:
        data_range = 255.0
    
    # Stability constants
    c1 = (k1 * data_range) ** 2
    c2 = (k2 * data_range) ** 2
    
    # Compute local means
    mu1 = _uniform_filter(img1, window_size)
    mu2 = _uniform_filter(img2, window_size)
    
    # Compute squares and cross term
    mu1_sq = mu1 ** 2
    mu2_sq = mu2 ** 2
    mu1_mu2 = mu1 * mu2
    
    # Compute variances and covariance
    sigma1_sq = _uniform_filter(img1 ** 2, window_size) - mu1_sq
    sigma2_sq = _uniform_filter(img2 ** 2, window_size) - mu2_sq
    sigma12 = _uniform_filter(img1 * img2, window_size) - mu1_mu2
    
    # SSIM formula
    numerator1 = 2 * mu1_mu2 + c1
    numerator2 = 2 * sigma12 + c2
    denominator1 = mu1_sq + mu2_sq + c1
    denominator2 = sigma1_sq + sigma2_sq + c2
    
    ssim_map_result = (numerator1 * numerator2) / (denominator1 * denominator2)
    
    return ssim_map_result


def main():
    """Example usage of SSIM."""
    # Create example images
    np.random.seed(42)
    
    # Original image (2D float array)
    img1 = np.random.rand(256, 256) * 255
    
    # Modified image (add noise)
    noise = np.random.randn(256, 256) * 10
    img2 = img1 + noise
    img2 = np.clip(img2, 0, 255)
    
    # Also test with integer arrays
    img1_int = img1.astype(np.uint8)
    img2_int = img2.astype(np.uint8)
    
    print("=" * 50)
    print("SSIM Example (Pure NumPy Implementation)")
    print("=" * 50)
    
    # Test with float arrays
    ssim_value_float = ssim(img1, img2)
    print(f"\nSSIM between float arrays: {ssim_value_float:.6f}")
    
    # Test with int arrays
    ssim_value_int = ssim(img1_int, img2_int)
    print(f"SSIM between int arrays:   {ssim_value_int:.6f}")
    
    # Test identical images
    ssim_identical = ssim(img1, img1)
    print(f"SSIM of identical images:  {ssim_identical:.6f}")
    
    # Test completely different images
    img3 = np.random.rand(256, 256) * 255
    ssim_different = ssim(img1, img3)
    print(f"SSIM of random images:     {ssim_different:.6f}")
    
    # Get SSIM map
    ssim_m = ssim_map(img1, img2)
    print(f"\nSSIM map shape: {ssim_m.shape}")
    print(f"SSIM map range: [{ssim_m.min():.4f}, {ssim_m.max():.4f}]")


if __name__ == "__main__":
    main()
