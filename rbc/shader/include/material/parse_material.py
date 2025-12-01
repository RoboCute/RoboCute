from pathlib import Path
file_dir = Path(__file__).parent / 'openpbr.hpp'

f = open(file_dir, 'r')
s = f.read()
f.close()

s = s.replace('\t', ' ').replace('    ', ' ').replace('  ', ' ').replace(';', '')

lines = s.replace('\r\n', '\n').split('\n')
stack = []
finished_struct = []
basic_types = {'int', 'float', 'uint', 'half', 'float', 'bool'}
types = [
    'MatImageHandle', 'MatVolumeHandle', 'MatBufferHandle',
]
for t in basic_types:
    for i in range(2, 5):
        types.append(f'std::array<{t},{i}>')
        types.append(f'{t}{i}')
for t in basic_types:
    types.append(t)
for line in lines:
    words = line.split(' ')
    for init_idx in range(len(words)):
        if words[init_idx] != '':
            break
    if init_idx >= (len(words) - 1):
        continue
    # start struct
    if words[init_idx] == 'struct' and init_idx < len(words) - 1:
        stack.append([])
        continue
    
    if words[init_idx] == '}':
        if len(stack) == 0:
            continue
        finished_struct.append((
            stack[-1], words[init_idx + 1]
        ))
        del stack[-1]
        continue
    
    line = line.replace(' ', '')
    for type in types:
        find_idx = line.find(type)
        if find_idx != 0:
            continue
        name_start = find_idx + len(type)
        words = line[name_start:].split('{')
        for init_idx in range(len(words)):
            if words[init_idx] != '':
                break
        if len(stack) > 1:
            stack[-1].append(words[init_idx])
            break
        
for s in finished_struct:
    for word in s[0]:
        f = f'serde_func(x.{s[1]}.{word}, "{s[1]}_{word}");'
        f = f.replace(f'{s[1]}_{s[1]}_', f'{s[1]}_')
        print(f)