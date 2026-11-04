/*
 * FITD PAK reader for big-endian Wii PowerPC, also host-testable.
 * Source format traced to yaz0r/FITD FitdLib/pak.cpp @ 543e22bc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_PAK_H
#define FITD_WII_PAK_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define FITD_PAK_MAX_ENTRIES 8192u
#define FITD_PAK_MAX_SIZE (8u*1024u*1024u)
typedef struct {
    uint32_t packed_size;
    uint32_t unpacked_size;
    uint32_t data_offset;
    uint8_t compression;  /* FITD 0 = raw, 1 = implode, 4 = deflate */
    uint8_t flags;
    uint32_t file_offset;
} FitdPakEntry;
typedef struct {
    uint32_t count;
    uint32_t file_size;
    uint32_t table_end;
} FitdPakDirectory;
/* All functions return zero on success, negative status on failure. */
int fitd_pak_directory(FILE *f,FitdPakDirectory *result);
int fitd_pak_entry(FILE *f,const FitdPakDirectory *d,
                   uint32_t index,FitdPakEntry *out);
int fitd_pak_load(FILE *f,const FitdPakEntry *e,
                  uint8_t **out,size_t *out_size);
const char *fitd_pak_error(int code);
/* Original Cyril Voila / Mark Adler implode decoder, adapted for Wii. */
int fitd_pak_explode(const uint8_t *src,uint8_t *dst,
                     uint32_t packed,uint32_t unpacked,uint16_t flags);
#endif
