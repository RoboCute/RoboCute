if __name__ == '__main__':
    import sys
    from pathlib import Path
    from multiprocessing import Process
    sys.path.append(str(Path(__file__).parent / 'scripts/py'))
    modules = ['test_codegen']
    processes = []
    for i in modules:
        exec(f'import {i}')
        p = eval(f'Process(target = {i}.{i}, args = ())')
        p.start()
        processes.append(p)
    for i in processes:
        i.join()
