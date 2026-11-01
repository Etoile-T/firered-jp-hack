"""段階1: マッハ／ダート自転車パッチ（日本版 FR / BPRJ 専用）。

- じてんしゃ(0x168) を「マッハじてんしゃ」に改名（挙動は FR 標準の自転車のまま）
- 自転車使用時のフラグを道具ごとに MACH / ACRO に分ける（tools/asm/bike_select.s）
- ハナダの自転車屋で、ひきかえけんと交換時に「ダートじてんしゃ」も渡す
- 初期 PC に マッハじてんしゃ・ダートじてんしゃ を入れる
- ダートじてんしゃのウィリー・ジャンプ操作（tools/acro_patch.py）

使い方: python3 tools/patch_bikes.py [入力ROM] [出力ROM]
"""
import hashlib
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

import acro_patch
from jpcharset import encode

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_IN = ROOT / "base" / "Pocket Monsters - FireRed (Japan).gba"
DEFAULT_OUT = ROOT / "work" / "FR_bikes.gba"
ASM_SRC = ROOT / "tools" / "asm" / "bike_select.s"

ROM_BASE = 0x08000000
GAME_CODE = b"BPRJ"

# 道具テーブル
ITEM_TABLE = 0x3A06F8
ITEM_ENTRY_SIZE = 40
ITEM_NAME_LEN = 10
ITEM_DESC_OFFSET = 16
ITEM_MACH_BIKE = 0x103
ITEM_ACRO_BIKE = 0x110
ITEM_BICYCLE = 0x168
ITEM_BIKE_VOUCHER = 0x160

# 自転車使用コールバックへのポインタ (sItemUseOnFieldCB = ItemUseOnFieldCB_Bicycle)
BIKE_CB_PTR = 0x0A25B4
BIKE_CB_ORIGINAL = 0x080A25D9

# 自転車屋: additem ITEM_BICYCLE の直後の removeitem ITEM_BIKE_VOUCHER
BIKE_SHOP_REMOVE_VOUCHER = 0x180E97

# 初期 PC 道具: NewGameInitPCItems (0x080EC604) が参照する gNewGamePCItems へのポインタ
NEW_GAME_PC_ITEMS_PTR = 0x0EC654
NEW_GAME_PC_ITEMS_ORIGINAL = 0x083C83F8
ITEM_POTION = 0x0D
NEW_GAME_PC_ITEMS = [(ITEM_POTION, 1), (ITEM_BICYCLE, 1), (ITEM_ACRO_BIKE, 1)]

# 空き領域（0xFF 埋め、0x6C7D40-0xC00000 が空いている）
FREE_CODE = 0x800000
FREE_SCRIPT = 0x800100
FREE_PC_ITEMS = 0x800200
FREE_END = 0x800300

# スクリプトコマンド
SCR_RETURN = 0x03
SCR_CALL = 0x04
SCR_CALLSTD = 0x09
SCR_SETORCOPYVAR = 0x1A
SCR_REMOVEITEM = 0x45
STD_OBTAIN_ITEM = 0
VAR_0x8000 = 0x8000
VAR_0x8001 = 0x8001


def assemble(src):
    with tempfile.TemporaryDirectory() as tmp:
        obj = Path(tmp) / "out.o"
        binf = Path(tmp) / "out.bin"
        subprocess.run(["arm-none-eabi-as", "-mthumb", "-mcpu=arm7tdmi", "-o", obj, src], check=True)
        subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", "-j", ".text", obj, binf], check=True)
        return binf.read_bytes()


def expect(rom, off, data, what):
    if bytes(rom[off:off + len(data)]) != data:
        raise SystemExit(f"想定外のデータ: {what} @ {off:#x}（別バージョンのROM、またはパッチ済み？）")


def write(rom, off, data):
    rom[off:off + len(data)] = data


def item_offset(item_id):
    return ITEM_TABLE + item_id * ITEM_ENTRY_SIZE


def patch_rename_bicycle(rom):
    name = encode("マッハじてんしゃ") + b"\xff"
    name = name.ljust(ITEM_NAME_LEN, b"\x00")
    expect(rom, item_offset(ITEM_BICYCLE), encode("じてんしゃ") + b"\xff", "じてんしゃの名前")
    write(rom, item_offset(ITEM_BICYCLE), name)
    mach_desc = rom[item_offset(ITEM_MACH_BIKE) + ITEM_DESC_OFFSET:][:4]
    write(rom, item_offset(ITEM_BICYCLE) + ITEM_DESC_OFFSET, mach_desc)


def patch_bike_callback(rom):
    code = assemble(ASM_SRC)
    if FREE_CODE + len(code) > FREE_SCRIPT:
        raise SystemExit("ASM が確保領域に収まりません")
    expect(rom, BIKE_CB_PTR, struct.pack("<I", BIKE_CB_ORIGINAL), "自転車コールバックのポインタ")
    write(rom, FREE_CODE, code)
    write(rom, BIKE_CB_PTR, struct.pack("<I", ROM_BASE + FREE_CODE + 1))


def patch_bike_shop(rom):
    remove_voucher = struct.pack("<BHH", SCR_REMOVEITEM, ITEM_BIKE_VOUCHER, 1)
    expect(rom, BIKE_SHOP_REMOVE_VOUCHER, remove_voucher, "自転車屋の removeitem")
    script = (
        remove_voucher
        + struct.pack("<BHH", SCR_SETORCOPYVAR, VAR_0x8000, ITEM_ACRO_BIKE)
        + struct.pack("<BHH", SCR_SETORCOPYVAR, VAR_0x8001, 1)
        + bytes([SCR_CALLSTD, STD_OBTAIN_ITEM, SCR_RETURN])
    )
    write(rom, FREE_SCRIPT, script)
    write(rom, BIKE_SHOP_REMOVE_VOUCHER, struct.pack("<BI", SCR_CALL, ROM_BASE + FREE_SCRIPT))


def patch_new_game_pc_items(rom):
    expect(rom, NEW_GAME_PC_ITEMS_PTR, struct.pack("<I", NEW_GAME_PC_ITEMS_ORIGINAL), "初期PC道具のポインタ")
    table = b"".join(struct.pack("<HH", item, qty) for item, qty in NEW_GAME_PC_ITEMS)
    table += struct.pack("<HH", 0, 0)
    write(rom, FREE_PC_ITEMS, table)
    write(rom, NEW_GAME_PC_ITEMS_PTR, struct.pack("<I", ROM_BASE + FREE_PC_ITEMS))


def main():
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_IN
    dst = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_OUT
    rom = bytearray(src.read_bytes())
    expect(rom, 0xAC, GAME_CODE, "ゲームコード")
    expect(rom, FREE_CODE, b"\xff" * (FREE_END - FREE_CODE), "空き領域")

    patch_rename_bicycle(rom)
    patch_bike_callback(rom)
    patch_bike_shop(rom)
    patch_new_game_pc_items(rom)
    acro_patch.apply(rom, expect, write)

    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(rom)
    print(f"{dst}  sha1={hashlib.sha1(rom).hexdigest()}")


if __name__ == "__main__":
    main()
