"""ROM 内の Thumb 関数の開始位置を推定し、C ソースの関数順と並べて表示する。

使い方:
  python3 tools/funcscan.py <rom.gba> <開始addr> <終了addr> [ソース.c]

開始位置の候補:
  - ROM 全体の BL 命令の飛び先
  - ROM 内の Thumb 関数ポインタ (奇数アドレス) の指す先
  - 直前の命令が関数終端 (bx / pop {pc}) で、自身が push {.., lr}
ソースを渡すと、ソース上の関数定義の順番と並べて出す（同じ翻訳単位は ROM 上も同じ順）。
"""
import re
import struct
import sys
from functools import lru_cache

ROM_BASE = 0x08000000


def load(path):
    return open(path, "rb").read()


@lru_cache(maxsize=None)
def bl_targets(path):
    rom = load(path)
    targets = {}
    hw = struct.unpack_from(f"<{len(rom) // 2}H", rom)
    for i in range(len(hw) - 1):
        hi, lo = hw[i], hw[i + 1]
        if (hi & 0xF800) == 0xF000 and (lo & 0xF800) == 0xF800:
            off = ((hi & 0x7FF) << 12) | ((lo & 0x7FF) << 1)
            if off & 0x400000:
                off -= 0x800000
            tgt = ROM_BASE + i * 2 + 4 + off
            targets[tgt] = targets.get(tgt, 0) + 1
    return targets


@lru_cache(maxsize=None)
def thumb_pointers(path):
    rom = load(path)
    ptrs = {}
    for i in range(0, len(rom) - 3, 4):
        v, = struct.unpack_from("<I", rom, i)
        if v & 1 and ROM_BASE <= v < ROM_BASE + len(rom):
            ptrs[v - 1] = ptrs.get(v - 1, 0) + 1
    return ptrs


def is_push_lr(h):
    return (h & 0xFF00) == 0xB500


def is_return(h):
    return (h & 0xFF87) == 0x4700 or (h & 0xFF00) == 0xBD00  # bx rX / pop {.., pc}


def scan(path, start, end):
    rom = load(path)
    bls = bl_targets(path)
    ptrs = thumb_pointers(path)
    found = {}
    for a in range(start, end, 2):
        h, = struct.unpack_from("<H", rom, a - ROM_BASE)
        tags = []
        if a in bls:
            tags.append(f"bl×{bls[a]}")
        if a in ptrs:
            tags.append(f"ptr×{ptrs[a]}")
        if is_push_lr(h):
            prev, = struct.unpack_from("<H", rom, a - 2 - ROM_BASE)
            prev2, = struct.unpack_from("<H", rom, a - 4 - ROM_BASE)
            if is_return(prev) or is_return(prev2) or prev == 0:
                tags.append("push")
        if tags:
            found[a] = tags
    return found


FUNC_DEF = re.compile(r"^(?:static\s+)?(?:inline\s+)?[A-Za-z_][\w\s\*]*?\b(\w+)\s*\([^;]*$")


def source_functions(path):
    names = []
    lines = open(path, encoding="utf-8", errors="replace").read().split("\n")
    for i, line in enumerate(lines):
        m = FUNC_DEF.match(line)
        if not m or line.startswith((" ", "\t", "#")):
            continue
        nxt = lines[i + 1].strip() if i + 1 < len(lines) else ""
        if line.rstrip().endswith("{") or nxt.startswith("{"):
            names.append(m.group(1))
    return names


def main():
    path = sys.argv[1]
    start, end = int(sys.argv[2], 16), int(sys.argv[3], 16)
    found = scan(path, start, end)
    for a, tags in found.items():
        print(f"{a:#010x}  {' '.join(tags)}")
    if len(sys.argv) > 4:
        print("\n# ソース上の関数順")
        for i, n in enumerate(source_functions(sys.argv[4])):
            print(f"{i:3d} {n}")


if __name__ == "__main__":
    main()
