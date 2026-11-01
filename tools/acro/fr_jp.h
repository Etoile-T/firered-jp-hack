// 日本版 FR (BPRJ) の構造体・定数・関数の宣言。
// 構造体は pokefirered (ref/pokefirered/include/global.fieldmap.h) から必要分だけ写したもの。
// アドレスは fr_jp_symbols.ld で定義する（notes/fr_jp_bikes.md 参照）。
#ifndef GUARD_FR_JP_H
#define GUARD_FR_JP_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char s8;
typedef signed short s16;
typedef u8 bool8;

#define TRUE 1
#define FALSE 0
#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define STATIC_ASSERT(cond, name) typedef char name[(cond) ? 1 : -1]

// keys
#define A_BUTTON      0x0001
#define B_BUTTON      0x0002
#define SELECT_BUTTON 0x0004
#define START_BUTTON  0x0008
#define DPAD_RIGHT    0x0010
#define DPAD_LEFT     0x0020
#define DPAD_UP       0x0040
#define DPAD_DOWN     0x0080

enum { DIR_NONE, DIR_SOUTH, DIR_NORTH, DIR_WEST, DIR_EAST };

#define PLAYER_AVATAR_FLAG_MACH_BIKE (1 << 1)
#define PLAYER_AVATAR_FLAG_ACRO_BIKE (1 << 2)

enum { PLAYER_SPEED_STANDING, PLAYER_SPEED_NORMAL };
enum { NOT_MOVING, TURN_DIRECTION, MOVING };

enum
{
    COLLISION_NONE,
    COLLISION_OUTSIDE_RANGE,
    COLLISION_IMPASSABLE,
    COLLISION_ELEVATION_MISMATCH,
    COLLISION_OBJECT_EVENT,
    COLLISION_STOP_SURFING,
    COLLISION_LEDGE_JUMP,
    COLLISION_PUSHED_BOULDER,
    COLLISION_DIRECTIONAL_STAIR_WARP, // エメラルドでは COLLISION_ROTATING_GATE
    COLLISION_WHEELIE_HOP,
    COLLISION_ISOLATED_VERTICAL_RAIL,
    COLLISION_ISOLATED_HORIZONTAL_RAIL,
    COLLISION_VERTICAL_RAIL,
    COLLISION_HORIZONTAL_RAIL,
};

#define SE_WALL_HIT 7
#define SE_BIKE_HOP 28

#define MOVEMENT_ACTION_JUMP_DOWN  0x4E
#define MOVEMENT_ACTION_JUMP_UP    0x4F
#define MOVEMENT_ACTION_JUMP_LEFT  0x50
#define MOVEMENT_ACTION_JUMP_RIGHT 0x51

#define COPY_MOVE_WALK 2

struct Coords16
{
    s16 x;
    s16 y;
};

struct ObjectEvent
{
    u32 active:1;
    u32 singleMovementActive:1;
    u32 triggerGroundEffectsOnMove:1;
    u32 triggerGroundEffectsOnStop:1;
    u32 disableCoveringGroundEffects:1;
    u32 landingJump:1;
    u32 heldMovementActive:1;
    u32 heldMovementFinished:1;
    u32 frozen:1;
    u32 facingDirectionLocked:1;
    u32 disableAnim:1;
    u32 enableAnim:1;
    u32 inanimate:1;
    u32 invisible:1;
    u32 offScreen:1;
    u32 trackedByCamera:1;
    u32 isPlayer:1;
    u32 hasReflection:1;
    u32 inShortGrass:1;
    u32 inShallowFlowingWater:1;
    u32 inSandPile:1;
    u32 inHotSprings:1;
    u32 hasShadow:1;
    u32 spriteAnimPausedBackup:1;
    u32 spriteAffineAnimPausedBackup:1;
    u32 disableJumpLandingGroundEffect:1;
    u32 fixedPriority:1;
    u32 hideReflection:1;
    u8 spriteId;
    u8 graphicsId;
    u8 movementType;
    u8 trainerType;
    u8 localId;
    u8 mapNum;
    u8 mapGroup;
    u8 currentElevation:4;
    u8 previousElevation:4;
    struct Coords16 initialCoords;
    struct Coords16 currentCoords;
    struct Coords16 previousCoords;
    u8 facingDirection:4;
    u8 movementDirection:4;
    u16 rangeX:4;
    u16 rangeY:4;
    u8 fieldEffectSpriteId;
    u8 warpArrowSpriteId;
    u8 movementActionId;
    u8 trainerRange_berryTreeId;
    u8 currentMetatileBehavior;
    u8 previousMetatileBehavior;
    u8 previousMovementDirection;
    u8 directionSequenceIndex;
    u8 playerCopyableMovement;
};
STATIC_ASSERT(sizeof(struct ObjectEvent) == 0x24, ObjectEventSize);

// FR の PlayerAvatar はエメラルドの abStartSelectTimerHistory[8] が削られている。
// 移植では dirTimerHistory[8] を前半=方向キー、後半=A/B/START/SELECT の履歴として分けて使う。
struct PlayerAvatar
{
    u8 flags;
    u8 transitionFlags;
    u8 runningState;
    u8 tileTransitionState;
    u8 spriteId;
    u8 objectEventId;
    bool8 preventStep;
    u8 gender;
    u8 acroBikeState;
    u8 newDirBackup;
    u8 bikeFrameCounter;
    u8 bikeSpeed;
    u32 directionHistory;
    u32 abStartSelectHistory;
    u8 dirTimerHistory[8];
    u16 lastSpinTile;
};
STATIC_ASSERT(__builtin_offsetof(struct PlayerAvatar, dirTimerHistory) == 0x14, PlayerAvatarTimers);

// RAM
extern struct PlayerAvatar gPlayerAvatar;
extern struct ObjectEvent gObjectEvents[];

// field_player_avatar.c
void PlayerSetAnimId(u8 movementActionId, u8 copyableMovement);
void PlayerOnBikeCollide(u8 direction);
void PlayerFaceDirection(u8 direction);
void PlayerJumpLedge(u8 direction);
void PlayerIdleWheelie(u8 direction);
void PlayerStartWheelie(u8 direction);
void PlayerEndWheelie(u8 direction);
void PlayerStandingHoppingWheelie(u8 direction);
void PlayerMovingHoppingWheelie(u8 direction);
void PlayerLedgeHoppingWheelie(u8 direction);
void PlayerAcroTurnJump(u8 direction);
void PlayerAcroWheelieCollide(u8 direction);
void PlayerAcroPopWheelie(u8 direction);
void PlayerAcroWheelieMove(u8 direction);

// bike.c (FR 側の static 関数・テーブル)
u8 FR_GetBikeTransitionId(u8 *direction, u16 newKeys, u16 heldKeys);
void FR_BikeTransition_FaceDirection(u8 direction);
void FR_BikeTransition_TurnDirection(u8 direction);
void FR_BikeTransition_MoveDirection(u8 direction);
u8 FR_GetBikeCollision(u8 direction);
bool8 FR_CanBikeFaceDirectionOnRail(u8 direction, u8 metatileBehavior);
extern void (*const FR_sBikeTransitions[])(u8);

// sound.c
void PlaySE(u16 songNum);

#endif // GUARD_FR_JP_H
