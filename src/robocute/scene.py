# 运行时Scene数据的集合，可以load/save成一个.rbc文件
class Scene:
    def __init__(self):
        pass

    def load(self, path: str):
        if not path.endiswith(".rbc"):
            print("Invalid Scnee Path, use '.rbc' file format")
            return


class BaseComponent:
    def __init__(self):
        pass


class TransformComponent:
    def __init__(self):
        pass


class MeshComponent:
    def __init__(self):
        pass
