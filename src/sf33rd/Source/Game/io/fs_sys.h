#ifndef FS_SYS_H
#define FS_SYS_H

#include "structs.h"
#include "types.h"

/** @brief Open a file on the filesystem */
s32 fsOpen(REQ* req);

/** @brief Close an open file */
void fsClose(REQ* /* unused */);

/** @brief Get the size of a file in bytes */
u32 fsGetFileSize(u16 fnum);

/** @brief Calculate sector size for file */
u32 fsCalSectorSize(u32 size);

/** @brief Cancel an asynchronous read */
s32 fsCansel(REQ* req);

/** @brief Check if a command is currently executing */
s32 fsCheckCommandExecuting();

/** @brief Issue an asynchronous read request */
s32 fsRequestFileRead(REQ* req, u32 sec, void* buff);

/** @brief Check if an asynchronous read has completed */
s32 fsCheckFileReaded(REQ* req);

/** @brief Perform a synchronous file read */
s32 fsFileReadSync(REQ* req, u32 sec, void* buff);

/** @brief Dummy vsync wait */
void waitVsyncDummy();

#endif
