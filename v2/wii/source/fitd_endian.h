/*
 * FITD archives encode little-endian values; Wii PowerPC is big-endian.
 * Never reinterpret unaligned game bytes as native short/int.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_ENDIAN_H
#define FITD_WII_ENDIAN_H
#include <stdint.h>
static inline uint16_t fitd_read_le16(const void *src){
    const uint8_t *p=(const uint8_t *)src;
    return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8));
}
static inline int16_t fitd_read_sle16(const void *src){
    return (int16_t)fitd_read_le16(src);
}
static inline uint32_t fitd_read_le32(const void *src){
    const uint8_t *p=(const uint8_t *)src;
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|
           ((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static inline int fitd_is_big_endian(void){
    const uint32_t probe=0x10203040U;
    return *((const uint8_t *)&probe)==0x10;
}
static inline int fitd_endian_selftest(void){
    const uint8_t data[]={0xCD,0xAB,0x78,0x56,0x34,0x12,0xFF,0xFF};
    return fitd_read_le16(data)==0xABCDU &&
           fitd_read_le32(data+2)==0x12345678U &&
           fitd_read_sle16(data+6)==-1;
}
#endif
