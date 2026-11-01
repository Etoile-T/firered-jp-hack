"""日本版 Gen3 の文字コード（主要部分のみ）。未対応のバイトは {XX} で表す。"""

HIRAGANA = ("あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをん"
            "ぁぃぅぇぉゃゅょがぎぐげござじずぜぞだぢづでどばびぶべぼぱぴぷぺぽっ")
KATAKANA = ("アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワヲン"
            "ァィゥェォャュョガギグゲゴザジズゼゾダヂヅデドバビブベボパピプペポッ")
SYMBOLS = {0x00: "　", 0xA1: "0", 0xA2: "1", 0xA3: "2", 0xA4: "3", 0xA5: "4", 0xA6: "5",
           0xA7: "6", 0xA8: "7", 0xA9: "8", 0xAA: "9", 0xAB: "！", 0xAC: "？", 0xAD: "。",
           0xAE: "ー", 0xAF: "・", 0xB0: "…", 0xFE: "\n"}

DECODE = {i + 0x01: c for i, c in enumerate(HIRAGANA)}
DECODE.update({i + 0x51: c for i, c in enumerate(KATAKANA)})
DECODE.update(SYMBOLS)
ENCODE = {c: b for b, c in DECODE.items()}


def decode(data):
    return "".join(DECODE.get(b, f"{{{b:02X}}}") for b in data)


def encode(text):
    return bytes(ENCODE[c] for c in text)
