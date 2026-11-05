# Native Wii GX boot.dol bootstrap, not the complete game.
# Requires devkitPPC, libogc and libfat from devkitPro/wii-dev.
.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error DEVKITPPC is missing; use devkitpro/devkitppc Docker image)
endif
include $(DEVKITPPC)/wii_rules

TARGET := fitd-wii-gx
CFLAGS := -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing \
          $(MACHDEP) -I$(LIBOGC_INC) -Isource
LDFLAGS := $(MACHDEP) -Wl,-Map,$(TARGET).map -L$(LIBOGC_LIB)
LDLIBS := -lfat -lwiiuse -lbte -logc -lm

.PHONY: all clean
all: boot.dol

$(TARGET).elf: source/main.c source/fitd_endian.h source/fitd_pak.h source/fitd_pak.c source/fitd_unpack.c source/fitd_camera.h source/fitd_camera.c source/fitd_body.h source/fitd_body.c source/fitd_anim.h source/fitd_anim.c source/fitd_stage.h source/fitd_stage.c source/fitd_mask_gx.h source/fitd_mask_gx.c source/fitd_scene.h source/fitd_scene.c source/fitd_scene_gx.h source/fitd_scene_gx.c source/fitd_motion.h source/fitd_motion.c source/fitd_suite.h source/fitd_suite.c source/fitd_hud.h source/fitd_hud.c
	$(CC) $(CFLAGS) source/main.c source/fitd_pak.c source/fitd_unpack.c source/fitd_camera.c source/fitd_body.c source/fitd_anim.c source/fitd_stage.c source/fitd_mask_gx.c source/fitd_scene.c source/fitd_scene_gx.c source/fitd_motion.c source/fitd_suite.c source/fitd_hud.c -o $@ $(LDFLAGS) $(LDLIBS)

boot.dol: $(TARGET).elf
	elf2dol $(TARGET).elf boot.dol

clean:
	rm -f $(TARGET).elf $(TARGET).map boot.dol
