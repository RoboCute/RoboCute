import inspect

zz_meta = False


class bind:
    def __init__(self, cls):
        self.cls = cls

    def __call__(self, *args, **kwargs):
        inst = self.cls(*args, **kwargs)
        global zz_meta
        if not zz_meta:
            for method in inspect.getmembers(inst, inspect.ismethod):
                setattr(inst, method[0], lambda: print("override method"))
            return inst
        else:
            print(inst.__class__.__name__)
            for method in inspect.getmembers(inst, inspect.ismethod):
                print(method[0])
                sig = inspect.signature(method[1])
                print(sig.parameters)
                print(sig.return_annotation)
            inst.zz_generated = lambda: print("generated: ")

            return inst


@bind
class MetaClass0:
    def get_name(self) -> str: ...
    def some_method(self, x: int, y: str) -> float: ...


@bind
class MetaClass1:
    def get_name(self):
        return self.name
