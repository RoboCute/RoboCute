@echo off
uv sync
uv run scripts/setup.py
uv run generate.py