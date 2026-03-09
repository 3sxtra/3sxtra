import io

def convert_to_utf8(input_path, output_path):
    try:
        with io.open(input_path, 'r', encoding='shift_jis', errors='replace') as f:
            content = f.read()
        with io.open(output_path, 'w', encoding='utf-8') as out:
            out.write(content)
        print("Conversion successful.")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    convert_to_utf8(
        'D:/3sxprivated/sf3_3rd_trial_lua/sf3_3rd_trial_lua_20220613/sf3_3rd_trial_20220613.lua',
        'D:/3sxprivated/sf3_3rd_trial_lua/sf3_3rd_trial_utf8.lua'
    )
