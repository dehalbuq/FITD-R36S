/* Wii Test07 numeric animation ID overlay.
 * The user need not identify clips from timing and log timestamps.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FITD_WII_HUD_H
#define FITD_WII_HUD_H
#include <stdbool.h>
void fitd_hud_draw(unsigned index,unsigned current,unsigned total,
                   unsigned width,unsigned height,bool autoplay);
#endif
