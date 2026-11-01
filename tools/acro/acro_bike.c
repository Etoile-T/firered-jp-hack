// エメラルドのダートじてんしゃ操作 (ref/pokeemerald/src/bike.c) を日本版 FR へ移植したもの。
// FR の MovePlayerOnBike (0x080BE35C) の先頭から MovePlayerOnBike_Hook へ飛ばして使う。
//
// エメラルドとの違い:
// - でこぼこ坂 (MetatileBehavior_IsBumpySlope) はカントーに無いので判定を省いた
// - 通常走行・向き変更は FR 側の BikeTransition_* を使う（岩の階段など FR 固有の地形に対応するため）
// - FR には「走りながらウィリーをやめる」移動アクションが無いので、その場面は通常走行にする
#include "fr_jp.h"

enum
{
    ACRO_STATE_NORMAL,
    ACRO_STATE_TURNING,
    ACRO_STATE_WHEELIE_STANDING,
    ACRO_STATE_BUNNY_HOP,
    ACRO_STATE_WHEELIE_MOVING,
    ACRO_STATE_SIDE_JUMP,
    ACRO_STATE_TURN_JUMP,
};

enum
{
    ACRO_TRANS_FACE_DIRECTION,
    ACRO_TRANS_TURN_DIRECTION,
    ACRO_TRANS_MOVING,
    ACRO_TRANS_NORMAL_TO_WHEELIE,
    ACRO_TRANS_WHEELIE_TO_NORMAL,
    ACRO_TRANS_WHEELIE_IDLE,
    ACRO_TRANS_WHEELIE_HOPPING_STANDING,
    ACRO_TRANS_WHEELIE_HOPPING_MOVING,
    ACRO_TRANS_SIDE_JUMP,
    ACRO_TRANS_TURN_JUMP,
    ACRO_TRANS_WHEELIE_MOVING,
    ACRO_TRANS_WHEELIE_RISING_MOVING,
    ACRO_TRANS_WHEELIE_LOWERING_MOVING,
};

// Turning 状態でこのフレーム数を超えたら向きを変える（1マス進むのに 6 フレーム）
#define TURN_WAIT_FRAMES 6
// ウィリーのままこのフレーム数 B を押し続けるとその場ジャンプになる
#define BUNNY_HOP_HOLD_FRAMES 40

// dirTimerHistory[8] を分割して使う（FR には abStartSelectTimerHistory が無いため）
#define HISTORY_LEN 4
#define DIR_TIMERS (&gPlayerAvatar.dirTimerHistory[0])
#define AB_TIMERS  (&gPlayerAvatar.dirTimerHistory[HISTORY_LEN])
#define TIMER_MAX 0xFF

struct BikeHistoryInputInfo
{
    u32 dirHistoryMatch;
    u32 abStartSelectHistoryMatch;
    u32 dirHistoryMask;
    u32 abStartSelectHistoryMask;
    const u8 *dirTimerHistoryList;
    const u8 *abStartSelectHistoryList;
    u32 direction;
};

static const u8 sAcroBikeJumpTimerList[] = {4, 0};

// 方向キーと B をほぼ同時に押すと、その方向へジャンプ
static const struct BikeHistoryInputInfo sAcroBikeTricksList[] =
{
    {DIR_SOUTH, B_BUTTON, 0xF, 0xF, sAcroBikeJumpTimerList, sAcroBikeJumpTimerList, DIR_SOUTH},
    {DIR_NORTH, B_BUTTON, 0xF, 0xF, sAcroBikeJumpTimerList, sAcroBikeJumpTimerList, DIR_NORTH},
    {DIR_WEST,  B_BUTTON, 0xF, 0xF, sAcroBikeJumpTimerList, sAcroBikeJumpTimerList, DIR_WEST},
    {DIR_EAST,  B_BUTTON, 0xF, 0xF, sAcroBikeJumpTimerList, sAcroBikeJumpTimerList, DIR_EAST},
};

static const u8 sJumpMovementActions[] =
{
    [DIR_NONE]  = MOVEMENT_ACTION_JUMP_DOWN,
    [DIR_SOUTH] = MOVEMENT_ACTION_JUMP_DOWN,
    [DIR_NORTH] = MOVEMENT_ACTION_JUMP_UP,
    [DIR_WEST]  = MOVEMENT_ACTION_JUMP_LEFT,
    [DIR_EAST]  = MOVEMENT_ACTION_JUMP_RIGHT,
};

static const u8 sOppositeDirections[] =
{
    [DIR_NONE]  = DIR_NONE,
    [DIR_SOUTH] = DIR_NORTH,
    [DIR_NORTH] = DIR_SOUTH,
    [DIR_WEST]  = DIR_EAST,
    [DIR_EAST]  = DIR_WEST,
};

static u8 CheckMovementInputAcroBike(u8 *newDirection, u16 newKeys, u16 heldKeys);

// ---- 小さな補助関数（FR 本体の同名関数と同じ処理） ----

static struct ObjectEvent *GetPlayerObjEvent(void)
{
    return &gObjectEvents[gPlayerAvatar.objectEventId];
}

static u8 GetPlayerMovementDirection(void)
{
    return GetPlayerObjEvent()->movementDirection;
}

static u8 GetPlayerFacingDirection(void)
{
    return GetPlayerObjEvent()->facingDirection;
}

static u8 GetOppositeDirection(u8 direction)
{
    if (direction >= ARRAY_COUNT(sOppositeDirections))
        return direction;
    return sOppositeDirections[direction];
}

static u8 GetJumpMovementAction(u8 direction)
{
    if (direction >= ARRAY_COUNT(sJumpMovementActions))
        direction = DIR_NONE;
    return sJumpMovementActions[direction];
}

static void SetObjectEventDirection(struct ObjectEvent *objectEvent, u8 direction)
{
    objectEvent->previousMovementDirection = objectEvent->facingDirection;
    if (!objectEvent->facingDirectionLocked)
        objectEvent->facingDirection = direction;
    objectEvent->movementDirection = direction;
}

static void Bike_SetBikeStill(void)
{
    gPlayerAvatar.bikeFrameCounter = 0;
    gPlayerAvatar.bikeSpeed = PLAYER_SPEED_STANDING;
}

// 進めない向きなら今の進行方向に置き換える
static u8 FaceableDirection(u8 direction)
{
    struct ObjectEvent *playerObjEvent = GetPlayerObjEvent();

    if (!FR_CanBikeFaceDirectionOnRail(direction, playerObjEvent->currentMetatileBehavior))
        return playerObjEvent->movementDirection;
    return direction;
}

static bool8 CanFaceDirection(u8 direction)
{
    return FR_CanBikeFaceDirectionOnRail(direction, GetPlayerObjEvent()->currentMetatileBehavior);
}

static bool8 IsBlockingCollision(u8 collision)
{
    return collision > COLLISION_NONE && collision < COLLISION_VERTICAL_RAIL;
}

static bool8 WillPlayerCollideWithCollision(u8 collision, u8 direction)
{
    if (direction == DIR_NORTH || direction == DIR_SOUTH)
    {
        if (collision == COLLISION_ISOLATED_VERTICAL_RAIL || collision == COLLISION_VERTICAL_RAIL)
            return FALSE;
    }
    else if (collision == COLLISION_ISOLATED_HORIZONTAL_RAIL || collision == COLLISION_HORIZONTAL_RAIL)
    {
        return FALSE;
    }
    return TRUE;
}

// ---- 入力履歴（横跳び・振り向きジャンプの判定用） ----

static u8 Bike_DPadToDirection(u16 heldKeys)
{
    if (heldKeys & DPAD_UP)
        return DIR_NORTH;
    if (heldKeys & DPAD_DOWN)
        return DIR_SOUTH;
    if (heldKeys & DPAD_LEFT)
        return DIR_WEST;
    if (heldKeys & DPAD_RIGHT)
        return DIR_EAST;
    return DIR_NONE;
}

static void PushHistory(u32 *history, u8 *timers, u8 input)
{
    u8 i;

    *history = (*history << 4) | (input & 0xF);
    for (i = HISTORY_LEN - 1; i != 0; i--)
        timers[i] = timers[i - 1];
    timers[0] = 1;
}

static void UpdateHistory(u32 *history, u8 *timers, u8 input)
{
    if (input == (*history & 0xF))
    {
        if (timers[0] < TIMER_MAX)
            timers[0]++;
    }
    else
    {
        PushHistory(history, timers, input);
        gPlayerAvatar.bikeSpeed = PLAYER_SPEED_STANDING;
    }
}

static void AcroBike_TryHistoryUpdate(u16 heldKeys)
{
    UpdateHistory(&gPlayerAvatar.directionHistory, DIR_TIMERS, Bike_DPadToDirection(heldKeys));
    UpdateHistory(&gPlayerAvatar.abStartSelectHistory, AB_TIMERS,
                  heldKeys & (A_BUTTON | B_BUTTON | SELECT_BUTTON | START_BUTTON));
}

static bool8 IsWithinTimerList(const u8 *timers, const u8 *limits)
{
    u8 i;

    for (i = 0; i < HISTORY_LEN && limits[i] != 0; i++)
    {
        if (timers[i] > limits[i])
            return FALSE;
    }
    return TRUE;
}

static u8 AcroBike_GetJumpDirection(void)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sAcroBikeTricksList); i++)
    {
        const struct BikeHistoryInputInfo *info = &sAcroBikeTricksList[i];

        if ((gPlayerAvatar.directionHistory & info->dirHistoryMask) == info->dirHistoryMatch
         && (gPlayerAvatar.abStartSelectHistory & info->abStartSelectHistoryMask) == info->abStartSelectHistoryMatch
         && IsWithinTimerList(DIR_TIMERS, info->dirTimerHistoryList)
         && IsWithinTimerList(AB_TIMERS, info->abStartSelectHistoryList))
            return info->direction;
    }
    return DIR_NONE;
}

// ---- 入力 → 遷移の決定 ----

static u8 AcroBikeHandleInputNormal(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
    u8 direction = GetPlayerMovementDirection();

    gPlayerAvatar.bikeFrameCounter = 0;
    if (*newDirection == DIR_NONE)
    {
        *newDirection = direction;
        gPlayerAvatar.runningState = NOT_MOVING;
        if (newKeys & B_BUTTON)
        {
            gPlayerAvatar.acroBikeState = ACRO_STATE_WHEELIE_STANDING;
            return ACRO_TRANS_NORMAL_TO_WHEELIE;
        }
        return ACRO_TRANS_FACE_DIRECTION;
    }
    if (*newDirection == direction && (heldKeys & B_BUTTON) && gPlayerAvatar.bikeSpeed == PLAYER_SPEED_STANDING)
    {
        gPlayerAvatar.bikeSpeed++;
        gPlayerAvatar.acroBikeState = ACRO_STATE_WHEELIE_MOVING;
        return ACRO_TRANS_WHEELIE_RISING_MOVING;
    }
    if (*newDirection != direction && gPlayerAvatar.runningState != MOVING)
    {
        gPlayerAvatar.acroBikeState = ACRO_STATE_TURNING;
        gPlayerAvatar.newDirBackup = *newDirection;
        gPlayerAvatar.runningState = NOT_MOVING;
        return CheckMovementInputAcroBike(newDirection, newKeys, heldKeys);
    }
    gPlayerAvatar.runningState = MOVING;
    return ACRO_TRANS_MOVING;
}

static u8 AcroBikeHandleInputTurning(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
    u8 direction;

    *newDirection = gPlayerAvatar.newDirBackup;
    gPlayerAvatar.bikeFrameCounter++;
    if (gPlayerAvatar.bikeFrameCounter > TURN_WAIT_FRAMES)
    {
        gPlayerAvatar.runningState = TURN_DIRECTION;
        gPlayerAvatar.acroBikeState = ACRO_STATE_NORMAL;
        Bike_SetBikeStill();
        return ACRO_TRANS_TURN_DIRECTION;
    }
    direction = GetPlayerMovementDirection();
    if (*newDirection == AcroBike_GetJumpDirection())
    {
        Bike_SetBikeStill();
        gPlayerAvatar.bikeSpeed = PLAYER_SPEED_NORMAL;
        if (*newDirection == GetOppositeDirection(direction))
        {
            gPlayerAvatar.acroBikeState = ACRO_STATE_TURN_JUMP;
            return ACRO_TRANS_TURN_JUMP;
        }
        gPlayerAvatar.runningState = MOVING;
        gPlayerAvatar.acroBikeState = ACRO_STATE_SIDE_JUMP;
        return ACRO_TRANS_SIDE_JUMP;
    }
    *newDirection = direction;
    return ACRO_TRANS_FACE_DIRECTION;
}

static u8 AcroBikeHandleInputWheelieStanding(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
    u8 direction = GetPlayerMovementDirection();

    gPlayerAvatar.runningState = NOT_MOVING;
    if (!(heldKeys & B_BUTTON))
    {
        gPlayerAvatar.bikeFrameCounter = 0;
        *newDirection = direction;
        gPlayerAvatar.acroBikeState = ACRO_STATE_NORMAL;
        Bike_SetBikeStill();
        return ACRO_TRANS_WHEELIE_TO_NORMAL;
    }
    gPlayerAvatar.bikeFrameCounter++;
    if (gPlayerAvatar.bikeFrameCounter >= BUNNY_HOP_HOLD_FRAMES)
    {
        *newDirection = direction;
        gPlayerAvatar.acroBikeState = ACRO_STATE_BUNNY_HOP;
        Bike_SetBikeStill();
        return ACRO_TRANS_WHEELIE_HOPPING_STANDING;
    }
    if (*newDirection == direction)
    {
        gPlayerAvatar.runningState = MOVING;
        gPlayerAvatar.acroBikeState = ACRO_STATE_WHEELIE_MOVING;
        Bike_SetBikeStill();
        return ACRO_TRANS_WHEELIE_MOVING;
    }
    if (*newDirection == DIR_NONE)
    {
        *newDirection = direction;
        return ACRO_TRANS_WHEELIE_IDLE;
    }
    gPlayerAvatar.runningState = TURN_DIRECTION;
    return ACRO_TRANS_WHEELIE_IDLE;
}

static u8 AcroBikeHandleInputBunnyHop(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
    u8 direction = GetPlayerMovementDirection();

    if (!(heldKeys & B_BUTTON))
    {
        Bike_SetBikeStill();
        *newDirection = direction;
        gPlayerAvatar.runningState = NOT_MOVING;
        gPlayerAvatar.acroBikeState = ACRO_STATE_NORMAL;
        return ACRO_TRANS_WHEELIE_TO_NORMAL;
    }
    if (*newDirection == DIR_NONE)
    {
        *newDirection = direction;
        gPlayerAvatar.runningState = NOT_MOVING;
        return ACRO_TRANS_WHEELIE_HOPPING_STANDING;
    }
    if (*newDirection != direction && gPlayerAvatar.runningState != MOVING)
    {
        gPlayerAvatar.runningState = TURN_DIRECTION;
        return ACRO_TRANS_WHEELIE_HOPPING_STANDING;
    }
    gPlayerAvatar.runningState = MOVING;
    return ACRO_TRANS_WHEELIE_HOPPING_MOVING;
}

static u8 AcroBikeHandleInputWheelieMoving(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
    u8 direction = GetPlayerFacingDirection();

    if (!(heldKeys & B_BUTTON))
    {
        Bike_SetBikeStill();
        gPlayerAvatar.acroBikeState = ACRO_STATE_NORMAL;
        if (*newDirection == DIR_NONE)
        {
            *newDirection = direction;
            gPlayerAvatar.runningState = NOT_MOVING;
            return ACRO_TRANS_WHEELIE_TO_NORMAL;
        }
        if (*newDirection != direction && gPlayerAvatar.runningState != MOVING)
        {
            gPlayerAvatar.runningState = NOT_MOVING;
            return ACRO_TRANS_WHEELIE_TO_NORMAL;
        }
        gPlayerAvatar.runningState = MOVING;
        return ACRO_TRANS_WHEELIE_LOWERING_MOVING;
    }
    if (*newDirection == DIR_NONE)
    {
        *newDirection = direction;
        gPlayerAvatar.acroBikeState = ACRO_STATE_WHEELIE_STANDING;
        gPlayerAvatar.runningState = NOT_MOVING;
        Bike_SetBikeStill();
        return ACRO_TRANS_WHEELIE_IDLE;
    }
    if (direction != *newDirection && gPlayerAvatar.runningState != MOVING)
    {
        gPlayerAvatar.runningState = NOT_MOVING;
        return ACRO_TRANS_WHEELIE_IDLE;
    }
    gPlayerAvatar.runningState = MOVING;
    return ACRO_TRANS_WHEELIE_MOVING;
}

static u8 AcroBikeHandleInputSidewaysJump(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
    struct ObjectEvent *playerObjEvent = GetPlayerObjEvent();

    playerObjEvent->facingDirectionLocked = 0;
    SetObjectEventDirection(playerObjEvent, playerObjEvent->facingDirection);
    gPlayerAvatar.acroBikeState = ACRO_STATE_NORMAL;
    return CheckMovementInputAcroBike(newDirection, newKeys, heldKeys);
}

static u8 AcroBikeHandleInputTurnJump(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
    gPlayerAvatar.acroBikeState = ACRO_STATE_NORMAL;
    return CheckMovementInputAcroBike(newDirection, newKeys, heldKeys);
}

static u8 (*const sAcroBikeInputHandlers[])(u8 *, u16, u16) =
{
    [ACRO_STATE_NORMAL]           = AcroBikeHandleInputNormal,
    [ACRO_STATE_TURNING]          = AcroBikeHandleInputTurning,
    [ACRO_STATE_WHEELIE_STANDING] = AcroBikeHandleInputWheelieStanding,
    [ACRO_STATE_BUNNY_HOP]        = AcroBikeHandleInputBunnyHop,
    [ACRO_STATE_WHEELIE_MOVING]   = AcroBikeHandleInputWheelieMoving,
    [ACRO_STATE_SIDE_JUMP]        = AcroBikeHandleInputSidewaysJump,
    [ACRO_STATE_TURN_JUMP]        = AcroBikeHandleInputTurnJump,
};

static u8 CheckMovementInputAcroBike(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
    // 想定外の値（FR 側の自転車状態が残っていた等）は通常状態に戻す
    if (gPlayerAvatar.acroBikeState >= ARRAY_COUNT(sAcroBikeInputHandlers))
        gPlayerAvatar.acroBikeState = ACRO_STATE_NORMAL;
    return sAcroBikeInputHandlers[gPlayerAvatar.acroBikeState](newDirection, newKeys, heldKeys);
}

// ---- 遷移の実行 ----

static void AcroBikeTransition_NormalToWheelie(u8 direction)
{
    PlayerStartWheelie(FaceableDirection(direction));
}

static void AcroBikeTransition_WheelieToNormal(u8 direction)
{
    PlayerEndWheelie(FaceableDirection(direction));
}

static void AcroBikeTransition_WheelieIdle(u8 direction)
{
    PlayerIdleWheelie(FaceableDirection(direction));
}

static void AcroBikeTransition_WheelieHoppingStanding(u8 direction)
{
    PlayerStandingHoppingWheelie(FaceableDirection(direction));
}

static void AcroBikeTransition_WheelieHoppingMoving(u8 direction)
{
    u8 collision;

    if (!CanFaceDirection(direction))
    {
        AcroBikeTransition_WheelieHoppingStanding(GetPlayerObjEvent()->movementDirection);
        return;
    }
    collision = FR_GetBikeCollision(direction);
    if (collision != COLLISION_NONE && collision != COLLISION_WHEELIE_HOP)
    {
        if (collision == COLLISION_LEDGE_JUMP)
        {
            PlayerLedgeHoppingWheelie(direction);
            return;
        }
        if (collision >= COLLISION_STOP_SURFING && collision <= COLLISION_DIRECTIONAL_STAIR_WARP)
            return;
        if (collision < COLLISION_VERTICAL_RAIL)
        {
            AcroBikeTransition_WheelieHoppingStanding(direction);
            return;
        }
    }
    PlayerMovingHoppingWheelie(direction);
}

static void AcroBikeTransition_SideJump(u8 direction)
{
    u8 collision = FR_GetBikeCollision(direction);

    if (collision != COLLISION_NONE)
    {
        if (collision == COLLISION_PUSHED_BOULDER)
            return;
        if (collision < COLLISION_ISOLATED_VERTICAL_RAIL || !WillPlayerCollideWithCollision(collision, direction))
        {
            FR_BikeTransition_TurnDirection(direction);
            return;
        }
    }
    PlaySE(SE_BIKE_HOP);
    GetPlayerObjEvent()->facingDirectionLocked = 1;
    PlayerSetAnimId(GetJumpMovementAction(direction), COPY_MOVE_WALK);
}

static void AcroBikeTransition_TurnJump(u8 direction)
{
    PlayerAcroTurnJump(direction);
}

// ウィリーで前進するときの共通処理。進めたら TRUE
static bool8 TryWheelieForward(u8 direction)
{
    u8 collision = FR_GetBikeCollision(direction);

    if (!IsBlockingCollision(collision))
        return TRUE;
    if (collision == COLLISION_LEDGE_JUMP)
        PlayerLedgeHoppingWheelie(direction);
    else if (collision == COLLISION_WHEELIE_HOP)
        PlayerIdleWheelie(direction);
    else if (collision < COLLISION_STOP_SURFING)
        PlayerAcroWheelieCollide(direction);
    return FALSE;
}

static void AcroBikeTransition_WheelieMoving(u8 direction)
{
    if (!CanFaceDirection(direction))
    {
        PlayerIdleWheelie(GetPlayerObjEvent()->movementDirection);
        return;
    }
    if (!TryWheelieForward(direction))
        return;
    PlayerAcroWheelieMove(direction);
    gPlayerAvatar.runningState = MOVING;
}

static void AcroBikeTransition_WheelieRisingMoving(u8 direction)
{
    if (!CanFaceDirection(direction))
    {
        PlayerStartWheelie(GetPlayerObjEvent()->movementDirection);
        return;
    }
    if (!TryWheelieForward(direction))
        return;
    PlayerAcroPopWheelie(direction);
    gPlayerAvatar.runningState = MOVING;
}

static void AcroBikeTransition_WheelieLoweringMoving(u8 direction)
{
    u8 collision;

    if (!CanFaceDirection(direction))
    {
        PlayerEndWheelie(GetPlayerObjEvent()->movementDirection);
        return;
    }
    collision = FR_GetBikeCollision(direction);
    if (IsBlockingCollision(collision))
    {
        if (collision == COLLISION_LEDGE_JUMP)
            PlayerJumpLedge(direction);
        else if (collision < COLLISION_STOP_SURFING || collision > COLLISION_DIRECTIONAL_STAIR_WARP)
            PlayerEndWheelie(direction);
        return;
    }
    FR_BikeTransition_MoveDirection(direction);
}

static void (*const sAcroBikeTransitions[])(u8) =
{
    [ACRO_TRANS_FACE_DIRECTION]          = FR_BikeTransition_FaceDirection,
    [ACRO_TRANS_TURN_DIRECTION]          = FR_BikeTransition_TurnDirection,
    [ACRO_TRANS_MOVING]                  = FR_BikeTransition_MoveDirection,
    [ACRO_TRANS_NORMAL_TO_WHEELIE]       = AcroBikeTransition_NormalToWheelie,
    [ACRO_TRANS_WHEELIE_TO_NORMAL]       = AcroBikeTransition_WheelieToNormal,
    [ACRO_TRANS_WHEELIE_IDLE]            = AcroBikeTransition_WheelieIdle,
    [ACRO_TRANS_WHEELIE_HOPPING_STANDING] = AcroBikeTransition_WheelieHoppingStanding,
    [ACRO_TRANS_WHEELIE_HOPPING_MOVING]  = AcroBikeTransition_WheelieHoppingMoving,
    [ACRO_TRANS_SIDE_JUMP]               = AcroBikeTransition_SideJump,
    [ACRO_TRANS_TURN_JUMP]               = AcroBikeTransition_TurnJump,
    [ACRO_TRANS_WHEELIE_MOVING]          = AcroBikeTransition_WheelieMoving,
    [ACRO_TRANS_WHEELIE_RISING_MOVING]   = AcroBikeTransition_WheelieRisingMoving,
    [ACRO_TRANS_WHEELIE_LOWERING_MOVING] = AcroBikeTransition_WheelieLoweringMoving,
};

// FR の MovePlayerOnBike の置き換え。ダート以外は FR の元の処理をそのまま行う。
void MovePlayerOnBike_Hook(u8 direction, u16 newKeys, u16 heldKeys)
{
    if (gPlayerAvatar.flags & PLAYER_AVATAR_FLAG_ACRO_BIKE)
    {
        AcroBike_TryHistoryUpdate(heldKeys);
        sAcroBikeTransitions[CheckMovementInputAcroBike(&direction, newKeys, heldKeys)](direction);
    }
    else
    {
        FR_sBikeTransitions[FR_GetBikeTransitionId(&direction, newKeys, heldKeys)](direction);
    }
}
