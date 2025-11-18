import inspect

import meta_file
import submeta

meta_file.zz_meta = True

for cls_tuple in inspect.getmembers(meta_file, lambda x: isinstance(x, meta_file.bind)):
    print(cls_tuple)
    inst_gen = cls_tuple[1]()
    inst_gen.zz_generated()

for cls_tuple in inspect.getmembers(submeta, lambda x: isinstance(x, meta_file.bind)):
    print(cls_tuple)
    inst_gen = cls_tuple[1]()
    inst_gen.zz_generated()
