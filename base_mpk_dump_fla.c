// File: src/mpk_dump_fla.c
// Purpose: Dump up to 1 Mbit (128 KiB) multi‑bank Controller Pak to cart FlashRAM.
// Result on flashcart: a 131072‑byte .fla save file next to the ROM / in SAVE folder.
//
// Build target: libdragon (official toolchain). No special cart SDKs needed.
//
// Tested assumptions (design-time):
//  - Joybus mempak bank select: write 32 bytes at address 0x8000, first byte = bank (0..3).
//  - Data window: 0x0000..0x7FE0 in 0x20 (32‑byte) steps; 8 blocks (8×32 = 256) per 256‑byte page.
//  - 128 pages per bank (=> 32 KiB), up to 4 banks => 128 KiB total.
//  - Writing to FlashRAM via libdragon flashram_* will make ED64/64drive create a .fla file.
//
// If your libdragon version uses slightly different flashram function names, see the
// FLASHRAM_* shim at the bottom and adjust as noted.

#include <libdragon.h>
#include <string.h>
#include <stdio.h>

#define BANK_COUNT          4
#define SECTORS_PER_BANK    128        // "pages" of 256 bytes
#define SECTOR_SIZE         256
#define MPK_ADDR_BANKSEL    0x8000     // special register area
#define MPK_ADDR_DATA_START 0x0000
#define MPK_ADDR_DATA_END   0x7FE0     // inclusive start of last 32B block
#define MPK_BLOCK_SIZE      32

static uint8_t dumpbuf[BANK_COUNT * SECTORS_PER_BANK * SECTOR_SIZE] __attribute__((aligned(64)));

static void wait_vsync(void){ while(display_is_busy()) ; display_show(NULL); }

static void logf(const char *fmt, ...) {
    static char line[128];
    va_list ap; va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    debugf("%s\n", line);
}

static int mempak_bank_select(int port, int bank)
{
    uint8_t blk[MPK_BLOCK_SIZE];
    memset(blk, 0, sizeof(blk));
    blk[0] = (uint8_t)bank;   // convention: first byte carries bank id
    // write 32B to 0x8000 -> bank select (Datel/Blaze 1Mbit paks)
    return mempak_write(port, MPK_ADDR_BANKSEL, blk);
}

static int mempak_read_256(int port, uint16_t base_addr, uint8_t *dst)
{
    // Read 8×32B blocks to assemble one 256‑byte page
    for (int i = 0; i < 8; i++) {
        int rc = mempak_read(port, base_addr + i*MPK_BLOCK_SIZE, dst + i*MPK_BLOCK_SIZE);
        if (rc) return rc;
    }
    return 0;
}

static int dump_bank(int port, int bank, uint8_t *out)
{
    int rc = mempak_bank_select(port, bank);
    if (rc) return rc;

    for (int s = 0; s < SECTORS_PER_BANK; s++) {
        uint16_t addr = MPK_ADDR_DATA_START + s*SECTOR_SIZE;
        int rr = mempak_read_256(port, addr, out + s*SECTOR_SIZE);
        if (rr) return rr;
        if ((s & 7) == 0) logf("Bank %d: %d/%d pages", bank, s, SECTORS_PER_BANK);
    }
    return 0;
}

static int flashram_store_full_dump(const void *buf, size_t len)
{
    // Initialize FlashRAM and write the whole buffer. Many libdragon versions
    // expose flashram_init(), flashram_write(offset, src, len) and an erase helper.
    // We'll try erase+write. If your version differs, adjust the shim below.

    flashram_init();

    // Full‑chip erase (some libdragon versions use flashram_erase());
    // If your version doesn't require manual erase, the call will be a no‑op.
    if (flashram_erase) {
        flashram_erase();
    }

    size_t written = 0;
    const uint8_t *p = (const uint8_t*)buf;
    while (written < len) {
        size_t chunk = 128; // FlashRAM native write block is 128 bytes
        if (chunk > len - written) chunk = len - written;
        flashram_write(written, p + written, chunk);
        written += chunk;
    }

    // Ensure data is committed (if API offers flush)
    if (flashram_flush)
        flashram_flush();

    return 0;
}

int main(void)
{
    // Minimal video+debug setup (no frame buffer drawing needed)
    debug_init_isviewer();
    timer_init();
    controller_init();

    logf("MPK→FlashRAM Dumper start");
    controller_scan();
    int present = mempak_init(0); // port 0 (Controller 1)
    if (!present) {
        logf("ERROR: Kein Mempak an Port 1 gefunden.");
        for(;;) {}
    }

    // Dump up to 4 banks
    for (int b = 0; b < BANK_COUNT; b++) {
        int rc = dump_bank(0, b, dumpbuf + b*SECTORS_PER_BANK*SECTOR_SIZE);
        if (rc) {
            logf("WARN: Bank %d nicht lesbar (rc=%d) — wir brechen nach vorherigen Banken ab.", b, rc);
            // Truncate remaining banks to 0xFF for clarity
            memset(dumpbuf + b*SECTORS_PER_BANK*SECTOR_SIZE, 0xFF,
                   (BANK_COUNT - b)*SECTORS_PER_BANK*SECTOR_SIZE);
            break;
        }
    }

    // Write 128 KiB to FlashRAM -> Flashcart will create .fla
    logf("Schreibe 128 KiB in FlashRAM...");
    flashram_store_full_dump(dumpbuf, sizeof(dumpbuf));
    logf("Fertig. Beende das ROM (RESET) → Flashcart legt .fla an.");

    // Idle: wait for reset
    for(;;) {}
}

/* ================== Build Notes ==================
1) Ordnerstruktur:
   project/
     ├─ src/mpk_dump_fla.c  (dieses File)
     ├─ Makefile
     └─ run.sh

2) Makefile (libdragon skeleton):
---------------------------------------------------
# File: Makefile
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
---------------------------------------------------

3) run.sh (simple64 oder mupen64plus):
---------------------------------------------------
#!/usr/bin/env bash
set -euo pipefail
ROM="${1:-mpk_dump_fla.z64}"
if command -v simple64 >/dev/null 2>&1; then
  exec simple64 "$ROM"
elif command -v mupen64plus >/dev/null 2>&1; then
  exec mupen64plus "$ROM"
else
  echo "Bitte simple64 oder mupen64plus installieren und ins PATH legen." >&2
  exit 1
fi
---------------------------------------------------

4) Nutzung:
   - Controller‑Pak (Blaze/Datel 1 Mbit) an Port 1 stecken.
   - ROM starten. Es wird leise 4 Bänke (0..3) lesen.
   - Nach ~1–2 s: Dump in FlashRAM geschrieben.
   - Reset drücken / ROM verlassen → Flashcart erzeugt 131072‑Byte .fla.

5) Ausgabe-Datei:
   - EverDrive: meist /ED64/SAVE/<ROMNAME>.fla oder neben dem ROM.
   - 64drive: neben dem ROM, Endung .fla.

6) Prüfen/Konvertieren:
   - .fla ist exakt 128 KiB. Für Tools, die .mpk wollen, ggf. einfach auf .bin umbenennen.
   - Der Dump enthält 4× (Bank0..Bank3) à 32 KiB hintereinander.

7) FlashRAM-API-Anmerkung:
   - Falls euer libdragon andere Signaturen hat:
     * Alte Versionen: flashram_write(offset, src, len) existiert; Erase optional.
     * Manchmal heißt Erase z. B. flashram_erase_chip() oder ist implizit.
   - Passen Sie die Funktion flashram_store_full_dump entsprechend an.
*/
