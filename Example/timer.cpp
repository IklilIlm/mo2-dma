#include <functions.h>
#include <entity.h>
#include <DMALibrary/Memory/Memory.h>
#include <cstdio>
#include <cstdint>

TimerSettings g_Timer{};

// ============================================================
// TimerUpdate
// ============================================================
void TimerUpdate()
{
    static bool  keyWasDown = false;
    static bool  active = false;
    static float originalValue = 1.0f;
    static uintptr_t cachedPawn = 0;

    // если фича выключена — принудительно откатываем
    if (!g_Timer.enabled) {
        if (active && cachedPawn) {
            mem.Write<float>(cachedPawn + offset_CustomTimeDilation, originalValue);
            printf("[timer] disabled - restored %.2f\n", originalValue);
        }
        keyWasDown = false;
        active = false;
        cachedPawn = 0;
        return;
    }

    int key = g_Timer.key;
    if (key == 0) return;

    bool keyDown = mem.GetKeyboard()->IsKeyDown(key);

    if (keyDown && !keyWasDown)
    {
        // --- взять актуальный Pawn (без локов, через снапшот) ---
        uintptr_t myPawn = 0;
        {
            auto snap = Entity::Snapshot();
            if (snap) myPawn = snap->hero.APawn;
        }
        if (!myPawn || myPawn == (uintptr_t)-1) return;

        if (!active)
        {
            // --- включение ---
            float cur = mem.Read<float>(myPawn + offset_CustomTimeDilation);

            // защита от мусора
            if (cur <= 0.0f || cur > 100.0f)
                cur = 1.0f;

            originalValue = cur;
            cachedPawn = myPawn;

            float newValue = g_Timer.speed;
            mem.Write<float>(cachedPawn + offset_CustomTimeDilation, newValue);

            active = true;
            printf("[timer] ON  %.2f -> %.2f\n", originalValue, newValue);
        }
        else
        {
            // --- выключение ---
            if (cachedPawn && cachedPawn != (uintptr_t)-1) {
                mem.Write<float>(cachedPawn + offset_CustomTimeDilation, originalValue);
                printf("[timer] OFF %.2f restored\n", originalValue);
            }
            active = false;
            cachedPawn = 0;
        }
    }

    keyWasDown = keyDown;

    // страховка: если Pawn сменился (респавн, телепорт) — вернём оригинал у старого
    if (active && cachedPawn)
    {
        uintptr_t curPawn = 0;
        {
            auto snap = Entity::Snapshot();
            if (snap) curPawn = snap->hero.APawn;
        }
        if (curPawn && curPawn != cachedPawn)
        {
            // вернули старому
            mem.Write<float>(cachedPawn + offset_CustomTimeDilation, originalValue);
            printf("[timer] pawn changed, restored old\n");

            // переключились на новый
            float cur = mem.Read<float>(curPawn + offset_CustomTimeDilation);
            if (cur <= 0.0f || cur > 100.0f)
                cur = 1.0f;

            originalValue = cur;
            cachedPawn = curPawn;

            float newValue = g_Timer.speed;
            mem.Write<float>(cachedPawn + offset_CustomTimeDilation, newValue);
        }
    }
}