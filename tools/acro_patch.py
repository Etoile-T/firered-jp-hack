"""段階2: ダートじてんしゃの操作（ウィリー・ジャンプ）を日本版 FR に入れる。

- tools/acro/acro_bike.c をビルドして空き領域に置き、FR の MovePlayerOnBike から飛ばす
- 自転車の絵のアニメ表を FR に残っている sAnimTable_AcroBike に切り替える
- ウィリー用のコマ (15〜32) はまだ絵が無いので、同じ向きの通常コマで代用したコマ表を作る
"""
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "tools" / "acro"
BUILD_DIR = ROOT / "build" / "acro"
DEVKITARM_BIN = Path("/opt/devkitpro/devkitARM/bin")
CC = DEVKITARM_BIN / "arm-none-eabi-gcc"
OBJCOPY = DEVKITARM_BIN / "arm-none-eabi-objcopy"
NM = DEVKITARM_BIN / "arm-none-eabi-nm"

ROM_BASE = 0x08000000
HOOK_ENTRY = "MovePlayerOnBike_Hook"

# MovePlayerOnBike (0x080BE35C) の先頭を「ldr r3,[pc,#0]; bx r3; .word 飛び先」に書き換える
MOVE_PLAYER_ON_BIKE = 0x0BE35C
MOVE_PLAYER_ON_BIKE_ORIGINAL = bytes.fromhex("10b581b06b461870")
LDR_R3_PC0_BX_R3 = bytes.fromhex("004b1847")

# 空き領域（link.ld の配置先と一致させる）
FREE_ACRO_CODE = 0x800400
FREE_ACRO_CODE_END = 0x801000
FREE_PIC_TABLE_RED = 0x801000
FREE_PIC_TABLE_GREEN = 0x801200
FREE_PIC_TABLE_END = 0x801400

# ObjectEventGraphicsInfo (OBJ_EVENT_GFX_RED_BIKE / GREEN_BIKE)
GFX_INFO_RED_BIKE = 0x367C5C
GFX_INFO_GREEN_BIKE = 0x368610
GFX_INFO_ANIMS = 0x18
GFX_INFO_IMAGES = 0x1C
ANIM_TABLE_STANDARD = 0x083673F0
ANIM_TABLE_ACRO_BIKE = 0x0836756C
PIC_TABLE_RED_BIKE = 0x083641C8
PIC_TABLE_GREEN_BIKE = 0x083642B0

SPRITE_FRAME_IMAGE_SIZE = 8
BIKE_FRAME_COUNT = 9
# sAnimTable_AcroBike が使うコマ 0〜32 → 既存の 9 コマへの割り当て（絵ができたら差し替える）
#   0 南, 1 北, 2 西, 3-4 南歩き, 5-6 北歩き, 7-8 西歩き
ACRO_FRAME_PLACEHOLDER = (
    list(range(BIKE_FRAME_COUNT))  # 0-8  通常コマ
    + [0] * 6                      # 9-14 未使用
    + [0] * 4                      # 15-18 南向きウィリー
    + [1] * 4                      # 19-22 北向きウィリー
    + [2] * 4                      # 23-26 西向きウィリー
    + [3, 4, 5, 6, 7, 8]           # 27-32 ウィリー走行（南・北・西）
)


def build():
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    obj = BUILD_DIR / "acro_bike.o"
    elf = BUILD_DIR / "acro_bike.elf"
    binf = BUILD_DIR / "acro_bike.bin"
    flags = ["-mthumb", "-mcpu=arm7tdmi", "-mthumb-interwork", "-mlong-calls", "-O2",
             "-Wall", "-Wextra", "-Wno-unused-parameter", "-Werror",
             "-ffreestanding", "-fno-builtin", "-nostdlib"]
    subprocess.run([CC, *flags, "-c", SRC_DIR / "acro_bike.c", "-o", obj], check=True)
    subprocess.run([CC, "-mthumb", "-mcpu=arm7tdmi", "-nostdlib", "-T", "link.ld",
                    f"-Wl,-Map={BUILD_DIR / 'acro_bike.map'}", obj, "-o", elf],
                   check=True, cwd=SRC_DIR)
    subprocess.run([OBJCOPY, "-O", "binary", elf, binf], check=True)
    return binf.read_bytes(), symbol_address(elf, HOOK_ENTRY)


def symbol_address(elf, name):
    out = subprocess.run([NM, elf], check=True, capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == name:
            return int(parts[0], 16)
    raise SystemExit(f"シンボル {name} が見つかりません")


def patch_acro_code(rom, expect, write):
    code, entry = build()
    if FREE_ACRO_CODE + len(code) > FREE_ACRO_CODE_END:
        raise SystemExit(f"ダートのコードが確保領域を超えました ({len(code):#x} バイト)")
    expect(rom, MOVE_PLAYER_ON_BIKE, MOVE_PLAYER_ON_BIKE_ORIGINAL, "MovePlayerOnBike の先頭")
    write(rom, FREE_ACRO_CODE, code)
    write(rom, MOVE_PLAYER_ON_BIKE, LDR_R3_PC0_BX_R3 + struct.pack("<I", entry | 1))


def placeholder_pic_table(rom, original_table):
    off = original_table - ROM_BASE
    frames = [rom[off + i * SPRITE_FRAME_IMAGE_SIZE:off + (i + 1) * SPRITE_FRAME_IMAGE_SIZE]
              for i in range(BIKE_FRAME_COUNT)]
    return b"".join(bytes(frames[src]) for src in ACRO_FRAME_PLACEHOLDER)


def patch_bike_gfx(rom, expect, write, info, original_pics, new_pics):
    expect(rom, info + GFX_INFO_ANIMS, struct.pack("<II", ANIM_TABLE_STANDARD, original_pics),
           "自転車グラフィック情報")
    table = placeholder_pic_table(rom, original_pics)
    write(rom, new_pics, table)
    write(rom, info + GFX_INFO_ANIMS, struct.pack("<II", ANIM_TABLE_ACRO_BIKE, ROM_BASE + new_pics))


def apply(rom, expect, write):
    expect(rom, FREE_ACRO_CODE, b"\xff" * (FREE_PIC_TABLE_END - FREE_ACRO_CODE), "ダート用の空き領域")
    patch_acro_code(rom, expect, write)
    patch_bike_gfx(rom, expect, write, GFX_INFO_RED_BIKE, PIC_TABLE_RED_BIKE, FREE_PIC_TABLE_RED)
    patch_bike_gfx(rom, expect, write, GFX_INFO_GREEN_BIKE, PIC_TABLE_GREEN_BIKE, FREE_PIC_TABLE_GREEN)
