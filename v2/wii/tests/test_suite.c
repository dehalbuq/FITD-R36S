/* Deterministic candidate ranking regression - no commercial assets. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../source/fitd_suite.h"
int main(void){
    FitdSuiteReport s={0};
    fitd_suite_rank(&s,10,12.f);
    fitd_suite_rank(&s,11,6.f);
    fitd_suite_rank(&s,12,24.f);
    fitd_suite_rank(&s,13,-2.f);
    fitd_suite_rank(&s,14,NAN);
    if(s.top_count!=3||s.top_index[0]!=12||
       s.top_index[1]!=10||s.top_index[2]!=11)return 1;
    for(unsigned i=20;i<40;i++)
        fitd_suite_rank(&s,i,(float)(i-19));
    if(s.top_count!=FITD_SUITE_TOP || s.top_index[0]!=12||
       s.top_index[1]!=39 || s.top_index[2]!=38 ||
       s.top_score[0]<s.top_score[1]){
        puts("FAIL shortlist ranking");
        return 1;
    }
    puts("FITD Wii Test07 batch suite shortlist ranking: PASS");
    return 0;
}
