"""Progress bar utilities for displaying download and git operation progress."""

import sys
import threading
import re
from typing import Optional, Callable


def print_progress_bar(percent: int, width: int = 30, prefix: str = "Progress") -> None:
    """Print a progress bar to the console.
    
    Args:
        percent: Percentage complete (0-100)
        width: Width of the progress bar in characters
        prefix: Text to display before the progress bar
    """
    filled = int(width * percent / 100)
    bar = '█' * filled + '░' * (width - filled)
    print(f"\r{prefix}: [{bar}] {percent:3d}%", end='', flush=True)


def clear_progress_line() -> None:
    """Clear the current progress line."""
    print()


class GitProgressParser:
    """Parser for git command progress output."""
    
    # Git progress patterns
    PATTERNS = {
        'receiving': re.compile(r'receiving objects:\s*(\d+)%'),
        'compressing': re.compile(r'compressing objects:\s*(\d+)%'),
        'resolving': re.compile(r'resolving deltas:\s*(\d+)%'),
        'counting': re.compile(r'counting objects:\s*(\d+)%'),
        'unpacking': re.compile(r'unpacking objects:\s*(\d+)%'),
    }
    
    # Human-readable prefixes for each phase
    PREFIXES = {
        'receiving': "Receiving objects",
        'compressing': "Compressing objects",
        'resolving': "Resolving deltas",
        'counting': "Counting objects",
        'unpacking': "Unpacking objects",
    }
    
    def __init__(self, progress_callback: Optional[Callable[[int, str], None]] = None):
        """Initialize the parser.
        
        Args:
            progress_callback: Callback function(percent, prefix) called when progress updates
        """
        self.progress_callback = progress_callback or self._default_callback
        self.has_progress = False
    
    @staticmethod
    def _default_callback(percent: int, prefix: str) -> None:
        """Default progress callback that prints to console."""
        print_progress_bar(percent, prefix=prefix)
    
    def parse_line(self, line: str) -> bool:
        """Parse a line of git output for progress information.
        
        Args:
            line: Line of output from git command
            
        Returns:
            True if progress was found and displayed, False otherwise
        """
        line = line.strip()
        if not line:
            return False
        
        for phase, pattern in self.PATTERNS.items():
            match = pattern.search(line)
            if match:
                percent = int(match.group(1))
                prefix = self.PREFIXES.get(phase, phase.capitalize())
                self.progress_callback(percent, prefix)
                self.has_progress = True
                return True
        
        return False


class DownloadProgressTracker:
    """Tracker for download progress with callback support."""
    
    def __init__(self, total_size: int, progress_callback: Optional[Callable[[int], None]] = None):
        """Initialize the tracker.
        
        Args:
            total_size: Total size of the download in bytes
            progress_callback: Callback function(percent) called when progress updates
        """
        self.total_size = total_size
        self.downloaded = 0
        self.progress_callback = progress_callback or self._default_callback
        self.last_percent = -1
    
    @staticmethod
    def _default_callback(percent: int) -> None:
        """Default progress callback that prints to console."""
        print_progress_bar(percent, prefix="Downloading", width=25)
    
    def update(self, chunk_size: int) -> None:
        """Update progress with newly downloaded chunk.
        
        Args:
            chunk_size: Size of the chunk just downloaded
        """
        self.downloaded += chunk_size
        if self.total_size > 0:
            percent = int(self.downloaded * 100 / self.total_size)
            # Only update display when percentage changes
            if percent != self.last_percent:
                self.last_percent = percent
                self.progress_callback(percent)
    
    def finish(self) -> None:
        """Mark download as complete."""
        if self.total_size > 0:
            self.progress_callback(100)
            print()


def stream_reader_thread(stream, line_handler: Callable[[str], None]) -> None:
    """Read lines from a stream and pass them to a handler.
    
    Args:
        stream: Stream to read from (e.g., subprocess stdout/stderr)
        line_handler: Function to call for each line read
    """
    try:
        for line in iter(stream.readline, ''):
            line_handler(line)
    finally:
        stream.close()
