from meta_file import bind


@bind
class SubMetaClass:
    def get_name(self) -> str: ...
