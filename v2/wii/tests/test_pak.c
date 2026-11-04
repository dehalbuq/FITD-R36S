/* Synthetic FITD PAK tests. No copyrighted game data. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/fitd_pak.h"

static void le32(FILE *f,uint32_t n){
    unsigned char b[4]={(unsigned char)n,(unsigned char)(n>>8),
                        (unsigned char)(n>>16),(unsigned char)(n>>24)};
    if(fwrite(b,1,4,f)!=4)abort();
}
static void le16(FILE *f,uint16_t n){
    unsigned char b[2]={(unsigned char)n,(unsigned char)(n>>8)};
    if(fwrite(b,1,2,f)!=2)abort();
}
static int test_good(void){
    FILE *f=tmpfile();
    if(!f)return 1;
    le32(f,0);le32(f,12);le32(f,0); /* 1 archive item, tableEnd=12 */
    le32(f,0); /* no extra descriptor block */
    le32(f,9); /* stored size */
    le32(f,9); /* unpacked size */
    fputc(0,f);fputc(0,f);le16(f,0);
    fwrite("FITD-TEST",1,9,f);
    fflush(f);
    FitdPakDirectory dir={0};FitdPakEntry e={0};
    if(fitd_pak_directory(f,&dir)!=0 || dir.count!=1)return 2;
    if(fitd_pak_entry(f,&dir,0,&e)!=0 ||
       e.compression!=0 || e.packed_size!=9 ||
       e.data_offset!=28)return 3;
    uint8_t *p=NULL;size_t n=0;
    if(fitd_pak_load(f,&e,&p,&n)!=0 ||
       n!=9 || memcmp(p,"FITD-TEST",9)!=0)return 4;
    free(p);
    if(fitd_pak_entry(f,&dir,1,&e)==0)return 5;
    fclose(f);
    return 0;
}
static int test_bad_header(void){
    FILE *f=tmpfile();if(!f)return 1;
    le32(f,0);le32(f,0x7FFFFFFFu);le32(f,0);
    FitdPakDirectory d;
    int st=fitd_pak_directory(f,&d);
    fclose(f);
    return st<0?0:1;
}
static int test_bad_size(void){
    FILE *f=tmpfile();if(!f)return 1;
    le32(f,0);le32(f,12);le32(f,0);le32(f,0);
    le32(f,0xFFFFFFFFU);le32(f,0xFFFFFFFFU);
    fputc(0,f);fputc(0,f);le16(f,0);
    FitdPakDirectory d;FitdPakEntry e;
    int st=fitd_pak_directory(f,&d);
    if(!st)st=fitd_pak_entry(f,&d,0,&e);
    fclose(f);
    return st<0?0:1;
}
int main(void){
    int a=test_good(),b=test_bad_header(),c=test_bad_size();
    if(a||b||c){printf("PAK tests failed: good=%d header=%d size=%d\n",a,b,c);return 1;}
    puts("FITD Wii synthetic archive parser: PASS (endianness, offsets, sizes)");
    return 0;
}
