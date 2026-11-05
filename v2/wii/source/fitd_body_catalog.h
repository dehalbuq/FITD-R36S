/* AITD1 body catalog: find other articulated player-like models instead
 * of hardcoding BODY 15 (which can include rifle geometry).
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_BODY_CATALOG_H
#define FITD_WII_BODY_CATALOG_H
#include "fitd_body.h"
#define FITD_BODY_CATALOG_CAP 64u
typedef struct {
    unsigned index;
    unsigned vertices,groups,primitives;
} FitdBodyCandidate;
static inline bool fitd_body_catalog_match(const FitdBody *b){
    return b && b->valid &&
        (b->group_count==18u || b->group_count==20u) &&
        b->vertex_count>=100u && b->vertex_count<=500u &&
        b->primitive_count>=60u;
}
unsigned fitd_body_catalog_scan(const char *directory,
    FitdBodyCandidate *out,unsigned capacity,FILE *log);
#endif
