# firered-jp-hack

日本版『ポケットモンスター ファイアレッド』(BPRJ) を直接パッチして、エメラルドの自転車要素を移すための ROM ハック用ツール群です。

**ROM は含みません。** 自分で吸い出した ROM を用意してください。

## できること

- 「じてんしゃ」を「マッハじてんしゃ」に改名（挙動は FR 標準の自転車）
- 「ダートじてんしゃ」を入手可能にする（初期 PC、ハナダの自転車屋）
- ダートじてんしゃのウィリー・その場ジャンプ・横跳び・振り向きジャンプ（エメラルドの `bike.c` から移植）

ウィリーの絵はまだ無いため、ウィリー中も通常の自転車のコマで表示されます。

## 必要なもの

- 日本版ファイアレッドの ROM（ゲームコード `BPRJ`）
- Python 3
- [devkitARM](https://devkitpro.org/)（`/opt/devkitpro/devkitARM/bin` にある前提）

## 使い方

```sh
# ROM を置く
mkdir -p base
cp "/path/to/Pocket Monsters - FireRed (Japan).gba" base/

# パッチを当てる → work/FR_bikes.gba ができる
python3 tools/patch_bikes.py
```

入力・出力を指定する場合は `python3 tools/patch_bikes.py <入力ROM> <出力ROM>`。
想定と違う ROM には当たらないよう、書き換える前に元のバイト列を確認して止まります。

## 構成

| パス | 内容 |
|---|---|
| `tools/patch_bikes.py` | パッチ本体（段階1: 改名・入手・乗り分け） |
| `tools/acro_patch.py` | 段階2: ダートの操作コードをビルドして組み込む |
| `tools/acro/` | ダート操作の C コード、日本版アドレス定義、リンク設定 |
| `tools/asm/` | 小さなアセンブリパッチ |
| `tools/dump_items.py` | 道具テーブルを CSV に書き出す（`data/` へ。公開リポジトリには含めない） |
| `tools/funcscan.py` | ROM 内の関数開始位置を推定し、C ソースの関数順と並べる |
| `notes/fr_jp_bikes.md` | 調査メモ（日本版のアドレス、構造体、空き領域の使用状況） |

## 参照ソース

関数や構造体の解析に [pret/pokefirered](https://github.com/pret/pokefirered) と [pret/pokeemerald](https://github.com/pret/pokeemerald) を使っています。`ref/` に取得してください（リポジトリには含めません）。

```sh
git clone --depth 1 https://github.com/pret/pokefirered.git ref/pokefirered
git clone --depth 1 https://github.com/pret/pokeemerald.git ref/pokeemerald
```

decomp は英語版が基準のため、日本版のアドレスは逆アセンブルで個別に特定しています（`notes/fr_jp_bikes.md`）。
