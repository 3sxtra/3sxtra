#include "sf33rd/Source/Game/io/fs_sys.h"
#include "common.h"
#include "port/io/afs.h"
#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/emlTSB.h"
#include "sf33rd/Source/Game/debug/Debug.h"

static AFSHandle afs_handle = AFS_NONE;

s32 fsOpen(REQ* req) {
    if (req->fnum >= AFS_GetFileCount()) {
        return 0;
    }

    if (afs_handle != AFS_NONE) {
        AFS_Close(afs_handle);
    }

    afs_handle = AFS_Open(req->fnum);

    req->info.number = 1;
    return 1;
}

void fsClose(REQ* /* unused */) {
    AFS_Close(afs_handle);
    afs_handle = AFS_NONE;
}

u32 fsGetFileSize(u16 fnum) {
    if (fnum >= AFS_GetFileCount()) {
        return 0;
    }

    return AFS_GetSize(fnum);
}

u32 fsCalSectorSize(u32 size) {
    return (size + 2048 - 1) / 2048;
}

s32 fsCansel(REQ* /* unused */) {
    if ((afs_handle != AFS_NONE) && (AFS_GetState(afs_handle) == AFS_READ_STATE_READING)) {
        AFS_Stop(afs_handle);
    }

    return 1;
}

s32 fsCheckCommandExecuting() {
    if (afs_handle == AFS_NONE) {
        return 0;
    }

    const AFSReadState state = AFS_GetState(afs_handle);

    switch (state) {
    case AFS_READ_STATE_READING:
    case AFS_READ_STATE_ERROR:
        return 1;

    case AFS_READ_STATE_IDLE:
    case AFS_READ_STATE_FINISHED:
        return 0;

    default:
        fatal_error("Unhandled AFS state: %d", state);
    }
}

s32 fsRequestFileRead(REQ* /* unused */, u32 sec, void* buff) {
    AFS_Read(afs_handle, sec, buff);
    return 1;
}

s32 fsCheckFileReaded(REQ* /* unused */) {
    const AFSReadState state = AFS_GetState(afs_handle);

    switch (state) {
    case AFS_READ_STATE_ERROR:
        return 2;

    case AFS_READ_STATE_READING:
        return 0;

    case AFS_READ_STATE_IDLE:
    case AFS_READ_STATE_FINISHED:
        return 1;

    default:
        fatal_error("Unhandled AFS state: %d", state);
    }
}

s32 fsFileReadSync(REQ* req, u32 sec, void* buff) {
    AFS_ReadSync(afs_handle, sec, buff);
    const s32 rnum = fsCheckFileReaded(req);
    return (rnum == 1) ? 1 : 0;
}

void waitVsyncDummy() {
    // AFS_RunServer is called here intentionally to keep streaming operational during
    // synchronous file reads. This prevents audio/streaming stalls when the main loop
    // is blocked on file I/O. Moving this to only the main loop would break sync reads.
    AFS_RunServer();
    mlTsbExecServer();
}
