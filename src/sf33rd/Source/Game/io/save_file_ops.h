/**
 * @file save_file_ops.h
 * @brief Memory-card file operation setup helpers.
 *
 * Part of the io module.
 */

#ifndef SAVE_FILE_OPS_H
#define SAVE_FILE_OPS_H

#include "types.h"

u8 VM_Access_Request(u8 Request, u8 Drive);
void Setup_File_Property(s16 file_type, u8 number);

#endif
