/* 日本版 FR (BPRJ) のアドレス。根拠は notes/fr_jp_bikes.md
 * Thumb 関数は -mlong-calls で bx 経由で呼ぶため +1 (Thumb ビット) を付ける。 */

/* RAM */
gPlayerAvatar = 0x02036FAC;
gObjectEvents = 0x02036D6C;

/* field_player_avatar.c */
PlayerSetAnimId              = 0x0805B8E0 + 1;
PlayerOnBikeCollide          = 0x0805BA68 + 1;
PlayerFaceDirection          = 0x0805BAB0 + 1;
PlayerJumpLedge              = 0x0805BAF8 + 1;
PlayerIdleWheelie            = 0x0805BBC8 + 1;
PlayerStartWheelie           = 0x0805BBE0 + 1;
PlayerEndWheelie             = 0x0805BBF8 + 1;
PlayerStandingHoppingWheelie = 0x0805BC10 + 1;
PlayerMovingHoppingWheelie   = 0x0805BC34 + 1;
PlayerLedgeHoppingWheelie    = 0x0805BC58 + 1;
PlayerAcroTurnJump           = 0x0805BC7C + 1;
PlayerAcroWheelieCollide     = 0x0805BCA0 + 1;
PlayerAcroPopWheelie         = 0x0805BCC4 + 1;
PlayerAcroWheelieMove        = 0x0805BCDC + 1;

/* bike.c */
FR_GetBikeTransitionId          = 0x080BE394 + 1;
FR_BikeTransition_FaceDirection = 0x080BE53C + 1;
FR_BikeTransition_TurnDirection = 0x080BE54C + 1;
FR_BikeTransition_MoveDirection = 0x080BE588 + 1;
FR_GetBikeCollision             = 0x080BE660 + 1;
FR_CanBikeFaceDirectionOnRail   = 0x080BE7B0 + 1;
FR_sBikeTransitions             = 0x083AF240;

/* sound.c */
PlaySE = 0x08071A70 + 1;
