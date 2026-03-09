from scripts.utils import is_empty_folder, get_project_root, rel, compute_hash, unzip_dir, print_success, print_error, print_warning, print_info, print_debug, run_git_command
from pathlib import Path


def make_alembic_config(project_root: Path):
    """
    Generate Alembic Config.h from Config.h.in template.

    Replaces CMake variables and cmakedefine directives to produce
    the final Config.h header file.

    Args:
        project_root: Path to the project root directory
    """
    config_h_in = project_root / "thirdparty/alembic/lib/Alembic/Util/Config.h.in"
    config_h = project_root / "thirdparty/alembic/lib/Alembic/Util/Config.h"
    if config_h.exists():
        return

    if not config_h_in.exists():
        print_error(f"Config.h.in not found: {config_h_in}")
        return False

    # Alembic version from CMakeLists.txt (VERSION 1.8.10)
    version_major = 1
    version_minor = 8
    version_patch = 10

    # Read template content
    content = config_h_in.read_text(encoding='utf-8')

    # Replace CMake variables
    content = content.replace('${PROJECT_VERSION_MAJOR}', str(version_major))
    content = content.replace('${PROJECT_VERSION_MINOR}', str(version_minor))
    content = content.replace('${PROJECT_VERSION_PATCH}', str(version_patch))

    # Replace #cmakedefine with #define (HDF5 support disabled by default)
    content = content.replace('#cmakedefine ALEMBIC_WITH_HDF5', '#undef ALEMBIC_WITH_HDF5')

    # Write output file
    config_h.write_text(content, encoding='utf-8')

    print_success(f"Generated {config_h}")
    return True

def make_imath_config(project_root: Path):
    config_h = project_root / "thirdparty/Imath/src/Imath/ImathConfig.h"
    if config_h.exists():
        return
    
    config_content = '''#pragma once
#define IMATH_NOEXCEPT noexcept
#define IMATH_DEPRECATED(x)
#define IMATH_HOSTDEVICE
#define IMATH_UNLIKELY
#define IMATH_LIKELY
#if defined _WIN32 || defined _WIN64
#define IMATH_DLL 1
#endif
'''
    config_h.write_text(config_content)
    
    print_success(f"Generated {config_h}")
    return True
    