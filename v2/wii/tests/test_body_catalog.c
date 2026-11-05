#include <stdio.h>
#include <string.h>
#include "../source/fitd_body_catalog.h"
int main(void){
    FitdBody b={0};
    if(fitd_body_catalog_match(&b))return 1;
    b.valid=true;b.group_count=18;b.vertex_count=170;b.primitive_count=150;
    if(!fitd_body_catalog_match(&b))return 2;
    b.group_count=20;if(!fitd_body_catalog_match(&b))return 3;
    b.group_count=19;if(fitd_body_catalog_match(&b))return 4;
    b.group_count=18;b.vertex_count=10;
    if(fitd_body_catalog_match(&b))return 5;
    puts("FITD Wii Test07.1 body catalog filters PASS");
    return 0;
}
