"""Git operations with progress display."""

import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Optional

from rbc_build.utils import is_empty_folder, rel, print_success, print_error, print_info
from rbc_build.progress_utils import GitProgressParser, stream_reader_thread


def convert_to_ssh_url(https_url: str) -> str:
    """Convert HTTPS GitHub URL to SSH URL.
    
    Args:
        https_url: HTTPS URL like https://github.com/user/repo.git
        
    Returns:
        SSH URL like git@github.com:user/repo.git
    """
    import re
    pattern = r'https?://github\.com/([^/]+)/([^/]+?)(?:\.git)?$'
    match = re.match(pattern, https_url)
    if match:
        user, repo = match.groups()
        return f"git@github.com:{user}/{repo}.git"
    return https_url


def _run_with_spinner(process: subprocess.Popen, parser: GitProgressParser, description: str) -> int:
    """Run git process with a spinner animation.
    
    Args:
        process: The subprocess.Popen instance
        parser: GitProgressParser instance to parse progress
        description: Description to show with spinner (e.g., "Cloning repo")
        
    Returns:
        Process return code
    """
    spinner_chars = ['|', '/', '-', '\\']
    spinner_idx = 0
    stop_spinner = threading.Event()
    
    def read_stream(stream, is_stderr):
        """Read from stream and parse progress."""
        def handler(line):
            if is_stderr:
                parser.parse_line(line)
        stream_reader_thread(stream, handler)
    
    # Start threads to read stdout and stderr
    stdout_thread = threading.Thread(target=read_stream, args=(process.stdout, False))
    stderr_thread = threading.Thread(target=read_stream, args=(process.stderr, True))
    
    stdout_thread.start()
    stderr_thread.start()
    
    def spinner_thread():
        """Display spinner while process is running."""
        nonlocal spinner_idx
        while not stop_spinner.is_set():
            if not parser.has_progress:
                # Only show spinner if no git progress is being displayed
                char = spinner_chars[spinner_idx % len(spinner_chars)]
                print(f"\r{char} {description}...", end='', flush=True)
                spinner_idx += 1
            time.sleep(0.1)
    
    # Start spinner thread
    spinner = threading.Thread(target=spinner_thread)
    spinner.start()
    
    # Wait for process to complete
    returncode = process.wait()
    stdout_thread.join()
    stderr_thread.join()
    
    # Stop spinner
    stop_spinner.set()
    spinner.join()
    
    # Clear spinner line or move to new line
    if parser.has_progress:
        print()  # New line after progress bar
    else:
        print(f"\r  {description}... done")
    
    return returncode


def git_clone_or_pull(git_address: str, subdir: str, branch: Optional[str] = None, use_ssh: bool = False) -> None:
    """Clone or pull a git repository with progress display.
    
    Args:
        git_address: URL of the git repository
        subdir: Local subdirectory to clone/pull to
        branch: Branch to checkout (None for default branch)
        use_ssh: Whether to convert HTTPS URL to SSH URL
    """
    abs_subdir = rel(subdir)
    
    # Convert to SSH URL if requested
    if use_ssh:
        original_address = git_address
        git_address = convert_to_ssh_url(git_address)
        if git_address != original_address:
            print_info(f"Using SSH URL: {git_address}")
    
    args = []
    is_clone = is_empty_folder(abs_subdir)
    if is_clone:
        # Clone
        args = ["clone", "--progress", git_address]
        if branch:
            args.extend(["-b", branch])
        args.append(abs_subdir)
        action_desc = f"Cloning {subdir}"
    else:
        # Pull with progress
        args = ["-C", abs_subdir, "pull", "--progress"]
        if branch:
            args.extend(["origin", branch])
        action_desc = f"Pulling {subdir}"

    cmd = ['git'] + args
    
    try:
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding='utf-8',
            errors='replace',
            bufsize=1,
            universal_newlines=True
        )
    except FileNotFoundError:
        print_error("Git command not found. Please ensure git is installed.")
        sys.exit(1)
    except Exception as e:
        print_error(f"Failed to start git command: {e}")
        sys.exit(1)
    
    # Create progress parser and run with spinner
    parser = GitProgressParser()
    returncode = _run_with_spinner(process, parser, action_desc)
    
    if returncode == 0:
        if is_clone:
            print_success(f"[OK] Successfully cloned {subdir}")
        else:
            print_success(f"[OK] Successfully pulled {subdir}")
    else:
        if is_clone:
            print_error(f"[FAIL] Failed to clone {subdir}")
        else:
            print_error(f"[FAIL] Failed to pull {subdir}")
        sys.exit(1)
