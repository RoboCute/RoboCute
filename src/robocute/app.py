from typing import Optional
import robocute.rbc_ext as rbc_ext
from pathlib import Path

import os

BUILTIN_PROGRAM_PATH = Path(
    os.path.dirname(__file__) + "/rbc_ext/_C"
)  # Built-In Runtime Path


# Python-Side Application
class App:
    _ctx: Optional[rbc_ext.world.RBCContext] = None
    _initialized = False
    _instance: Optional["App"] = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def init(self, project_path: Path):
        world_path = project_path / "library"
        self.init_ctx()
        self.init_world(world_path)
        self.init_device()
        self._initialized = True

    def init_ctx(self):
        self._ctx = rbc_ext.world.RBCContext()

    def init_world(self, world_path: Path):
        if self._ctx is not None:
            self._ctx.init_world(str(world_path), str(world_path))

    def init_device(
        self, backend_name: str = "dx", program_path: Path = BUILTIN_PROGRAM_PATH
    ):
        shader_path = program_path / f"shader_build_{backend_name}"
        if self._ctx is not None:
            self._ctx.init_device(backend_name, str(program_path), str(shader_path))

    def initialized(self):
        return self._initialized
