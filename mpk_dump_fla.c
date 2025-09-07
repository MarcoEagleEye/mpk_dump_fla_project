// mpk_dump_fla.c — Dump 4×32 KiB Controller-Pak-Bänke in FlashRAM (.fla = 128 KiB)
// Universell: nutzt Joybus-API, wenn verfügbar, sonst mempak_* Fallback.

#include <libdragon.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ===== API-Erkennung ===== */
#ifdef __has_include
#  if __has_include(<joybus.h>)
#    include <joybus.h>
#    define USE_JOYBUS 1
#  endif
#  if __has_include(<controller.h>)
#    include <controller.h>
#  endif
#  if __has_include(<flashram.h>)
#    include <flashram.h>
#  endif
#endif
#ifndef USE_JOYBUS
#  define USE_JOYBUS 0
#endif

/* Fallback-Defines für Joybus-Constants */
#if USE_JOYBUS
#ifndef JOYBUS_ACCESSORY_ADDR_LABEL
#define JOYBUS_ACCESSORY_ADDR_LABEL 0x0000
#endif
#ifndef JOYBUS_ACCESSORY_ADDR_PROBE
#define JOYBUS_ACCESSORY_ADDR_PROBE 0x8000
#endif
#ifndef JOYBUS_ACCESSORY_IO_STATUS_OK
#define JOYBUS_ACCESSORY_IO_STATUS_OK 0
#endif
#endif

/* ===== Konstanten ===== */
#define BANKS        4
#define PAGES        128            /* 128 × 256 B = 32 KiB pro Bank */
#define PAGE_SIZE    256
#define BLK_SIZE     32

/* ===== Buffer ===== */
static uint8_t dumpbuf[BANKS * PAGES * PAGE_SIZE] __attribute__((aligned(64)));

/* ===== Logger (vermeidet Konflikt mit math.h:logf) ===== */
static void dbgln(const char *fmt, ...) {
    char s[128];
    va_list ap; va_start(ap, fmt); vsnprintf(s, sizeof(s), fmt, ap); va_end(ap);
    debugf("%s\n", s);
}

/* ===== Lowlevel R/W ===== */
#if USE_JOYBUS

/* Bank 0..3 wählen: 32B-Write an 0x8000, Byte 0 = Bank */
static int bank_select(int port, int bank) {
    uint8_t blk[BLK_SIZE] = {0};
    blk[0] = (uint8_t)(bank & 0x03);
    int st = joybus_accessory_write(port, JOYBUS_ACCESSORY_ADDR_PROBE, blk);
    return (st == JOYBUS_ACCESSORY_IO_STATUS_OK) ? 0 : -1;
}

/* 256-Byte-Page lesen (8 × 32B) */
static int read_page256(int port, uint16_t base_addr, uint8_t *dst256) {
    for (int i = 0; i < 8; i++) {
        int st = joybus_accessory_read(port, (uint16_t)(base_addr + i*BLK_SIZE),
                                       dst256 + i*BLK_SIZE);
        if (st != JOYBUS_ACCESSORY_IO_STATUS_OK) return -1;
    }
    return 0;
}

static int pak_present(int port) {
    uint8_t tmp[BLK_SIZE];
    return joybus_accessory_read(port, JOYBUS_ACCESSORY_ADDR_LABEL, tmp) == JOYBUS_ACCESSORY_IO_STATUS_OK;
}

#else /* ===== mempak_* Fallback ===== */

static int bank_select(int port, int bank) {
    uint8_t blk[BLK_SIZE] = {0};
    blk[0] = (uint8_t)(bank & 0x03);
    /* 0x8000 = Bank-Switch-Adresse */
    return mempak_write(port, 0x8000, blk);
}

static int read_page256(int port, uint16_t base_addr, uint8_t *dst256) {
    for (int i = 0; i < 8; i++) {
        int rc = mempak_read(port, (uint16_t)(base_addr + i*BLK_SIZE), dst256 + i*BLK_SIZE);
        if (rc) return rc;
    }
    return 0;
}

static int pak_present(int port) {
    /* mempak_init(port) liefert !=0 bei vorhandenem Pak */
    return mempak_init(port) != 0;
}

#endif /* USE_JOYBUS */

/* ===== FlashRAM Write ===== */
static void flashram_store_full_dump(const void *buf, size_t len) {
    flashram_init();
    size_t off = 0; const uint8_t *p = (const uint8_t*)buf;
    while (off < len) {
        size_t n = (len - off > 128) ? 128 : (len - off);   /* 128-Byte-Schreibblöcke */
        flashram_write(off, p + off, n);
        off += n;
    }
}

/* ===== main ===== */
int main(void) {
    debug_init_isviewer();
    timer_init();

#if USE_JOYBUS
    /* Joybus-Variante benötigt kein controller_init() */
#else
    controller_init();
#endif

    const int PORT = 0; /* Controller-Port 1 */

    if (!pak_present(PORT)) {
        dbgln("ERROR: Kein Controller-Pak an Port 1 erkannt.");
        for(;;) {}
    }

    /* Bis zu 4 Bänke lesen */
    for (int b = 0; b < BANKS; b++) {
        if (bank_select(PORT, b)) {
            dbgln("WARN: Bank %d nicht schaltbar — Rest mit 0xFF.", b);
            memset(dumpbuf + b*PAGES*PAGE_SIZE, 0xFF, (BANKS - b)*PAGES*PAGE_SIZE);
            break;
        }
        for (int s = 0; s < PAGES; s++) {
            uint16_t addr = (uint16_t)(s * PAGE_SIZE); /* 0x0000..0x7FE0 */
            if (read_page256(PORT, addr, dumpbuf + (b*PAGES + s)*PAGE_SIZE)) {
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
