from kimix.kimi_utils import *
# enable plan mode  and open a new session
set_plan_mode(True, False)
prompt('''
do this:
1. in plan mode, analyze python scripts under `src/robocute/rbc_ext/luisa/`, with the reference of API and usage.
2. use the analyzed result, verify and update `docs/api/lc_api.md`, make minimal changes.
''')
clear_context()
prompt('''
do this:
1. analyze `src/rbc_meta/types/world_interface.py`, summarize and save to plan file.
2. run `uv run gen` and read `src/robocute/rbc_ext/generated/world.py`, summarize and add to plan file.
3. use the analyzed result, verify and update `docs/api/world_api.md`, make minimal changes.
''')
