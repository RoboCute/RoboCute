from rbc_meta.utils.reflect import reflect, rpc


@reflect(cpp_namespace="rbc", module_name="test_ipc")
class Chat:
    @rpc(is_static=True)
    def chat(value: str) -> str: ...

    @rpc(is_static=True)
    def exit(): ...
