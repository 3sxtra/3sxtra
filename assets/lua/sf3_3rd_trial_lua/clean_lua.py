import io


def clean():
    try:
        with io.open(
            "D:/3sxprivated/sf3_3rd_trial_lua/sf3_3rd_trial_lua_20220613/sf3_3rd_trial_20220613.lua",
            "r",
            encoding="shift_jis",
            errors="replace",
        ) as f:
            content = f.read()
        lines = content.splitlines()
        with io.open(
            "D:/3sxprivated/sf3_3rd_trial_lua/sf3_3rd_trial_clean.lua",
            "w",
            encoding="utf-8",
        ) as out:
            out.write("\n".join(lines))
        print(f"Cleaned {len(lines)} lines")
    except Exception as e:
        print(e)


if __name__ == "__main__":
    clean()
