// mpk_dump_fla.c — Dump 4×32 KiB Mempak-Bänke in FlashRAM (ergibt .fla = 128 KiB)
#include <libdragon.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define BANKS 4
#define PAGES 128          // 128 × 256 B = 32 KiB pro Bank
#define PAGE_SIZE 256
#define BLK_SIZE 32
#define ADR_BANK   0x8000  // 0x8000.. Kommandobereich (Bankselect)
#define ADR_DATA   0x0000  // 0x0000..0x7FE0 Datenfenster

static uint8_t dumpbuf[BANKS * PAGES * PAGE_SIZE] __attribute__((aligned(64)));

static void logf(const char *fmt, ...) {
    char s[128];
    va_list ap; va_start(ap, fmt); vsnprintf(s, sizeof(s), fmt, ap); va_end(ap);
    debugf("%s\n", s);
}

static int mempak_bank_select(int port, int bank) {
    uint8_t blk[BLK_SIZE] = {0};
    blk[0] = (uint8_t)(bank & 0x03);   // 0..3
    return mempak_write(port, ADR_BANK, blk);
}

static int mempak_read_256(int port, uint16_t base_addr, uint8_t *dst256) {
    // 8 × 32-Byte-Blöcke zu 256-Byte-Page zusammenbauen
    for (int i = 0; i < 8; i++) {
        int rc = mempak_read(port, base_addr + i*BLK_SIZE, dst256 + i*BLK_SIZE);
        if (rc) return rc;
    }
    return 0;
}

static void flashram_store_full_dump(const void *buf, size_t len) {
    // Nur flashram_init + flashram_write (keine optionalen Symbole -> robust im Container)
    flashram_init();

    size_t off = 0;
    const uint8_t *p = (const uint8_t*)buf;
    while (off < len) {
        size_t n = (len - off > 128) ? 128 : (len - off);   // 128-Byte-Schreibblöcke
        flashram_write(off, p + off, n);
        off += n;
    }
}

int main(void) {
    debug_init_isviewer();
    timer_init();
    controller_init();

    if (!mempak_init(0)) {
        logf("ERROR: Kein Mempak an Port 1 gefunden.");
        for(;;) {}
    }

    // Bis zu 4 Bänke lesen
    for (int b = 0; b < BANKS; b++) {
        if (mempak_bank_select(0, b)) {
            logf("WARN: Bank %d nicht schaltbar — restliche Bänke werden mit 0xFF gefüllt.", b);
            memset(dumpbuf + b*PAGES*PAGE_SIZE, 0xFF, (BANKS - b)*PAGES*PAGE_SIZE);
            break;
        }
        for (int s = 0; s < PAGES; s++) {
            uint16_t addr = ADR_DATA + s*PAGE_SIZE; // 0x0000..0x7FE0
            int rc = mempak_read_256(0, addr, dumpbuf + (b*PAGES + s)*PAGE_SIZE);
            if (rc) {
                logf("WARN: Read-Fehler in Bank %d, Seite %d (rc=%d).", b, s, rc);
                // Rest der aktuellen und folgenden Seiten/Bänke mit 0xFF auffüllen
                memset(dumpbuf + (b*PAGES + s)*PAGE_SIZE, 0xFF,
                       (BANKS*PAGES - (b*PAGES + s))*PAGE_SIZE);
                goto write_out;
            }
            if ((s & 7) == 0) logf("Bank %d: %d/%d Seiten gelesen", b, s, PAGES);
        }
    }

write_out:
    logf("Schreibe 128 KiB in FlashRAM …");
    flashram_store_full_dump(dumpbuf, sizeof(dumpbuf));
    logf("Fertig. RESET → Flashcart erstellt .fla (131072 Bytes).");
    for(;;) {}
}
