/**
 * @file vm_sub.c
 * @brief Subroutines to configure memory-card file operations.
 *
 * Part of the io module.
 */

#include "sf33rd/Source/Game/io/vm_sub.h"
#include "game_state.h"
#include "common.h"
#include "sf33rd/Source/Game/io/vm.h"
#include "sf33rd/Source/Game/io/vm_data.h"
#include "sf33rd/Source/Game/system/work_sys.h"

/** @brief Issue a VM access request (load/save) to the given drive. */
u8 VM_Access_Request(u8 Request, u8 Drive) {
    g_state.vm_w.Request = Request;
    g_state.vm_w.Drive = Drive;
    return 1;
}

/** @brief Set file name, type, save size, block size, and icon for a file type. */
void Setup_File_Property(s16 file_type, u8 number) {
    switch (file_type) {
    case 0:
        g_state.vm_w.File_Name = SystemFileName;
        g_state.vm_w.File_Type = 0;
        g_state.vm_w.Save_Size = 0xC00;
        g_state.vm_w.Block_Size = 3;
        g_state.vm_w.Icon_Type = 0;
        break;

    case 3:
        break;

    case 1:
        g_state.vm_w.File_Name = Replay_File_Name[number];
        g_state.vm_w.File_Type = 1;
        g_state.vm_w.Save_Size = 0x3C00;
        g_state.vm_w.Block_Size = 0xF;
        g_state.vm_w.Icon_Type = 2;
        break;

    case 2:
        g_state.vm_w.File_Name = SysDir_File_Name[number];
        g_state.vm_w.File_Type = 2;
        g_state.vm_w.Save_Size = 0x400;
        g_state.vm_w.Block_Size = 1;
        g_state.vm_w.Icon_Type = 5;
        break;
    }
}
