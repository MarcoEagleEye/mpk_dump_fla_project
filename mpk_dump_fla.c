// mpk_dump_fla.c — Dump 4×32 KiB Controller-Pak-Bänke in FlashRAM (.fla = 128 KiB)
#include <libdragon.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define BANKS        4
#define PAGES        128            // 128 × 256 B = 32 KiB pro Bank
#define PAGE_SIZE    256
#define BLK_SIZE     32
#define ADR_BANKSEL  0x8000         // Bank-Switch per 32B-Write

static uint8_t dumpbuf[BANKS * PAGES * PAGE_SIZE] __attribute__((aligned(64)));

static void dbgln(const char *fmt, ...) {
    char s[128];
    va_list ap; va_start(ap, fmt); vsnprintf(s, sizeof(s), fmt, ap); va_end(ap);
    debugf("%s\n", s);
}

static int bank_select(int port, int bank) {
    uint8_t blk[BLK_SIZE] = {0};
    blk[0] = (uint8_t)(bank & 0x03); // 0..3
    // 32-Byte-Write an 0x8000 schaltet die Bank (Joybus Accessory Write)
    joypad_accessory_error_t err =
        joypad_accessory_xfer(port, JOYPAD_ACCESSORY_XFER_WRITE, ADR_BANKSEL, blk, sizeof(blk));
    return (err == JOYPAD_ACCESSORY_ERROR_NONE) ? 0 : -1;
}

static int read_page256(int port, uint16_t base_addr, uint8_t *dst256) {
    // 256-Byte-Read (8×32B) — Joypad Accessory Read erlaubt 256B am Stück
    joypad_accessory_error_t err =
        joypad_accessory_xfer(port, JOYPAD_ACCESSORY_XFER_READ, base_addr, dst256, PAGE_SIZE);
    return (err == JOYPAD_ACCESSORY_ERROR_NONE) ? 0 : -1;
}

static void flashram_store_full_dump(const void *buf, size_t len) {
    // robust: nur init + write, keine optionalen Symbole
    flashram_init();
    size_t off = 0; const uint8_t *p = (const uint8_t*)buf;
    while (off < len) {
        size_t n = (len - off > 128) ? 128 : (len - off); // 128-Byte-Schreibblöcke
        flashram_write(off, p + off, n);
        off += n;
    }
}

int main(void) {
    debug_init_isviewer();
    timer_init();

    // Joypad-Subsystem initialisieren & Zubehör checken
    joypad_init();
    if (!joypad_is_connected(JOYPAD_PORT_1)) {
        dbgln("ERROR: Kein Controller an Port 1.");
        for(;;) {}
    }
    if (joypad_get_accessory_type(JOYPAD_PORT_1) != JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK) {
        dbgln("ERROR: Kein Controller-Pak an Port 1.");
        for(;;) {}
    }

    // Bis zu 4 Bänke lesen
    for (int b = 0; b < BANKS; b++) {
        if (bank_select(JOYPAD_PORT_1, b)) {
            dbgln("WARN: Bank %d nicht schaltbar — rest mit 0xFF.", b);
            memset(dumpbuf + b*PAGES*PAGE_SIZE, 0xFF, (BANKS - b)*PAGES*PAGE_SIZE);
            break;
        }
        for (int s = 0; s < PAGES; s++) {
            uint16_t addr = (uint16_t)(s * PAGE_SIZE); // 0x0000..0x7FE0
            if (read_page256(JOYPAD_PORT_1, addr, dumpbuf + (b*PAGES + s)*PAGE_SIZE)) {
                dbgln("WARN: Read-Fehler in Bank %d, Seite %d.", b, s);
                memset(dumpbuf + (b*PAGES + s)*PAGE_SIZE, 0xFF,
                       (BANKS*PAGES - (b*PAGES + s))*PAGE_SIZE);
                goto write_out;
            }
            if ((s & 7) == 0) dbgln("Bank %d: %d/%d Seiten gelesen", b, s, PAGES);
        }
    }

write_out:
    dbgln("Schreibe 128 KiB in FlashRAM …");
    flashram_store_full_dump(dumpbuf, sizeof(dumpbuf));
    dbgln("Fertig. RESET → Flashcart erstellt .fla (131072 Bytes).");
    for(;;) {}
}
