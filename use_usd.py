from pxr import Usd, UsdGeom

# Create a new stage
stage = Usd.Stage.CreateNew("example.usda")

# Define a new sphere in the stage
UsdGeom.Sphere.Define(stage, "/World/Sphere")

# Save the stage
stage.GetRootLayer().Save()
