/* 空き領域に置くコードのリンク設定。書き込み可能なデータ (.data/.bss) は使わない。 */
INCLUDE fr_jp_symbols.ld

ENTRY(MovePlayerOnBike_Hook)

SECTIONS
{
    . = 0x08800400;
    .text : {
        *(.text*)
        *(.rodata*)
        *(.glue_7*)
    }
    /DISCARD/ : { *(.data*) *(.bss*) *(.comment) *(.ARM.attributes) *(.note*) }
}
