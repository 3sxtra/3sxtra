#include "sf33rd/Source/Game/io/file_loader.h"
#include "common.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/io/fs_sys.h"
#include "sf33rd/Source/Game/system/ramcnt.h"
#include "sf33rd/Source/Game/system/work_sys.h"

s32 load_it_use_any_key2(u16 fnum, void** adrs, s16* key, u8 kokey, u8 group) {
    u32 size;
    u32 err;

    if (fnum >= AFS_GetFileCount()) {
        flLogOut("ファイルナンバーに異常があります。ファイル番号：%d\n", fnum);
        return 0;
    }

    size = fsGetFileSize(fnum);
    *key = Pull_ramcnt_key(fsCalSectorSize(size) << 11, kokey, group, 0);
    *adrs = (void*)Get_ramcnt_address(*key);

    err = load_it_use_this_key(fnum, *key);

    if (err != 0) {
        return size;
    }

    Push_ramcnt_key(*key);
    return 0;
}

s16 load_it_use_any_key(u16 fnum, u8 kokey, u8 group) {
    u32 err;
    void* adrs;
    s16 key;

    err = load_it_use_any_key2(fnum, &adrs, &key, kokey, group);

    if (err != 0) {
        return key;
    }

    return 0;
}

s32 load_it_use_this_key(u16 fnum, s16 key) {
    REQ req;
    u32 err;

    req.fnum = fnum;

    while (1) {
        err = fsOpen(&req);

        if (err == 0) {
            continue;
        }

        req.size = fsGetFileSize(req.fnum);
        req.sect = fsCalSectorSize(req.size);
        err = fsFileReadSync(&req, req.sect, (void*)Get_ramcnt_address(key));
        fsClose(&req);
        Set_size_data_ramcnt_key(key, req.size);

        if (err != 0) {
            return 1;
        }

        flLogOut("ファイルの読み込みに失敗しました。ファイル番号：%d\n", fnum);
    }
}
