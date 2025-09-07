# N64 Mempak → FlashRAM Dumper (128 KiB)

Dieses kleine N64-ROM liest bis zu **1 Mbit (128 KiB)** eines **Multi‑Bank Controller‑Paks** (z. B. Blaze/Datel 1 Mbit) aus **und speichert den Dump in die Cartridge‑FlashRAM‑Save**. Dein Flashcart (EverDrive/64drive) schreibt daraus automatisch eine **`.fla`**‑Datei.

> Ergebnisdatei: **`mpk_dump_fla.fla`** (exakt **131 072 Bytes**) – Inhalt ist **4× 32 KiB** (Bank 0..3) hintereinander.

---

## Projektinhalt

```
mpk_dump_fla_project/
├─ src/
│  └─ mpk_dump_fla.c      # ROM‑Logik
├─ Makefile               # Build mit offizieller libdragon‑Toolchain
├─ run.sh                 # ROM lokal im Emulator starten (optional)
└─ .github/workflows/
   └─ build.yml           # GitHub Actions Workflow (Cloud‑Build)
```

---

## Build in der Cloud (GitHub Actions) – kein lokales Setup nötig

1. **Neues GitHub‑Repo** anlegen (privat ist ok).  
2. Diesen Ordner **unverändert** hochladen – wichtig ist der Pfad `.github/workflows/build.yml`.  
3. Im Repo auf **Actions** → Workflow **`build-n64-rom`** wählen → **Run workflow**.  
4. Nach ~1 Minute unter **Artifacts** das Paket **`mpk_dump_fla`** öffnen und **`mpk_dump_fla.z64`** herunterladen.

> Tipp: Du kannst die drei Projektdateien auch als ZIP nehmen: `mpk_dump_fla_project_with_actions.zip` entpacken und direkt ins Repo pushen.

### Häufige Fragen (GH Actions)
- **Was ist `.yml`?** Eine Konfigurationsdatei („YAML“), die GitHub sagt, wie gebaut wird.
- **Fehler bei `flashram_*` Symbolen?** In seltenen libdragon‑Versionen heißen die Funktionen etwas anders. **Fix:** in `src/mpk_dump_fla.c` die Zeilen mit `flashram_erase`/`flashram_flush` ggf. **auskommentieren** – `flashram_init()` + `flashram_write()` genügen.

---

## ROM benutzen (EverDrive 2.5)

1. `mpk_dump_fla.z64` auf die SD des EverDrive kopieren.  
2. Im ED‑ROM‑Menü **Save‑Typ = FlashRAM (1 Mbit)** einstellen.  
   - Falls dein OS das nicht anbietet: **Rename‑Hack** → benenne das ROM testweise wie ein bekanntes FlashRAM‑Spiel (z. B. „Paper Mario.z64“).  
3. **Controller‑Pak (1 Mbit)** in **Port 1** stecken.  
4. ROM starten, kurz warten (dumped still 4 Bänke).  
5. **Reset zurück ins ED‑Menü** → das ED legt eine **`.fla` (128 KiB)** an (neben dem ROM oder im SAVE‑Ordner).

**Prüfen:** Die Datei muss **131 072 Bytes** groß sein. Wenn du stattdessen eine **`.sra` (32 KiB)** siehst, war der Save‑Typ nicht FlashRAM.

---

## Inhalt der `.fla`

- 4 Blöcke à **32 KiB** (Bank **0**, **1**, **2**, **3**) aneinandergereiht.  
- Für Tools, die `.mpk` erwarten, kannst du die Datei einfach in `.bin`/`.mpk` **umbenennen** – viele Tools lesen das formatneutral.

---

## Troubleshooting

- **Nur 32 KiB?** → Save‑Typ prüfen (muss **FlashRAM** sein).  
- **Retrode 2 liest nur 32 KiB:** Ohne Bankschalter kann die Retrode mit Standard‑FW nur Bank 0 sehen. Benutze das ROM oder eine Pak‑Hardware mit DIP/Knopf und dump’ Bank 0..3 nacheinander.  
- **Pak hat DIP‑Schalter:** Software‑Bankswitch kann ignoriert werden; dann pro Bank dumpen (Schalter stellen → 32 KiB).  
- **`flashram_*` Compilefehler:** In `mpk_dump_fla.c` die beiden optionalen Zeilen mit `flashram_erase`/`flashram_flush` auskommentieren. `flashram_init()` + `flashram_write()` reichen.

---

## Optional: Lokal bauen (später)

- Offizielle libdragon‑Toolchain installieren (`N64_INST=~/n64`).  
- Dann im Projektordner:
  ```bash
  make
  ./run.sh mpk_dump_fla.z64   # optional Emulator‑Start
  ```

---

## Lizenz

Public Domain / Unlicense. Viel Spaß beim Dumpen!
