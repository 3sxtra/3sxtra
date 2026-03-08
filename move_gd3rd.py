import os


def extract_block(text, start_str, end_str=None, end_char=None):
    start_idx = text.find(start_str)
    if start_idx == -1:
        return "", text

    if end_str:
        end_idx = text.find(end_str, start_idx)
        if end_idx != -1:
            end_idx += len(end_str)
    elif end_char:
        # find matching bracket or brace
        stack = []
        in_block = False
        end_idx = start_idx
        for i in range(start_idx, len(text)):
            c = text[i]
            if c == "{":
                stack.append(c)
                in_block = True
            elif c == "}":
                stack.pop()
                if not stack and in_block:
                    end_idx = i + 1
                    break

    if end_idx == start_idx or end_idx == -1:
        return "", text

    # Backtrack to start of line for start_idx
    while start_idx > 0 and text[start_idx - 1] not in ["\n"]:
        start_idx -= 1
    # If there is a docstring above, grab it
    doc_idx = text.rfind("/**\n * @brief", 0, start_idx)
    # Check if this docstring is immediately above
    if (
        doc_idx != -1
        and text[doc_idx:start_idx].strip() == text[doc_idx : doc_idx + 20].strip()
    ):
        start_idx = doc_idx

    block = text[start_idx:end_idx]
    new_text = text[:start_idx] + text[end_idx:]
    return block, new_text


def remove_blocks(text, prefixes):
    extracted = []
    lines = text.split("\n")
    i = 0
    while i < len(lines):
        line = lines[i]
        matched = False
        for p in prefixes:
            if line.startswith(p):
                matched = True
                break
        if matched:
            start_i = i
            # Check for doc block
            if i > 0 and lines[i - 1] == " */":
                doc_start = i - 1
                while doc_start > 0 and not lines[doc_start].startswith("/**"):
                    doc_start -= 1
                if lines[doc_start].startswith("/**"):
                    start_i = doc_start

            # Find end of function
            end_i = i
            stack = 0
            in_fn = False
            while end_i < len(lines):
                for c in lines[end_i]:
                    if c == "{":
                        stack += 1
                        in_fn = True
                    elif c == "}":
                        stack -= 1
                if in_fn and stack == 0:
                    break
                end_i += 1

            block = "\n".join(lines[start_i : end_i + 1]) + "\n"
            extracted.append(block)
            del lines[start_i : end_i + 1]
            i = start_i  # re-evaluate at this position
        else:
            i += 1

    return "\n".join(extracted), "\n".join(lines)


def main():
    root = "d:/3sxtra/src/sf33rd/Source/Game/io"
    gd_c = os.path.join(root, "gd3rd.c")
    gd_h = os.path.join(root, "gd3rd.h")
    fs_c = os.path.join(root, "fs_sys.c")
    fs_h = os.path.join(root, "fs_sys.h")
    fl_c = os.path.join(root, "file_loader.c")
    fl_h = os.path.join(root, "file_loader.h")

    with open(gd_c, "r", encoding="utf-8") as f:
        c_text = f.read()
    with open(gd_h, "r", encoding="utf-8") as f:
        h_text = f.read()

    fs_funcs = [
        "s32 fsOpen",
        "void fsClose",
        "u32 fsGetFileSize",
        "u32 fsCalSectorSize",
        "static s32 fsCansel",
        "s32 fsCheckCommandExecuting",
        "s32 fsRequestFileRead",
        "s32 fsCheckFileReaded",
        "s32 fsFileReadSync",
        "void waitVsyncDummy",
    ]

    fl_funcs = [
        "s32 load_it_use_any_key2",
        "s16 load_it_use_any_key",
        "s32 load_it_use_this_key",
    ]

    fs_data, c_text = remove_blocks(c_text, fs_funcs)
    fl_data, c_text = remove_blocks(c_text, fl_funcs)

    # Extract afs_handle decl
    afs_handle_decl = "static AFSHandle afs_handle = AFS_NONE;\n"
    c_text = c_text.replace(afs_handle_decl, "")
    fs_data = afs_handle_decl + "\n" + fs_data

    # Generate fs_sys.h
    fs_h_content = """#ifndef FS_SYS_H
#define FS_SYS_H

#include "structs.h"
#include "types.h"

s32 fsOpen(REQ* req);
void fsClose(REQ* /* unused */);
u32 fsGetFileSize(u16 fnum);
u32 fsCalSectorSize(u32 size);
s32 fsCansel(REQ* req);
s32 fsCheckCommandExecuting();
s32 fsRequestFileRead(REQ* req, u32 sec, void* buff);
s32 fsCheckFileReaded(REQ* req);
s32 fsFileReadSync(REQ* req, u32 sec, void* buff);
void waitVsyncDummy();

#endif
"""
    # Generate file_loader.h
    fl_h_content = """#ifndef FILE_LOADER_H
#define FILE_LOADER_H

#include "structs.h"
#include "types.h"

s32 load_it_use_any_key2(u16 fnum, void** adrs, s16* key, u8 kokey, u8 group);
s16 load_it_use_any_key(u16 fnum, u8 kokey, u8 group);
s32 load_it_use_this_key(u16 fnum, s16 key);

#endif
"""

    # Generate fs_sys.c
    fs_c_content = """#include "sf33rd/Source/Game/io/fs_sys.h"
#include "common.h"
#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/emlTSB.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "port/io/afs.h"

""" + fs_data.replace("static s32 fsCansel", "s32 fsCansel")

    # Generate file_loader.c
    fl_c_content = (
        """#include "sf33rd/Source/Game/io/file_loader.h"
#include "sf33rd/Source/Game/io/fs_sys.h"
#include "common.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/system/ramcnt.h"
#include "sf33rd/Source/Game/system/work_sys.h"

"""
        + fl_data
    )

    # Remove decls from gd3rd.h
    h_lines = h_text.split("\n")
    new_h_lines = []
    for l in h_lines:
        ok = True
        for f in fs_funcs + fl_funcs:
            if f.split(" ")[-1] in l and ";" in l:
                if "fsCansel" in l:
                    continue
                ok = False
        if ok and "waitVsyncDummy" not in l and "fsCansel" not in l:
            new_h_lines.append(l)

    h_text = "\n".join(new_h_lines)
    h_text = h_text.replace(
        '#include "types.h"',
        '#include "types.h"\n#include "sf33rd/Source/Game/io/fs_sys.h"\n#include "sf33rd/Source/Game/io/file_loader.h"',
    )

    c_text = c_text.replace('#include "port/io/afs.h"', "")
    c_text = c_text.replace(
        "static void fsCansel(REQ* /* unused */)", "s32 fsCansel(REQ* req)"
    )  # in case

    with open(fs_c, "w", encoding="utf-8") as f:
        f.write(fs_c_content)
    with open(fs_h, "w", encoding="utf-8") as f:
        f.write(fs_h_content)
    with open(fl_c, "w", encoding="utf-8") as f:
        f.write(fl_c_content)
    with open(fl_h, "w", encoding="utf-8") as f:
        f.write(fl_h_content)
    with open(gd_c, "w", encoding="utf-8") as f:
        f.write(c_text)
    with open(gd_h, "w", encoding="utf-8") as f:
        f.write(h_text)


if __name__ == "__main__":
    main()
