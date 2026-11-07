#pragma once
#include <Windows.h>

#include <vector>
#include <string>
#include <cstdint>

#include <imgui.h>
#include <MyMath.h>
#include <motor_synergy.h>

// ============================================================
// FONT
// ============================================================
extern ImFont* g_font;

// ============================================================
// TIMER (CustomTimeDilation)
// ============================================================
struct TimerSettings final {
    bool  enabled = false;        // активна ли фича
    int   key = 0;                // VK код бинда (0 = не задан)
    float speed = 1.0f;           // множитель (1.0 .. 3.0)
};

extern TimerSettings g_Timer;

void TimerUpdate();

// ============================================================
// FRIEND
// ============================================================
struct FriendSettings final {
    bool  enabled = false;
    int   friendKey = VK_MBUTTON;

    // настройки выбора цели (как в аимботе)
    float maxAngle = 5.0f;    // градусов от прицела
    float maxDistance = 100.0f;  // метров
    bool  playersOnly = true;    // только BP_PlayerCharacter_C
};

extern FriendSettings g_FriendSettings;

void FriendsUpdate();
bool IsFriend(uintptr_t pActor);

// ============================================================
// AIMBOT
// ============================================================
struct AimbotSettings final {
    bool  enabled = false;
    int   aimKey = VK_XBUTTON2;
    float fovPixels = 150.f;
    float maxDistM = 100.f;
    float aimHeight = 0.7f;

    bool  skipFriends = true;
    bool  drawFovCircle = true;
    bool  showAimPoint = true;

    bool  lockTarget = false;
    bool  blockMouse = false;

    float trajSpeedScale = 0.7f;
    float curveScale = 1.0f;
    float overshootProb = 0.15f;

    float mouseScaleX = 1.0f;
    float mouseScaleY = 1.0f;

    int   mode = 0;
    int   autoMoveMs = 80;
    int   maxSendsPerFrame = 4;

    motor_synergy::config traj;
};

extern AimbotSettings g_Aimbot;

void AimbotUpdate();
void AimbotDraw();
void AimbotReset();

// ============================================================
// TRIGGERBOT
// ============================================================
struct TriggerbotSettings final {
    bool  enabled = false;
    int   activationKey = 0;
    int   fireKey = VK_LBUTTON;

    float triggerWidth = 20.f;
    float triggerHeight = 20.f;

    float maxDistM = 100.f;
    bool  skipFriends = true;

    bool  drawTriggerFov = true;

    int   triggerDelayMs = 30;
    int   holdTimeMs = 40;
    int   cooldownMs = 100;
};

extern TriggerbotSettings g_Triggerbot;

void TriggerbotUpdate();

// ============================================================
// MOUNT DATA
// ============================================================
struct MountData final {
    uintptr_t pController = 0;
    uintptr_t pMount = 0;

    float   fMountStaminaMax = 0.f;
    int32_t vMountMaxSpeed_count = 0;
    float   vMountMaxSpeed[16] = {};
    float   fSwimSpeedMultiplier = 0.f;
    float   SlowestSpeedTurnRate = 0.f;
    float   FastestSpeedTurnRate = 0.f;
    float   fMountBreakSpeed = 0.f;
    float   fMountThrottleSpeed = 0.f;
    float   fRiderWeightPenaltyThreshold = 0.f;
    float   fMaxArmorWeight_ModSpeed = 0.f;
    float   fMaxArmorWeight_ModTurnRate = 0.f;
    float   fSlidingMinSlopeAngle = 0.f;
    float   fSlidingMaxSlopeAngle = 0.f;

    uint8_t MountRiderPose = 0;
    float   fMountedSpeed = 0.f;
    int32_t iThrottleTgt = 0;
    float   fCurStamina = 0.f;
    bool    bLocalPlayerIsAllowedAsPassenger = false;
    int32_t iAiMountableFromLevel = 0;
    int32_t iAiMountableSlots = 0;
    float   fTickStamina = 0.f;
    int32_t iThrottleCap = 0;
    float   DefaultCapsuleRadius = 0.f;
    float   DefaultCapsuleHalfHeight = 0.f;

    bool    valid = false;
};

struct MountEditBuffer final {
    float fMountStaminaMax = 0.f;
    float fSwimSpeedMultiplier = 0.f;
    float fRiderWeightPenaltyThreshold = 0.f;
    float fMaxArmorWeight_ModSpeed = 0.f;
    float fMaxArmorWeight_ModTurnRate = 0.f;
    float fMountThrottleSpeed = 0.f;
    float fMountBreakSpeed = 0.f;
    float fCurStamina = 0.f;
    float fMountMaxSpeed[16] = {};
};

struct MountStaminaSettings final {
    bool  enabled = false;
    int   key = 0;
    float amount = 100.f;
};

extern MountData             g_MountData;
extern MountEditBuffer       g_MountEdit;
extern MountStaminaSettings  g_MountStamina;

void MovementUpdate();
void MovementWriteField(uintptr_t fieldOffset, float value);
void MovementWriteMaxSpeed(int index, float value);
void MountSetStamina(float value);
void MountAddStamina(float amount);
void MountStaminaUpdate();

// ============================================================
// BINDING
// ============================================================
bool IsWaitingForBind();
bool IsWaitingForBind(int* target);
void StartBind(int* target);
void CancelBind();
bool PollBindKey();
const char* GetKeyName(int vk, char* buf, size_t bufSize);

// ============================================================
// MENU / WALLHACK
// ============================================================
void menu();
void wallhack();

// ============================================================
// TELEPORT
// ============================================================
struct TeleportSettings final {
    bool      enabled = false;
    uintptr_t targetActor = 0;
    bool      hasFrozen = false;
    Vector3   frozenTarget{};

    float stopDistance = 3.0f;   // m — радиус остановки
    float speed = 100.0f;        // m/s — скорость полёта (теперь РАБОТАЕТ)
    float zOffset = 0.5f;        // m — смещение по Z относительно цели
    bool  moveZ = true;          // true = 3D (Lift+Fly+Land), false = XY-only

    // Pull Point (свободная точка): цель не актор, а точка, которую двигают стрелки.
    // Точка спавнится на луче прицела (центр экрана): Up/Down = дистанция от
    // игрока (дальше/ближе), Left/Right = сдвиг вбок. Стрелки работают только
    // когда enabled = false.
    bool  pointMode = false;     // цель — свободная точка (Spawn Point)
    float pointStep = 8.0f;      // m/s — скорость регулировки стрелками
    float pointDist = 5.0f;      // m — дистанция точки на луче прицела (запоминается)
};

extern TeleportSettings g_Teleport;
void TeleportDraw();
void TeleportUpdate();
void TeleportCapture();
void TeleportClear();

// Свободная pull-точка (World): треугольник на моих координатах, движение стрелками
void TeleportSpawnPoint();
void TeleportPointMove();   // вызывать каждый кадр (мультитенант: двигает frozenTarget)

// ============================================================
// NAV POINTS (вкладка Maps — точки навигации по миру)
// ============================================================
#define NAV_MAX_POINTS       64
#define NAV_POINT_NAME_LEN   48

struct NavPoint final {
    bool    enabled   = true;   // точка активна (рисуется)
    bool    showLabel = true;   // подпись (имя)
    bool    showColor = true;   // цветная точка (off = белая)
    bool    showDist  = true;   // метры дистанции

    Vector3 pos{};
    char    name[NAV_POINT_NAME_LEN] = {};
    float   color[4] = { 1.f, 0.15f, 0.15f, 1.f };
    float   size = 6.0f;        // радиус точки на экране (px)
};

struct NavPointsSettings final {
    bool     enabledDraw = true;    // рисовать все точки
    int      count = 0;
    NavPoint points[NAV_MAX_POINTS];
};

extern NavPointsSettings g_NavPoints;

void NavPointAdd();                       // поставить точку в текущих координатах игрока
void NavPointRemove(int index);
void NavPointsClear();
void NavPointsDraw();                     // вызывать каждый кадр (main loop)

// ============================================================
// ESP + RADAR SETTINGS
// ============================================================
#define ESP_CLASS_LEN        128
#define ESP_MAX_CLASS_RULES  256
#define ESP_MAX_IGNORE       1024

struct EspClassRule final {
    char  namePart[ESP_CLASS_LEN] = {};
    float color[4] = { 1.f, 1.f, 1.f, 1.f };
    bool  enabled = true;

    bool  drawBox = true;
    bool  drawHealthBar = true;
    bool  drawHealthText = true;
    bool  drawAttack = true;
    bool  drawFName = true;
    bool  drawNick = true;
    bool  drawDistance = true;
    bool  drawIcons = true;
};

struct EspSettings final {
    // ============ ESP ============
    bool  enabled = true;
    float maxDrawDist = 180.0f;

    // nearDist = Min dist (m): минимальная дальность отрисовки (ближе — не рисуем).
    // Поле переиспользовано у бывшего fade ("Full at"), имя сохранено ради config.bin.
    // midDist/farDist/midAlphaPct/minAlphaPct — прозрачность убрана, поля мертвы,
    // но удалять нельзя: config.bin пишется сырым дампом sizeof(EspSettings),
    // удаление сдвинет Aimbot/Triggerbot/Timer в старых конфигах.
    float nearDist = 0.0f;
    float midDist = 30.0f;      // (не используется)
    float farDist = 180.0f;     // (не используется)
    float midAlphaPct = 50.0f;  // (не используется)
    float minAlphaPct = 12.0f;  // (не используется)

    // --- Форма ---
    bool  drawBox = true;          // точка для всех (игрок/NPC/лут) — дефолт ON
    bool  drawDot = true;
    float boxThickness = 2.0f;
    float dotSize = 2.0f;

    // --- Здоровье: выключено по умолчанию ---
    bool  drawHealthBar = false;
    bool  drawHealthText = false;
    bool  hpTextOnlyPlayers = true;

    // --- Текст: выключено по умолчанию, кроме дистанции ---
    bool  drawNick = false;
    bool  drawFName = false;
    bool  drawAttack = false;
    bool  drawDistance = true;     // ← единственный оставленный текст
    bool  drawIcons = true;

    float textScale = 1.0f;
    float atkTextScale = 1.5f;
    float iconScale = 2.0f;

    float friendColor[4] = { 0.f, 1.f, 0.f, 1.f };
    float enemyColor[4] = { 1.f, 1.f, 0.f, 1.f };    // не используется, оставлен для config.bin
    float npcColor[4] = { 1.f, 0.63f, 0.f, 1.f }; // единый цвет для всех (не friend, не rule)
    float textColor[4] = { 1.f, 1.f, 1.f, 1.f };
    float atkColor[4] = { 1.f, 1.f, 1.f, 1.f };

    bool  iconsEnabled = true;

    int          classRuleCount = 0;
    EspClassRule classRules[ESP_MAX_CLASS_RULES];

    // ============ IGNORE CLASSES ============
    int   ignoreCount = 1;
    char  ignoreClasses[ESP_MAX_IGNORE][ESP_CLASS_LEN] = {
        "PlayerCameraManager"
    };

    // ============ RADAR ============
    bool  radarEnabled = true;
    float radarPosX = 20.0f;
    float radarPosY = 20.0f;
    float radarSize = 220.0f;
    float radarWorldSize = 20000.0f;
    float radarMinDist = 100.0f;
    float radarMaxDist = 30000.0f;

    bool  radarShowFriends = true;
    bool  radarShowEnemies = true;
    bool  radarShowRing = true;
    bool  radarShowAxes = true;
    bool  radarShowPlayer = true;
    float radarDotSize = 3.0f;
    float radarPlayerDotSize = 4.0f;

    float radarBgColor[4] = { 0.f, 0.f, 0.f, 0.47f };
    float radarBorderColor[4] = { 0.f, 1.f, 0.f, 0.71f };
    float radarAxisColor[4] = { 0.f, 1.f, 0.f, 0.24f };
    float radarPlayerColor[4] = { 0.f, 1.f, 0.f, 1.f };

    bool  radarUseEspColors = true;
    float radarFriendColor[4] = { 0.f, 1.f, 0.f, 1.f };
    float radarEnemyColor[4] = { 1.f, 0.24f, 0.24f, 1.f };

    int   radarRingCount = 3;
    float radarRingSizes[3] = { 0.3f, 0.6f, 0.9f };
    float radarRingColor[4] = { 0.f, 1.f, 0.f, 0.24f };

    // Прозрачность radar убрана — рисуется полной альфой внутри [radarMinDist, radarMaxDist].
    // Поля ниже мертвы, но оставлены ради config.bin (см. комментарий у ESP-фейда).
    bool  radarUseEspFade = true;     // (не используется)
    float radarNearDist = 20.0f;      // (не используется)
    float radarMidDist = 30.0f;       // (не используется)
    float radarFarDist = 180.0f;      // (не используется)
    float radarMidAlphaPct = 50.0f;   // (не используется)
    float radarMinAlphaPct = 30.0f;   // (не используется)

    // ============ MY COORDS ============
    bool  showMyCoords = true;
    float myCoordsColor[4] = { 1.f, 1.f, 1.f, 1.f };
};

extern EspSettings g_Esp;

ImU32 EspColorU32(const float rgba[4], float alphaMul = 1.0f);

// ============================================================
// CAMERA FULLBRIGHT  (fullbright.cpp)
// ============================================================
struct FullBrightSettings final {
    bool  enabled = false;
    float camBias = 3.0f;   // FPostProcessSettings::AutoExposureBias
};

extern FullBrightSettings g_FullBright;

void FullBrightUpdate();             // вызывать каждый кадр (main loop)
const char* FullBrightStatus();      // "cam:0x..." / "cam:not found" / "off"
