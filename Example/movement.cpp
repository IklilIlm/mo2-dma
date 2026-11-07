#include <functions.h>
#include <entity.h>
#include <global.h>
#include <mutex>
#include <DMALibrary/Memory/Memory.h>
#include <cstdio>
#include <cmath>

MountData            g_MountData{};
MountEditBuffer      g_MountEdit{};
MountStaminaSettings g_MountStamina{};

// ============================================================
// Найти свою лошадь через MOAIControllerMount
// ============================================================
static uintptr_t FindMyMount(uintptr_t& outController)
{
    outController = 0;

    auto snap = Entity::Snapshot();
    if (!snap) return 0;

    const auto& hero = snap->hero;
    const auto& actors = snap->actors;

    if (!hero.APawn) return 0;

    for (const auto& a : actors) {
        if (a.UEClassName.find("MOAIControllerMount") == std::string::npos) continue;
        if (!a.AActor || a.AActor == (uintptr_t)-1) continue;

        uintptr_t rider = 0;
        mem.Read(a.AActor + offset_MountRider, &rider, sizeof(uintptr_t));
        if (rider != hero.APawn) continue;

        uintptr_t pMount = 0;
        mem.Read(a.AActor + offset_MountPtr, &pMount, sizeof(uintptr_t));
        if (!pMount || pMount == (uintptr_t)-1) continue;

        outController = a.AActor;
        return pMount;
    }

    for (const auto& a : actors) {
        if (a.UEClassName.find("MOAIControllerMount") == std::string::npos) continue;
        if (!a.AActor || a.AActor == (uintptr_t)-1) continue;

        uintptr_t pMount = 0;
        mem.Read(a.AActor + offset_MountPtr, &pMount, sizeof(uintptr_t));
        if (!pMount || pMount == (uintptr_t)-1) continue;

        outController = a.AActor;
        return pMount;
    }

    return 0;
}

// ============================================================
// Разово прочитать ВСЁ (по кнопке Update)
// ============================================================
void MovementUpdate()
{
    g_MountData = MountData{};

    uintptr_t pController = 0;
    uintptr_t pMount = FindMyMount(pController);

    if (!pMount) {
        printf("[movement] mount not found\n");
        return;
    }

    g_MountData.pController = pController;
    g_MountData.pMount = pMount;

    // === FMOAIMountStats (pMount + 0x1080) ===
    mem.Read(pMount + offset_fMountStaminaMax, &g_MountData.fMountStaminaMax, sizeof(float));
    mem.Read(pMount + offset_vMountMaxSpeed_count, &g_MountData.vMountMaxSpeed_count, sizeof(int32_t));

    uintptr_t vData = 0;
    mem.Read(pMount + offset_vMountMaxSpeed_data, &vData, sizeof(uintptr_t));
    if (vData && vData != (uintptr_t)-1
        && g_MountData.vMountMaxSpeed_count > 0
        && g_MountData.vMountMaxSpeed_count <= MOUNT_MAX_SPEED_SLOTS)
    {
        for (int i = 0; i < g_MountData.vMountMaxSpeed_count; ++i)
            mem.Read(vData + i * 4, &g_MountData.vMountMaxSpeed[i], sizeof(float));
    }

    mem.Read(pMount + offset_fSwimSpeedMultiplier, &g_MountData.fSwimSpeedMultiplier, sizeof(float));
    mem.Read(pMount + offset_SlowestSpeedTurnRate, &g_MountData.SlowestSpeedTurnRate, sizeof(float));
    mem.Read(pMount + offset_FastestSpeedTurnRate, &g_MountData.FastestSpeedTurnRate, sizeof(float));
    mem.Read(pMount + offset_fMountBreakSpeed, &g_MountData.fMountBreakSpeed, sizeof(float));
    mem.Read(pMount + offset_fMountThrottleSpeed, &g_MountData.fMountThrottleSpeed, sizeof(float));
    mem.Read(pMount + offset_fRiderWeightPenaltyThreshold, &g_MountData.fRiderWeightPenaltyThreshold, sizeof(float));
    mem.Read(pMount + offset_fMaxArmorWeight_ModSpeed, &g_MountData.fMaxArmorWeight_ModSpeed, sizeof(float));
    mem.Read(pMount + offset_fMaxArmorWeight_ModTurnRate, &g_MountData.fMaxArmorWeight_ModTurnRate, sizeof(float));
    mem.Read(pMount + offset_fSlidingMinSlopeAngle, &g_MountData.fSlidingMinSlopeAngle, sizeof(float));
    mem.Read(pMount + offset_fSlidingMaxSlopeAngle, &g_MountData.fSlidingMaxSlopeAngle, sizeof(float));

    // === Runtime ===
    mem.Read(pMount + offset_MountRiderPose, &g_MountData.MountRiderPose, sizeof(uint8_t));
    mem.Read(pMount + offset_fMountedSpeed, &g_MountData.fMountedSpeed, sizeof(float));
    mem.Read(pMount + offset_iThrottleTgt, &g_MountData.iThrottleTgt, sizeof(int32_t));
    mem.Read(pMount + offset_fCurStamina, &g_MountData.fCurStamina, sizeof(float));
    mem.Read(pMount + offset_bAllowedAsPassenger, &g_MountData.bLocalPlayerIsAllowedAsPassenger, sizeof(bool));
    mem.Read(pMount + offset_iAiMountableFromLevel, &g_MountData.iAiMountableFromLevel, sizeof(int32_t));
    mem.Read(pMount + offset_iAiMountableSlots, &g_MountData.iAiMountableSlots, sizeof(int32_t));
    mem.Read(pMount + offset_fTickStamina, &g_MountData.fTickStamina, sizeof(float));
    mem.Read(pMount + offset_iThrottleCap, &g_MountData.iThrottleCap, sizeof(int32_t));
    mem.Read(pMount + offset_DefaultCapsuleRadius, &g_MountData.DefaultCapsuleRadius, sizeof(float));
    mem.Read(pMount + offset_DefaultCapsuleHalfHeight, &g_MountData.DefaultCapsuleHalfHeight, sizeof(float));

    g_MountData.valid = true;

    // Заполняем edit-buffer текущими значениями
    g_MountEdit.fMountStaminaMax = g_MountData.fMountStaminaMax;
    g_MountEdit.fSwimSpeedMultiplier = g_MountData.fSwimSpeedMultiplier;
    g_MountEdit.fRiderWeightPenaltyThreshold = g_MountData.fRiderWeightPenaltyThreshold;
    g_MountEdit.fMaxArmorWeight_ModSpeed = g_MountData.fMaxArmorWeight_ModSpeed;
    g_MountEdit.fMaxArmorWeight_ModTurnRate = g_MountData.fMaxArmorWeight_ModTurnRate;
    g_MountEdit.fMountThrottleSpeed = g_MountData.fMountThrottleSpeed;
    g_MountEdit.fMountBreakSpeed = g_MountData.fMountBreakSpeed;
    g_MountEdit.fCurStamina = g_MountData.fCurStamina;

    for (int i = 0; i < MOUNT_MAX_SPEED_SLOTS; ++i)
        g_MountEdit.fMountMaxSpeed[i] = g_MountData.vMountMaxSpeed[i];

    printf("[movement] mount update: pMount=0x%p count=%d\n",
        (void*)pMount, g_MountData.vMountMaxSpeed_count);
}

// ============================================================
// Записать ОДНО поле по offset'у от pMount
// ============================================================
void MovementWriteField(uintptr_t fieldOffset, float value)
{
    if (!g_MountData.valid || !g_MountData.pMount) {
        printf("[movement] write skipped - no mount\n");
        return;
    }

    mem.Write(g_MountData.pMount + fieldOffset, &value, sizeof(float));
    printf("[movement] wrote 0x%X = %.3f\n", (unsigned)fieldOffset, value);
}

// ============================================================
// Записать один элемент TArray<float> vMountMaxSpeed[i]
// ============================================================
void MovementWriteMaxSpeed(int index, float value)
{
    if (!g_MountData.valid || !g_MountData.pMount) {
        printf("[movement] speed write skipped - no mount\n");
        return;
    }
    if (index < 0 || index >= g_MountData.vMountMaxSpeed_count || index >= MOUNT_MAX_SPEED_SLOTS) {
        printf("[movement] speed write skipped - invalid index %d (count=%d)\n",
            index, g_MountData.vMountMaxSpeed_count);
        return;
    }

    uintptr_t vData = 0;
    mem.Read(g_MountData.pMount + offset_vMountMaxSpeed_data, &vData, sizeof(uintptr_t));
    if (!vData || vData == (uintptr_t)-1) {
        printf("[movement] speed write skipped - null array\n");
        return;
    }

    mem.Write(vData + index * 4, &value, sizeof(float));
    printf("[movement] wrote vMountMaxSpeed[%d] = %.3f\n", index, value);
}

// ============================================================
// STAmina — прямые сеттеры
// ============================================================
void MountSetStamina(float value)
{
    if (!g_MountData.valid || !g_MountData.pMount) {
        printf("[stamina] write skipped - no mount\n");
        return;
    }
    if (value < 0.f) value = 0.f;
    if (g_MountData.fMountStaminaMax > 0.f && value > g_MountData.fMountStaminaMax)
        value = g_MountData.fMountStaminaMax;

    mem.Write(g_MountData.pMount + offset_fCurStamina, &value, sizeof(float));
    g_MountData.fCurStamina = value;
    printf("[stamina] set -> %.1f\n", value);
}

void MountAddStamina(float amount)
{
    if (!g_MountData.valid || !g_MountData.pMount) {
        printf("[stamina] add skipped - no mount\n");
        return;
    }
    MountSetStamina(g_MountData.fCurStamina + amount);
}

// ============================================================
// STAmina REFILL — по клавише
// ============================================================
void MountStaminaUpdate()
{
    static bool keyWasDown = false;

    if (!g_MountStamina.enabled) {
        keyWasDown = false;
        return;
    }

    int key = g_MountStamina.key;
    if (key == 0) return;

    bool keyDown = mem.GetKeyboard()->IsKeyDown(key);
    if (keyDown && !keyWasDown)
        MountAddStamina(g_MountStamina.amount);

    keyWasDown = keyDown;
}