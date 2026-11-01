# 日本版 FR (BPRJ) 自転車まわり調査メモ

対象: `base/Pocket Monsters - FireRed (Japan).gba` (ゲームコード BPRJ)
参照ソース: `ref/pokefirered`, `ref/pokeemerald` (英語版基準の decomp。アドレスは日本版と異なる)

## アドレス（日本版 FR で確認済み）

| 名前 | アドレス | 確認方法 |
|---|---|---|
| 道具テーブル `gItems` | 0x083A06F8 (1件 40 バイト, 375件) | 名前「マッハじてんしゃ」から逆算 |
| `FieldUseFunc_Bike` | 0x080A2500 | じてんしゃの fieldUseFunc |
| `ItemUseOnFieldCB_Bicycle` | 0x080A25D8 | 上記から参照 (ポインタ @0x080A25B4) |
| `sItemUseOnFieldCB` | 0x02039910 | |
| `GetOnOffBike` | 0x080BE888 | |
| `TestPlayerAvatarFlags` | 0x0805C008 | |
| `PlaySE` | 0x08071A70 | |
| `ItemId_GetSecondaryId` | 0x0809A4A4 | |
| `ClearPlayerHeldMovementAndUnfreezeObjectEvents` | 0x08068F80 | |
| `UnlockPlayerFieldControls` | 0x0806920C | |
| `DestroyTask` | 0x08076CA0 | |
| `StartFishing` | 0x0805CB7C | |
| `gPlayerAvatar` | 0x02036FAC | |
| `gSpecialVar_ItemId` | 0x0203ACA8 | |
| 自転車屋 引換スクリプト (`additem ITEM_BICYCLE`) | 0x08180E92 | バイト列検索 |
| `NewGameInitPCItems` | 0x080EC604 | 初期PC道具テーブルの参照元 |
| `gNewGamePCItems` (キズぐすり×1) | 0x083C83F8 (ポインタ @0x080EC654) | |
| `AddPCItem` | 0x08099DD0 | |

### ダート移植で使うアドレス（段階2、中身を逆アセンブルして確認済み）
シンボル定義は `tools/acro/fr_jp_symbols.ld` が正。主なもの:

| 名前 | アドレス | 備考 |
|---|---|---|
| `gObjectEvents` | 0x02036D6C | 1件 0x24 バイト |
| `MovePlayerOnBike` | 0x080BE35C | 先頭をフックで書き換え |
| `GetBikeTransitionId` / `sBikeTransitions` / `sBikeInputHandlers` | 0x080BE394 / 0x083AF240 / 0x083AF254 | |
| `BikeTransition_Face/Turn/MoveDirection` | 0x080BE53C / 0x080BE54C / 0x080BE588 | |
| `GetBikeCollision` / `CanBikeFaceDirectionOnRail` | 0x080BE660 / 0x080BE7B0 | |
| `PlayerSetAnimId` | 0x0805B8E0 | |
| `PlayerIdleWheelie` 〜 `PlayerAcroWheelieMove` | 0x0805BBC8 〜 0x0805BCDC | 未使用のまま残存。並び順推定は 1 つずれていたので中身で確定 |
| `gObjectEventGraphicsInfoPointers` | 0x08363E38 | |
| 自転車グラフィック情報 レッド / リーフ | 0x08367C5C / 0x08368610 | anims +0x18, images +0x1C |
| `sAnimTable_Standard` / `sAnimTable_AcroBike` | 0x083673F0 / 0x0836756C | AcroBike も ROM に残存 |
| `sPicTable_RedBike` / `GreenBike` | 0x083641C8 / 0x083642B0 | 9 コマ |

FR の `PlayerAvatar` には `abStartSelectTimerHistory` が無い → `dirTimerHistory[8]` を前半/後半に分けて流用。
FR には「走りながらウィリー終了」の移動アクションが無い。でこぼこ坂もカントーに無いので省略。

## 日本版の道具エントリ構造 (40 バイト)
英語版 (44 バイト) との違いは名前欄が 10 バイトであること。
`name[10] / id u16 / price u16 / holdEffect u8 / param u8 / desc* / importance u8 / exitsBag u8 / pocket u8 / type u8 / fieldUseFunc* / battleUsage u8 (+3 pad) / battleUseFunc* / secondaryId u8 (+3 pad)`

## 道具の現状
- 0x103 マッハじてんしゃ / 0x110 ダートじてんしゃ は名前・説明・使用関数ともに残存（入手手段なし）
- secondaryId: マッハ=0, ダート=1, じてんしゃ(0x168)=0
- 全件ダンプ: `data/items_FireRed_JP.csv`, `data/items_Emerald_JP.csv`

## FR の自転車挙動
- FR は乗るとき `MACH|ACRO` (=6) を同時に立てる 1 種類の自転車
- 移動は一定速 (`PlayerRideWaterCurrent`)。加速なし
- `overworld.c` はワープ時に MACH(2)/ACRO(4) を個別に保持できる
- ウィリー等の動作関数・移動アクションは残存。ただし bike.c の入力ハンドラは削除済み
- 自転車スプライトは 9 フレーム（エメラルドのダート用は 27 フレーム）

## 空き領域
- 0x6C7D40〜0xC00000 付近が 0xFF 埋め
- 使用中: 0x800000 (自転車コールバック), 0x800100 (自転車屋追加スクリプト), 0x800200 (初期PC道具テーブル),
  0x800400-0x801000 (ダート操作コード), 0x801000 / 0x801200 (ダート用コマ表 レッド / リーフ)

## ツール
- `tools/funcscan.py`: 指定範囲の関数開始候補を列挙し、C ソースの関数順と並べる（推定のみ。必ず中身で確認する）
- ビルドには devkitARM の gcc を使う（`/opt/devkitpro/devkitARM/bin`）
- pokefirered を agbcc でビルドして関数を照合する案は、自動許可で止められたため未実施

## パッチ
`python3 tools/patch_bikes.py` → `work/FR_bikes.gba`
