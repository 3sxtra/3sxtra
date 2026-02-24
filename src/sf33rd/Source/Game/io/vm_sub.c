/**
 * @file vm_sub.c
 * @brief Subroutines to configure memory-card file operations.
 *
 * Part of the io module.
 */

#include "sf33rd/Source/Game/io/vm_sub.h"
#include "common.h"
#include "sf33rd/Source/Game/io/vm.h"
#include "sf33rd/Source/Game/io/vm_data.h"
#include "sf33rd/Source/Game/system/work_sys.h"

/** @brief Issue a VM access request (load/save) to the given drive. */
u8 VM_Access_Request(u8 Request, u8 Drive) {
    vm_w.Request = Request;
    vm_w.Drive = Drive;
    return 1;
}

/** @brief Set file name, type, save size, block size, and icon for a file type. */
void Setup_File_Property(s16 file_type, u8 number) {
    switch (file_type) {
    case 0:
        vm_w.File_Name = SystemFileName;
        vm_w.File_Type = 0;
        vm_w.Save_Size = 0xC00;
        vm_w.Block_Size = 3;
        vm_w.Icon_Type = 0;
        break;

    case 3:
        break;

    case 1:
        vm_w.File_Name = Replay_File_Name[number];
        vm_w.File_Type = 1;
        vm_w.Save_Size = 0x3C00;
        vm_w.Block_Size = 0xF;
        vm_w.Icon_Type = 2;
        break;

    case 2:
        vm_w.File_Name = SysDir_File_Name[number];
        vm_w.File_Type = 2;
        vm_w.Save_Size = 0x400;
        vm_w.Block_Size = 1;
        vm_w.Icon_Type = 5;
        break;
    }
}
