if __name__ == '__main__':
    from pathlib import Path
    from multiprocessing import Process
    file_path = Path(__file__)
    curr_dir = file_path.parent
    ps = []
    for i in curr_dir.glob("**/*.py"):
        if i != file_path:
            py_file_name = i.name
            py_file_name = py_file_name[0:len(py_file_name) - 3]
            exec(f'import {py_file_name}')
            p = eval(
                f'Process(target = {py_file_name}.{py_file_name}, args = ())')
            p.start()
            ps.append(p)
    for i in ps:
        i.join()
