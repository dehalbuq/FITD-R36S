#include <stdio.h>
#include <stdint.h>
#include "../source/fitd_endian.h"
int main(void){
    const uint8_t v[]={0x34,0x12,0xFF,0xFF,0xEF,0xCD,0xAB,0x89};
    if(!fitd_endian_selftest())return 2;
    if(fitd_read_le16(v)!=0x1234U)return 3;
    if(fitd_read_sle16(v+2)!=-1)return 4;
    if(fitd_read_le32(v+4)!=0x89ABCDEFU)return 5;
    puts("FITD Wii endian-safe archive reads: PASS");
    return 0;
}
