N64_INST ?= $(HOME)/n64
include $(N64_INST)/include/n64.mk

ROM_NAME := mpk_dump_fla
src := src/mpk_dump_fla.c

# Optional: smaller ROM
CFLAGS += -Os

$(eval $(call N64_ROM,${ROM_NAME},$(src)))

# Convenience targets
run: ${ROM_NAME}.z64
	@./run.sh ${ROM_NAME}.z64
