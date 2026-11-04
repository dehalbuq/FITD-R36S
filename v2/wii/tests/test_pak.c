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
/* Physical FITD layout: first_offset=12 means TWO entries, because
 * the header has one reserved u32 and two offset u32 slots.
 * Real ETAGE00.PAK uses this form with stage rooms at index 0 and
 * stage cameras at index 1. Test data below is completely synthetic.
 */
static int test_good(void){
    FILE *f=tmpfile();
    if(!f)return 1;
    le32(f,0);le32(f,12);le32(f,37); /* two offsets, first payload starts at 12 */
    le32(f,0); /* descriptor 0 => 4-byte prefix */
    le32(f,9);le32(f,9);
    fputc(0,f);fputc(0,f);le16(f,0);
    fwrite("FITD-TEST",1,9,f);
    le32(f,0);
    le32(f,7);le32(f,7);
    fputc(0,f);fputc(0,f);le16(f,0);
    fwrite("STAGE-1",1,7,f);
    fflush(f);
    FitdPakDirectory dir={0};FitdPakEntry e={0};
    if(fitd_pak_directory(f,&dir)!=0 || dir.count!=2)return 2;
    if(fitd_pak_entry(f,&dir,0,&e)!=0 ||
       e.compression!=0 || e.packed_size!=9 ||
       e.data_offset!=28)return 3;
    uint8_t *p=NULL;size_t n=0;
    if(fitd_pak_load(f,&e,&p,&n)!=0 ||
       n!=9 || memcmp(p,"FITD-TEST",9)!=0){free(p);return 4;}
    free(p);
    if(fitd_pak_entry(f,&dir,1,&e)!=0 ||
       e.data_offset!=53 || e.packed_size!=7)return 5;
    p=NULL;n=0;
    if(fitd_pak_load(f,&e,&p,&n)!=0 ||
       n!=7 || memcmp(p,"STAGE-1",7)!=0){free(p);return 6;}
    free(p);
    if(fitd_pak_entry(f,&dir,2,&e)==0)return 7;
    fclose(f);
    return 0;
}
static int test_single_entry(void){
    FILE *f=tmpfile();
    if(!f)return 1;
    le32(f,0);le32(f,8); /* one entry, offset 8 */
    le32(f,0);
    le32(f,3);le32(f,3);
    fputc(0,f);fputc(0,f);le16(f,0);
    fwrite("ONE",1,3,f);
    fflush(f);
    FitdPakDirectory d={0};FitdPakEntry e={0};
    int ok=(fitd_pak_directory(f,&d)==0 && d.count==1 &&
            fitd_pak_entry(f,&d,0,&e)==0 &&
            fitd_pak_entry(f,&d,1,&e)<0);
    fclose(f);
    return ok?0:1;
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
    int a=test_good(),b=test_bad_header(),c=test_bad_size(),d=test_single_entry();
    if(a||b||c||d){printf("PAK tests failed: good=%d header=%d size=%d single=%d\n",a,b,c,d);return 1;}
    puts("FITD Wii synthetic archive parser: PASS (endian, physical entry count including ETAGE camera, last item)");
    return 0;
}
