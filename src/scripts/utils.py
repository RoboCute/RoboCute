import os
from pathlib import Path


def is_empty_folder(dir_path):
    if os.path.exists(dir_path) and not os.path.isfile(dir_path):
        # Check if directory is empty
        if not os.listdir(dir_path):
            return True
        return False
    else:
        # Treat non-existent directory as empty (ready for cloning)
        return True


def get_project_root() -> Path:
    """Get the project root directory."""
    # Assumes this script is located in <root>/src/scripts/
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = Path(script_dir).parent.parent
    return project_root


# Global project root (initialized on import)
_PROJECT_ROOT = None


def rel(path):
    """Get a path relative to project root."""
    global _PROJECT_ROOT
    if _PROJECT_ROOT is None:
        _PROJECT_ROOT = get_project_root()
    return Path(_PROJECT_ROOT) / path
