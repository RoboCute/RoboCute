import uipc
from uipc import Engine, World, Scene, view, Vector3, builtin
from uipc.unit import MPa, GPa, kPa
from uipc.constitution import (
    AffineBodyConstitution,
    NeoHookeanShell,
    DiscreteShellBending,
    ElasticModuli,
)

from uipc.geometry import trimesh, label_surface

import numpy as np
import os
import json


def sim_impl(ws_root: str, debug=False):
    print(f"sim with {ws_root}")
