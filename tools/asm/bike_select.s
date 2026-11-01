@ ItemUseOnFieldCB_Bicycle (JP FR 0x080A25D8) の置き換え。
@ 元は GetOnOffBike(MACH | ACRO) 固定だったものを、
@ 道具の secondaryId で マッハ(0)=MACH_BIKE / ダート(1)=ACRO_BIKE に分ける。
@ void Callback(u8 taskId)

    .thumb
    .syntax unified

    .equ PLAYER_AVATAR_FLAG_MACH_BIKE, 2
    .equ PLAYER_AVATAR_FLAG_ACRO_BIKE, 4
    .equ SE_BIKE_BELL, 11
    .equ BIKE_SECONDARY_ACRO, 1

    .equ gSpecialVar_ItemId,                              0x0203ACA8
    .equ TestPlayerAvatarFlags,                           0x0805C008
    .equ PlaySE,                                          0x08071A70
    .equ ItemId_GetSecondaryId,                           0x0809A4A4
    .equ GetOnOffBike,                                    0x080BE888
    .equ ClearPlayerHeldMovementAndUnfreezeObjectEvents,  0x08068F80
    .equ UnlockPlayerFieldControls,                       0x0806920C
    .equ DestroyTask,                                     0x08076CA0

    .global ItemUseOnFieldCB_SelectBike
ItemUseOnFieldCB_SelectBike:
    push    {r4, lr}
    lsls    r4, r0, #24
    lsrs    r4, r4, #24

    @ 乗っていなければベルを鳴らす
    movs    r0, #(PLAYER_AVATAR_FLAG_MACH_BIKE | PLAYER_AVATAR_FLAG_ACRO_BIKE)
    ldr     r3, =TestPlayerAvatarFlags + 1
    bl      call_r3
    cmp     r0, #0
    bne     choose_bike
    movs    r0, #SE_BIKE_BELL
    ldr     r3, =PlaySE + 1
    bl      call_r3

choose_bike:
    ldr     r0, =gSpecialVar_ItemId
    ldrh    r0, [r0]
    ldr     r3, =ItemId_GetSecondaryId + 1
    bl      call_r3
    movs    r1, #PLAYER_AVATAR_FLAG_MACH_BIKE
    cmp     r0, #BIKE_SECONDARY_ACRO
    bne     get_on_off
    movs    r1, #PLAYER_AVATAR_FLAG_ACRO_BIKE
get_on_off:
    movs    r0, r1
    ldr     r3, =GetOnOffBike + 1
    bl      call_r3

    ldr     r3, =ClearPlayerHeldMovementAndUnfreezeObjectEvents + 1
    bl      call_r3
    ldr     r3, =UnlockPlayerFieldControls + 1
    bl      call_r3
    movs    r0, r4
    ldr     r3, =DestroyTask + 1
    bl      call_r3

    pop     {r4}
    pop     {r0}
    bx      r0

call_r3:
    bx      r3

    .pool
