import os
import re

renames = {
    'set_new_jpdir': 'set_new_jump_direction',
    'char_move_cmhs': 'char_move_cmd_hit_stop',
    'pp_conv_kow': 'pp_convert_waza_type',
    'effect_E3_move': 'effect_e3_move',
    'effect_E3_init': 'effect_e3_init',
    'effect_K5_move': 'effect_k5_move',
    'effect_K5_init': 'effect_k5_init',
    'init_K5_work': 'init_k5_work',
    'K5_init_data': 'k5_init_data',
    'K5_init_data_copy': 'k5_init_data_copy',
    'K5_init_data_copy2': 'k5_init_data_copy2',
    'K5_main_process': 'k5_main_process',
    'K5_decode_new_hit_index': 'k5_decode_new_hit_index'
}

def process_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        return

    original_content = content
    for old, new in renames.items():
        content = re.sub(rf'\b{old}\b', new, content)

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Updated {filepath}")

def main():
    root_dir = r"d:\3sxtra\src"
    for subdir, dirs, files in os.walk(root_dir):
        for file in files:
            if file.endswith('.c') or file.endswith('.h') or file.endswith('.cpp'):
                process_file(os.path.join(subdir, file))

if __name__ == "__main__":
    main()
