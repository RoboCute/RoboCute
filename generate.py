if __name__ == '__main__':
    import sys
    from pathlib import Path
    from multiprocessing import Process
    sys.path.append(str(Path(__file__).parent / 'scripts/py'))
    sys.path.append(str(Path(__file__).parent / 'scripts/py/render_codegen'))
    modules = ['test_codegen', 'test_serde', 'pipeline_settings']
    processes = []
    for i in modules:
        exec(f'import {i}')
        p = eval(f'Process(target = {i}.{i}, args = ())')
        p.start()
        processes.append(p)
    for i in processes:
        i.join()
