import sys

files = [
    'src/sf33rd/Source/Game/sound/sound3rd.c',
    'src/sf33rd/Source/Game/stage/bg_load.c',
    'src/sf33rd/Source/Game/stage/ta_sub.c',
    'src/sf33rd/Source/Game/stage/tate00.c',
    'src/sf33rd/Source/Game/system/sys_sub.c'
]

insert = b'\n#include "port/feature_toggles.h"\n'

for fp in files:
    with open(fp, 'rb') as f:
        data = f.read()

    idx = data.rfind(b'#include')
    if idx != -1:
        end_idx = data.find(b'\n', idx) + 1
        new_data = data[:end_idx] + insert + data[end_idx:]
    else:
        new_data = insert + data

    with open(fp, 'wb') as f:
        f.write(new_data)
    print(f'Updated {fp}')
