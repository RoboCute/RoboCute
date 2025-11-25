import robocute as rbc
import time


def on_complete(resource_id, res):
    print("Loaded Resource ID", resource_id)


def main():
    print("=== RoboCute Resource Management Example ===/n")
    scene = rbc.Scene(cache_budget_mb=2048)
    scene.start()

    # Access the resource manager
    resource_mgr = scene.resource_manager

    print("[1] Loading basic resources...")
    mesh_id = resource_mgr.load_mesh(
        "D:/ws/data/assets/models/cube.obj",
        priority=rbc.LoadPriority.High,
        on_complete=on_complete,
    )
    print(f"   Registered mesh: {mesh_id}")
    robot = scene.create_entity("Robot")
    scene.add_component(
        robot.id,
        "transform",
        rbc.TransformComponent(
            position=[0.0, 1.0, 0.0],
            rotation=[0.0, 0.0, 0.0, 1.0],
            scale=[1.0, 1.0, 1.0],
        ),
    )
    scene.stop()

    print("Scene Complete")


if __name__ == "__main__":
    main()
