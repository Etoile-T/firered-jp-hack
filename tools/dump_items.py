"""日本版 Gen3 ROM の道具テーブルを CSV に書き出す。

使い方: python3 -I tools/dump_items.py <rom.gba> <out.csv>
テーブル位置は「マッハじてんしゃ」(item 0x103) の名前から逆算する。
"""
import csv
import struct
import sys

from jpcharset import decode, encode

ENTRY_SIZE = 40
NAME_LEN = 10
ITEM_MACH_BIKE = 0x103
ITEM_COUNT = 0x177  # FR/エメラルド共通 (ITEMS_COUNT)
ROM_BASE = 0x08000000


def find_table(rom):
    name = encode("マッハじてんしゃ") + b"\xff"
    pos = rom.find(name)
    while pos != -1:
        idx, = struct.unpack_from("<H", rom, pos + NAME_LEN)
        if idx == ITEM_MACH_BIKE:
            return pos - ITEM_MACH_BIKE * ENTRY_SIZE
        pos = rom.find(name, pos + 1)
    raise SystemExit("道具テーブルが見つかりません")


def read_string(rom, ptr, limit=200):
    off = ptr - ROM_BASE
    if not 0 <= off < len(rom):
        return ""
    end = rom.find(b"\xff", off, off + limit)
    return decode(rom[off:end if end != -1 else off + limit])


def parse_entry(rom, off):
    e = rom[off:off + ENTRY_SIZE]
    idx, price = struct.unpack_from("<HH", e, 10)
    desc, = struct.unpack_from("<I", e, 16)
    field_fn, = struct.unpack_from("<I", e, 24)
    battle_fn, = struct.unpack_from("<I", e, 32)
    return {
        "offset": hex(off), "index": hex(idx), "name": decode(e[:NAME_LEN].split(b"\xff")[0]),
        "price": price, "holdEffect": e[14], "holdParam": e[15],
        "descPtr": hex(desc), "importance": e[20], "exitsBag": e[21], "pocket": e[22],
        "type": e[23], "fieldUseFunc": hex(field_fn), "battleUsage": e[28],
        "battleUseFunc": hex(battle_fn), "secondaryId": e[36],
        "description": read_string(rom, desc).replace("\n", "⏎"),
    }


def main():
    rom = open(sys.argv[1], "rb").read()
    base = find_table(rom)
    rows = [parse_entry(rom, base + i * ENTRY_SIZE) for i in range(ITEM_COUNT)]
    with open(sys.argv[2], "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=rows[0].keys())
        w.writeheader()
        w.writerows(rows)
    print(f"table={hex(base)} items={len(rows)} -> {sys.argv[2]}")


if __name__ == "__main__":
    main()
