import inspect

import meta_file

for cls_tuple in inspect.getmembers(meta_file, lambda x: isinstance(x, meta_file.bind)):
    print(cls_tuple)
    inst_gen = cls_tuple[1]()
    try:
        inst_gen.get_name()
        inst_gen.zz_generated()
    except:
        print(cls_tuple[0], " zz_geneareted() not defined")
