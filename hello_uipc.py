import threading
import time
import asyncio

import os
import pathlib


class AssetDir:
    this_file = pathlib.Path(os.path.dirname(__file__)).resolve()
    _output_path = pathlib.Path(this_file / "../data/output/").resolve()
    _assets_path = pathlib.Path(this_file / "../data/assets/").resolve()
    _tetmesh_path = _assets_path / "sim_data" / "tetmesh"
    _trimesh_path = _assets_path / "sim_data" / "trimesh"

    @staticmethod
    def asset_path():
        return str(AssetDir._assets_path)

    @staticmethod
    def tetmesh_path():
        return str(AssetDir._tetmesh_path)

    @staticmethod
    def trimesh_path():
        return str(AssetDir._trimesh_path)

    @staticmethod
    def output_path(file):
        file_dir = pathlib.Path(file).absolute()
        this_python_root = AssetDir.this_file.parent.parent
        # get the relative path from the python root to the file
        relative_path = file_dir.relative_to(this_python_root)
        # construct the output path
        output_dir = AssetDir._output_path / relative_path / ""
        if not os.path.exists(output_dir):
            os.makedirs(output_dir)
        return str(output_dir)

    @staticmethod
    def folder(file):
        return pathlib.Path(file).absolute().parent


import numpy as np

import uipc
from uipc import Logger, Timer
from uipc.core import Engine, World, Scene
from uipc.geometry import (
    tetmesh,
    label_surface,
    label_triangle_orient,
    flip_inward_triangles,
)
from uipc.constitution import AffineBodyConstitution
from uipc.gui import SceneGUI
from uipc.unit import MPa, GPa

Timer.enable_all()
Logger.set_level(Logger.Level.Warn)

workspace = AssetDir.output_path(__file__)
engine = Engine("cuda", workspace)
world = World(engine)
config = Scene.default_config()
dt = 0.02
config["dt"] = dt
config["gravity"] = [[0.0], [-9.8], [0.0]]
scene = Scene(config)

# create constitution and contact model
abd = AffineBodyConstitution()

# friction ratio and contact resistance
scene.contact_tabular().default_model(0.5, 1.0 * GPa)
default_element = scene.contact_tabular().default_element()

# create a regular tetrahedron
Vs = np.array(
    [[0, 1, 0], [0, 0, 1], [-np.sqrt(3) / 2, 0, -0.5], [np.sqrt(3) / 2, 0, -0.5]]
)
Ts = np.array([[0, 1, 2, 3]])

# setup a base mesh to reduce the later work
base_mesh = tetmesh(Vs, Ts)
# apply the constitution and contact model to the base mesh
abd.apply_to(base_mesh, 100 * MPa)
# apply the default contact model to the base mesh
default_element.apply_to(base_mesh)

# label the surface, enable the contact
label_surface(base_mesh)
# label the triangle orientation to export the correct surface mesh
label_triangle_orient(base_mesh)
# flip the triangles inward for better rendering
base_mesh = flip_inward_triangles(base_mesh)

mesh1 = base_mesh.copy()
pos_view = uipc.view(mesh1.positions())
# move the mesh up for 1 unit
pos_view += uipc.Vector3.UnitY() * 1.5

mesh2 = base_mesh.copy()
is_fixed = mesh2.instances().find(uipc.builtin.is_fixed)
is_fixed_view = uipc.view(is_fixed)
is_fixed_view[:] = 1

# create objects
object1 = scene.objects().create("upper_tet")
object1.geometries().create(mesh1)

object2 = scene.objects().create("lower_tet")
object2.geometries().create(mesh2)

world.init(scene)
run = False


world.advance()
world.retrieve()
