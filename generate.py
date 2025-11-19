"""
RoboCute Code Generation Tool

This script orchestrates the code generation process for the RoboCute project.
It configures the Python environment and executes generation tasks in parallel
to ensure efficiency.

Usage:
    python generate.py
"""

import sys
import time
import importlib
from pathlib import Path
from multiprocessing import Process

# Configuration: List of modules and their entry point functions to run.
# These modules are expected to be located in the paths configured in setup_python_path().
GENERATION_TASKS = [
    ("meta.test_codegen", "test_codegen"),
    ("meta.test_serde", "test_serde"),
    ("meta.pipeline_settings", "pipeline_settings"),
]


def run_generation_task(module_name, function_name):
    """
    Imports a module and executes its specified entry point function.

    Args:
        module_name (str): The name of the module to import.
        function_name (str): The name of the function to call within the module.
    """
    try:
        # Dynamic import replaces the unsafe exec()
        module = importlib.import_module(module_name)

        if hasattr(module, function_name):
            func = getattr(module, function_name)
            print(f"[{module_name}] Starting generation...")
            func()
            print(f"[{module_name}] Completed successfully.")
        else:
            print(f"[{module_name}] Error: Function '{function_name}' not found.")
            sys.exit(1)

    except ImportError as e:
        print(f"[{module_name}] Error: Failed to import module. {e}")
        sys.exit(1)
    except Exception as e:
        print(f"[{module_name}] Error: An unexpected error occurred. {e}")
        sys.exit(1)


def main():
    """
    Main execution function. Sets up paths and spawns parallel processes for generation.
    """
    start_time = time.time()
    print(f"Starting code generation for {len(GENERATION_TASKS)} modules...")
    processes = []
    for module_name, function_name in GENERATION_TASKS:
        p = Process(target=run_generation_task, args=(module_name, function_name))
        p.start()
        processes.append(p)

    # Wait for all processes to complete
    exit_code = 0
    for p in processes:
        p.join()
        if p.exitcode != 0:
            exit_code = 1
            print(f"Process for task {p.name} failed with exit code {p.exitcode}")

    duration = time.time() - start_time
    print(f"Code generation finished in {duration:.2f} seconds.")
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
