# ===== Libdragon Makefile (robust) =====
# Findet C-Quellen in ./ und ./src und baut mpk_dump_fla.z64

N64_INST ?= $(HOME)/n64
include $(N64_INST)/include/n64.mk

ROM_NAME := mpk_dump_fla

# Quellen: Repo-Root und src/
SRC_DIRS := . src
src := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.c))

# Compiler-Flags
CFLAGS += -Os -std=gnu11 -Wno-deprecated-declarations

# ROM bauen (Makro aus n64.mk)
$(eval $(call N64_ROM,$(ROM_NAME),$(src)))

# Komfort: Emulator-Start (nur lokal)
.PHONY: run
run: $(ROM_NAME).z64
	@./run.sh $(ROM_NAME).z64

# Aufräumen
.PHONY: clean
clean:
	$(RM) -f $(ROM_NAME).z64 $(ROM_NAME).elf $(ROM_NAME).bin $(patsubst %.c,%.o,$(src))
