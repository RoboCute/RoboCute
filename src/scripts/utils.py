import os
from pathlib import Path
import shutil
import sys
import subprocess
from enum import Enum
from typing import Optional


class Color(Enum):
    """ANSI color codes for foreground colors."""

    BLACK = 30
    RED = 31
    GREEN = 32
    YELLOW = 33
    BLUE = 34
    MAGENTA = 35
    CYAN = 36
    WHITE = 37
    BRIGHT_BLACK = 90
    BRIGHT_RED = 91
    BRIGHT_GREEN = 92
    BRIGHT_YELLOW = 93
    BRIGHT_BLUE = 94
    BRIGHT_MAGENTA = 95
    BRIGHT_CYAN = 96
    BRIGHT_WHITE = 97


class BgColor(Enum):
    """ANSI color codes for background colors."""

    BLACK = 40
    RED = 41
    GREEN = 42
    YELLOW = 43
    BLUE = 44
    MAGENTA = 45
    CYAN = 46
    WHITE = 47
    BRIGHT_BLACK = 100
    BRIGHT_RED = 101
    BRIGHT_GREEN = 102
    BRIGHT_YELLOW = 103
    BRIGHT_BLUE = 104
    BRIGHT_MAGENTA = 105
    BRIGHT_CYAN = 106
    BRIGHT_WHITE = 107


class Style(Enum):
    """ANSI style codes."""

    RESET = 0
    BOLD = 1
    DIM = 2
    ITALIC = 3
    UNDERLINE = 4
    BLINK = 5
    REVERSE = 7
    HIDDEN = 8
    STRIKETHROUGH = 9


def colorful_print(
    text: str,
    fg: Optional[Color] = None,
    bg: Optional[BgColor] = None,
    styles: Optional[list[Style]] = None,
    end: str = "\n",
) -> None:
    """
    Print text with optional colors and styles.

    Args:
        text: The text to print
        fg: Foreground color
        bg: Background color
        styles: List of text styles to apply
        end: String to append at the end (default: newline)
    """
    codes = []

    if styles:
        codes.extend(style.value for style in styles)
    if fg:
        codes.append(fg.value)
    if bg:
        codes.append(bg.value)

    if codes:
        text = f"\033[{';'.join(map(str, codes))}m{text}\033[0m"

    print(text, end=end)


def print_success(text: str, end: str = "\n") -> None:
    """Print success message in green."""
    colorful_print(text, fg=Color.GREEN, styles=[Style.BOLD], end=end)


def print_debug(text: str, end: str = "\n") -> None:
    """Print success message in green."""
    colorful_print(text, fg=Color.BLUE, styles=[Style.BOLD], end=end)


def print_error(text: str, end: str = "\n") -> None:
    """Print error message in red."""
    colorful_print(text, fg=Color.RED, styles=[Style.BOLD], end=end)


def print_warning(text: str, end: str = "\n") -> None:
    """Print warning message in yellow."""
    colorful_print(text, fg=Color.YELLOW, styles=[Style.BOLD], end=end)


def print_info(text: str, end: str = "\n") -> None:
    """Print info message in blue."""
    print(text, end=end)


# Global project root (initialized on import)
_PROJECT_ROOT = None


def rel(path):
    """Get a path relative to project root."""
    global _PROJECT_ROOT
    if _PROJECT_ROOT is None:
        _PROJECT_ROOT = get_project_root()
    return Path(_PROJECT_ROOT) / path


def is_empty_folder(dir_path):
    if os.path.exists(dir_path) and not os.path.isfile(dir_path):
        # Check if directory is empty (contains no files/subdirectories)
        return len(os.listdir(dir_path)) == 0
    else:
        # Treat non-existent directory as empty (ready for cloning)
        return True


def get_project_root() -> Path:
    """Get the project root directory."""
    # Assumes this script is located in <root>/src/scripts/
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = Path(script_dir).parent.parent
    return project_root


def compute_hash(path: Path) -> str:
    """Compute SHA256 hash of a file or directory.

    If path is a file, returns the SHA256 hash of the file content.
    If path is a directory, computes SHA256 for each file recursively,
    sorts them, and returns the SHA256 of the concatenated hashes.
    """
    import hashlib

    if not path.exists():
        raise FileNotFoundError(f"Path does not exist: {path}")

    if path.is_file():
        # Compute SHA256 for a single file
        sha256 = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(8192), b""):
                sha256.update(chunk)
        return sha256.hexdigest()

    elif path.is_dir():
        # Compute SHA256 for each file in the directory recursively
        file_hashes = []
        for file_path in sorted(path.rglob("*")):
            if file_path.is_file():
                file_hash = compute_hash(file_path)
                # Include relative path to make hash sensitive to file structure
                relative_path = file_path.relative_to(path).as_posix()
                file_hashes.append(f"{relative_path}:{file_hash}")

        # Sort and compute final hash from concatenated file hashes
        file_hashes.sort()
        final_hash = hashlib.sha256()
        for h in file_hashes:
            final_hash.update(h.encode("utf-8"))
        return final_hash.hexdigest()

    else:
        raise ValueError(f"Path is neither a file nor a directory: {path}")


path_7z = None


def find_7z_executable():
    global path_7z
    if path_7z:
        return path_7z
    """Find 7z executable for extracting archives."""
    # Search for 7z in xmake installation directory
    xmake_path = shutil.which("xmake")
    xmake_7z_path = None
    if xmake_path:
        xmake_dir = Path(xmake_path).parent
        xmake_7z = xmake_dir / "winenv" / "bin" / "7z.exe"
        if xmake_7z.exists():
            xmake_7z_path = str(xmake_7z)

    possible_paths = [
        xmake_7z_path,
        shutil.which("7z"),
        shutil.which("7za"),
        "C:/Program Files/7-Zip/7z.exe",
        "C:/Program Files (x86)/7-Zip/7z.exe",
    ]

    for path in possible_paths:
        if path and os.path.exists(path):
            path_7z = path
            return path_7z
    return path_7z


def unzip_dir(zip_path: Path, extract_to: Path):
    """Extract archive to specified directory. Supports 7z, rar, and zip formats.

    Args:
        zip_dir: Path to the archive file
        unziped_dir: Directory to extract files to
    """
    from pathlib import Path
    import zipfile

    if not zip_path.exists():
        print_error(f"ERROR: Archive file not found: {zip_path}")
        sys.exit(1)

    # Ensure extract directory exists
    extract_to.mkdir(parents=True, exist_ok=True)

    # Get file extension
    suffix = zip_path.suffix.lower()

    if suffix == ".zip":
        # Use standard library zipfile for zip archives
        print_info(f"Extracting {zip_path.name} to {extract_to}...")
        try:
            with zipfile.ZipFile(zip_path, "r") as zf:
                zf.extractall(extract_to)
            print_success(f"✓ Successfully extracted {zip_path.name}")
        except zipfile.BadZipFile:
            print_error(f"ERROR: Invalid or corrupted zip file: {zip_path.name}")
            sys.exit(1)
        except Exception as e:
            print_error(f"ERROR: Failed to extract {zip_path.name}: {e}")
            sys.exit(1)

    elif suffix in [".7z", ".rar"]:
        # Use 7z executable for 7z and rar archives
        seven_zip = find_7z_executable()
        if not seven_zip:
            print_error(
                f"ERROR: 7z executable not found. Please install 7-Zip to extract {suffix} files."
            )
            print("  Download from: https://www.7-zip.org/")
            sys.exit(1)

        print_info(f"Extracting {zip_path.name} to {extract_to}...")
        try:
            subprocess.check_call(
                [seven_zip, "x", str(zip_path), f"-o{extract_to}", "-y"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
            )
            print_success(f"✓ Successfully extracted {zip_path.name}")
        except subprocess.CalledProcessError:
            print_error(f"ERROR: Failed to extract {zip_path.name}")
            print(f"  Command: {seven_zip} x {zip_path} -o{extract_to} -y")
            sys.exit(1)

    else:
        print_error(f"ERROR: Unsupported archive format: {suffix}")
        print("  Supported formats: .zip, .7z, .rar")
        sys.exit(1)


def _get_git_error_explanation(returncode: int, stderr: str) -> str:
    """Get human-readable explanation for common git error codes.

    Args:
        returncode: The exit code from git command
        stderr: Standard error output from git command

    Returns:
        Human-readable explanation of the error
    """
    explanations = {
        2: "Misuse of shell builtins or incorrect arguments.",
        126: "Command invoked cannot execute - permission denied or not an executable.",
        127: "Command not found - git executable not found in PATH.",
        128: "Fatal error - usually indicates repository not found, invalid ref, or "
        "network connectivity issues.",
        129: "Usage error - invalid command-line arguments or bad flag.",
        130: "Command terminated by Ctrl+C (SIGINT).",
        137: "Command killed (SIGKILL) - possibly out of memory.",
        143: "Command terminated (SIGTERM).",
    }

    base_explanation = explanations.get(
        returncode,
        f"Unknown error code {returncode}. See https://git-scm.com/docs for more info.",
    )

    # Add specific hints based on stderr content
    hints = []
    stderr_lower = stderr.lower()

    if "could not resolve host" in stderr_lower:
        hints.append(
            "Network issue: Unable to resolve hostname. "
            "Check your internet connection and DNS settings."
        )
    elif "authentication failed" in stderr_lower or "403" in stderr:
        hints.append(
            "Authentication failed: Check your credentials, token, or repository permissions. "
            "For HTTPS, ensure your password/token is correct. "
            "For SSH, ensure your SSH key is properly configured."
        )
    elif "could not read from remote repository" in stderr_lower:
        hints.append(
            "Cannot access remote repository. Possible causes:\n"
            "  - Repository doesn't exist or is private\n"
            "  - Network connectivity issues\n"
            "  - SSH key not configured (for SSH URLs)\n"
            "  - Invalid credentials (for HTTPS URLs)"
        )
    elif "merge conflict" in stderr_lower or "conflict" in stderr_lower:
        hints.append(
            "Merge conflicts detected. Resolve conflicts manually and commit the result."
        )
    elif "already exists" in stderr_lower:
        hints.append(
            "The target already exists. Use 'git clone' into an empty directory "
            "or remove the existing directory first."
        )
    elif "not a git repository" in stderr_lower:
        hints.append(
            "Not a git repository. Run 'git init' to create one, "
            "or ensure you're in the correct directory."
        )
    elif "failed to connect" in stderr_lower:
        hints.append(
            "Failed to connect to remote server. Check your internet connection, "
            "proxy settings, or firewall configuration."
        )
    elif "ssl certificate problem" in stderr_lower:
        hints.append(
            "SSL certificate verification failed. Possible solutions:\n"
            "  - Update CA certificates\n"
            "  - Check system time is correct\n"
            "  - Configure git to use correct SSL backend"
        )

    result = base_explanation
    if hints:
        result += "\nHints:\n" + "\n".join(f"  • {hint}" for hint in hints)

    return result


def run_git_command(args: list[str], cwd: Optional[Path] = None) -> tuple[bool, str]:
    """Run a git command and return success status and output log.

    Args:
        args: List of git command arguments (e.g., ['clone', 'url'])
        cwd: Optional working directory to run the command in

    Returns:
        A tuple of (success: bool, log: str). If the command fails,
        success is False and log contains stderr output.
    """
    cmd = ["git"] + args
    error_log = None
    for i in range(3):
        try:
            result = subprocess.run(
                cmd, cwd=cwd, capture_output=True, text=True, encoding="utf-8"
            )
            if result.returncode == 0:
                return True, result.stdout
            else:
                explanation = _get_git_error_explanation(
                    result.returncode, result.stderr
                )
                error_log = f"Command failed with exit code {result.returncode}\n"
                error_log += f"Explanation: {explanation}\n"
                if result.stderr:
                    error_log += f"stderr: {result.stderr}\n"
                if result.stdout:
                    error_log += f"stdout: {result.stdout}\n"
                error_log += (
                    "\nFor more information about git error codes, visit:\n"
                    "  - https://git-scm.com/docs\n"
                    "  - https://git-scm.com/book/en/v2/Git-Internals-Environment-Variables"
                )
                continue
        except FileNotFoundError:
            return False, (
                "Git command not found. Please ensure git is installed.\n"
                "Download from: https://git-scm.com/downloads"
            )
        except Exception as e:
            error_log = f"Unexpected error running git command: {e}"
            continue
    return False, error_log
