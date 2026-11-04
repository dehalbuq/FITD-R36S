/*
 * FITD PAK archive reader. Data on disk is always little-endian;
 * Wii / PowerPC host values are big-endian.
 *
 * Layout (from yaz0r/FITD FitdLib/pak.cpp):
 *   u32 reserved
 *   u32 offsets[count]   index i offset lives at file offset (i+1)*4
 *   u32 additional_desc_size
 *   [additional_desc_size-4 bytes, when nonzero]
 *   u32 packed_bytes, u32 expanded_bytes, u8 compression, u8 flags,
 *   u16 optional_name_bytes, [optional name], payload
 *
 * This module never mutates source archives, only reads.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fitd_pak.h"
#include "fitd_endian.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static int read_at(FILE *f,uint32_t pos,void *to,size_t size){
    if(!f || pos>LONG_MAX || fseek(f,(long)pos,SEEK_SET)!=0)return -1;
    return fread(to,1,size,f)==size?0:-2;
}

int fitd_pak_directory(FILE *f,FitdPakDirectory *d){
    if(!f || !d)return -1;
    memset(d,0,sizeof(*d));
    if(fseek(f,0,SEEK_END)!=0)return -2;
    const long length=ftell(f);
    if(length<12 || (uint64_t)length>UINT32_MAX)return -3;
    uint8_t b[8];
    if(read_at(f,0,b,sizeof(b))<0)return -2;
    const uint32_t first=fitd_read_le32(b+4);
    if((first&3u)!=0 || first<12u ||
       first>(uint32_t)length || first/4u<2u)return -4;
    const uint32_t count=first/4u-2u;
    if(count>FITD_PAK_MAX_ENTRIES)return -5;
    d->file_size=(uint32_t)length;
    d->count=count;
    d->table_end=first;
    return 0;
}

int fitd_pak_entry(FILE *f,const FitdPakDirectory *d,
                   uint32_t i,FitdPakEntry *out){
    if(!f || !d || !out || i>=d->count)return -1;
    memset(out,0,sizeof(*out));
    uint8_t raw[12];
    uint32_t ptr=(i+1u)*4u;
    uint8_t b[4];
    if(ptr>d->table_end-4 || read_at(f,ptr,b,4)<0)return -2;
    const uint32_t off=fitd_read_le32(b);
    if(off<d->table_end || off>d->file_size-16u)return -4;
    if(read_at(f,off,b,4)<0)return -2;
    const uint32_t extra=fitd_read_le32(b);
    /* Normal archive descriptor is zero (just four bytes). Nonzero
       descriptors include their own four-byte length field. */
    const uint32_t prefix=extra?extra:4u;
    if(prefix<4u || prefix>d->file_size-off-12u)return -4;
    const uint32_t meta=off+prefix;
    if(read_at(f,meta,raw,sizeof(raw))<0)return -2;
    const uint32_t packed=fitd_read_le32(raw);
    const uint32_t unpacked=fitd_read_le32(raw+4);
    const uint32_t name_len=fitd_read_le16(raw+10);
    const uint8_t compression=raw[8];
    const uint32_t payload=meta+12u+name_len;
    if(payload<meta+12u || payload>d->file_size ||
       packed>d->file_size-payload)return -4;
    if(packed>FITD_PAK_MAX_SIZE || unpacked>FITD_PAK_MAX_SIZE)return -5;
    if(compression!=0 && compression!=1 && compression!=4)return -6;
    if((compression==0 && packed!=unpacked) ||
       packed==0 || unpacked==0)return -4;
    out->file_offset=off;
    out->packed_size=packed;
    out->unpacked_size=unpacked;
    out->data_offset=payload;
    out->compression=compression;
    out->flags=raw[9];
    return 0;
}

int fitd_pak_load(FILE *f,const FitdPakEntry *e,
                  uint8_t **out,size_t *output_size){
    if(!f || !e || !out || !output_size)return -1;
    *out=NULL;*output_size=0;
    if(!e->unpacked_size || e->unpacked_size>FITD_PAK_MAX_SIZE ||
       e->packed_size>FITD_PAK_MAX_SIZE)return -5;
    uint8_t *decoded=(uint8_t*)malloc(e->unpacked_size);
    if(!decoded)return -7;
    int status=0;
    if(e->compression==0){
        if(read_at(f,e->data_offset,decoded,e->unpacked_size)<0)status=-2;
    } else if(e->compression==1){
        uint8_t *in=(uint8_t*)malloc(e->packed_size);
        if(!in){free(decoded);return -7;}
        if(read_at(f,e->data_offset,in,e->packed_size)<0 ||
           fitd_pak_explode(in,decoded,e->packed_size,
                            e->unpacked_size,e->flags)!=0)status=-8;
        free(in);
    } else {
        /* Wii Test02 supports original FITD compression 0/1.
           Compression 4 uses raw DEFLATE (requires a dedicated decoder). */
        status=-9;
    }
    if(status){free(decoded);return status;}
    *out=decoded;
    *output_size=e->unpacked_size;
    return 0;
}

const char *fitd_pak_error(int e){
    switch(e){
    case 0:return "OK";
    case -1:return "BAD_ARGUMENT_OR_INDEX";
    case -2:return "IO_ERROR";
    case -3:return "TRUNCATED_ARCHIVE";
    case -4:return "BAD_PAK_OFFSETS_OR_SIZES";
    case -5:return "EXCEEDS_MEMORY_OR_ENTRY_LIMIT";
    case -6:return "UNKNOWN_COMPRESSION";
    case -7:return "OUT_OF_MEMORY";
    case -8:return "IMPLODE_DECODER_FAILED";
    case -9:return "DEFLATE_NOT_YET_SUPPORTED";
    default:return "PAK_ERROR";
    }
}
