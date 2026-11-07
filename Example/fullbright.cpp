#include "fullbright.h"
#include "functions.h"
#include "global.h"
#include "entity.h"
#include <DMALibrary/Memory/Memory.h>
#include <Windows.h>
#include <string>
#include <mutex>
#include <cstring>

// ============================================================
// CAMERA FULLBRIGHT — local post-process via offsets
//
// Логика:
//   ON  : читаем оригиналы -> сохраняем -> пишем свои значения
//   OFF : восстанавливаем оригиналы (ровно то, что было до ON)
//
// Пишем только когда:
//   - фича только что включилась
//   - сменился pawn/камера (ре-скан + пересохранение оригиналов)
//   - изменились настройки в UI (camBias)
//   - фича выключилась (restore)
// ============================================================

FullBrightSettings g_FullBright;

namespace {

    uintptr_t g_pPP = 0;
    uintptr_t g_pBlend = 0;
    uintptr_t g_pPawn = 0;
    bool      g_viaPOV = false;
    std::atomic<bool> g_CamValid{ false };
    std::atomic<bool> g_WasEnabled{ false };

    // ------------------------------------------------------------
    // Оригинальные значения, считанные ОДИН РАЗ при включении.
    // Именно их восстановим при выключении.
    // ------------------------------------------------------------
    struct OriginalValues {
        bool      valid = false;   // оригиналы считаны
        uintptr_t pPP = 0;       // к какому адресу относятся
        uintptr_t pBlend = 0;

        float     bias = 1.0f;
        float     minB = 1.0f;
        float     maxB = 1.0f;
        float     spdDown = 1.0f;
        float     ao = 1.0f;
        float     blend = 1.0f;

        // override-биты: сохраняем целые байты, т.к. биты соседние с другими
        uint8_t   ovrExposureMinMax = 0;   // offset_PP_Ovr_ExposureMinMax
        uint8_t   ovrExposureSpdBias = 0;  // offset_PP_Ovr_ExposureSpdBias
        uint8_t   ovrAO = 0;               // offset_PP_Ovr_AO
    };
    OriginalValues g_orig;

    // ------------------------------------------------------------
    // Кэш того, что мы последний раз записали (чтобы не спамить записью)
    // ------------------------------------------------------------
    struct WrittenCache {
        bool      valid = false;
        uintptr_t pPP = 0;
        uintptr_t pBlend = 0;
        float     bias = 0.0f;
        float     minB = 0.0f;
        float     maxB = 0.0f;
        float     spdDown = 0.0f;
        float     ao = 0.0f;
    };
    WrittenCache g_written;

    bool ScanCamera()
    {
        auto snap = Entity::Snapshot();
        if (!snap) return false;

        const uintptr_t pCamMgr = snap->hero.APlayerCameraManager;
        const uintptr_t pPawn = snap->hero.APawn;
        if (!pCamMgr && !pPawn) return false;

        if (pPawn)
        {
            const uintptr_t pCam = mem.Read<uintptr_t>(pPawn + offset_CameraComponent);
            if (pCam)
            {
                g_pPP = pCam + offset_CamPostProcess;
                g_pBlend = pCam + offset_CamBlendWeight;
                g_viaPOV = false;
                g_pPawn = pPawn;
                g_CamValid.store(true, std::memory_order_relaxed);
                return true;
            }
        }

        if (pCamMgr)
        {
            const uintptr_t pov = pCamMgr + offset_CamCachePrivate + offset_CamMgr_POV;
            g_pPP = pov + offset_POV_PostProcess;
            g_pBlend = pov + offset_POV_PostProcessBlend;
            g_viaPOV = true;
            g_pPawn = pPawn;
            g_CamValid.store(true, std::memory_order_relaxed);
            return true;
        }

        return false;
    }

    // ------------------------------------------------------------
    // Чтение 1 байта override-поля
    // ------------------------------------------------------------
    uint8_t ReadByte(uintptr_t addr)
    {
        uint8_t v = 0;
        mem.Read(addr, &v, 1);
        return v;
    }

    // ------------------------------------------------------------
    // Прочитать все оригиналы (сохранить) ДО первой записи.
    // ------------------------------------------------------------
    void CaptureOriginals()
    {
        g_orig.pPP = g_pPP;
        g_orig.pBlend = g_pBlend;

        mem.Read(g_pPP + offset_PP_AutoExposureBias, &g_orig.bias, 4);
        mem.Read(g_pPP + offset_PP_AutoExposureMinB, &g_orig.minB, 4);
        mem.Read(g_pPP + offset_PP_AutoExposureMaxB, &g_orig.maxB, 4);
        mem.Read(g_pPP + offset_PP_AutoExposureSpeedDown, &g_orig.spdDown, 4);
        mem.Read(g_pPP + offset_PP_AmbientOcclusion, &g_orig.ao, 4);

        if (g_pBlend)
            mem.Read(g_pBlend, &g_orig.blend, 4);

        g_orig.ovrExposureMinMax = ReadByte(g_pPP + offset_PP_Ovr_ExposureMinMax);
        g_orig.ovrExposureSpdBias = ReadByte(g_pPP + offset_PP_Ovr_ExposureSpdBias);
        g_orig.ovrAO = ReadByte(g_pPP + offset_PP_Ovr_AO);

        g_orig.valid = true;
    }

    // ------------------------------------------------------------
    // Восстановить оригиналы (вызывается ОДИН раз при выключении)
    // ------------------------------------------------------------
    void RestoreOriginals()
    {
        if (!g_orig.valid) return;
        if (!g_pPP || g_pPP != g_orig.pPP) return;   // камера уже другая — восстанавливать некуда

        mem.Write(g_pPP + offset_PP_AutoExposureBias, g_orig.bias);
        mem.Write(g_pPP + offset_PP_AutoExposureMinB, g_orig.minB);
        mem.Write(g_pPP + offset_PP_AutoExposureMaxB, g_orig.maxB);
        mem.Write(g_pPP + offset_PP_AutoExposureSpeedDown, g_orig.spdDown);
        mem.Write(g_pPP + offset_PP_AmbientOcclusion, g_orig.ao);

        if (g_orig.pBlend)
            mem.Write(g_orig.pBlend, g_orig.blend);

        // override-биты возвращаем как было (перезаписываем байт целиком)
        mem.Write(g_pPP + offset_PP_Ovr_ExposureMinMax, g_orig.ovrExposureMinMax);
        mem.Write(g_pPP + offset_PP_Ovr_ExposureSpdBias, g_orig.ovrExposureSpdBias);
        mem.Write(g_pPP + offset_PP_Ovr_AO, g_orig.ovrAO);

        g_orig.valid = false;
    }

    // ------------------------------------------------------------
    // Установить нужный бит в override-байте (только если не стоит)
    // ------------------------------------------------------------
    void WriteOverrideByte(uint32_t off, uint8_t mask)
    {
        uint8_t cur = ReadByte(g_pPP + off);
        if ((cur & mask) == mask) return;
        cur |= mask;
        mem.Write(g_pPP + off, &cur, 1);
    }

    void WriteOverridesOnce()
    {
        WriteOverrideByte(offset_PP_Ovr_ExposureMinMax, (1u << 5) | (1u << 6));
        WriteOverrideByte(offset_PP_Ovr_ExposureSpdBias, (1u << 1) | (1u << 2));
        WriteOverrideByte(offset_PP_Ovr_AO, (1u << 7));
    }

    // ------------------------------------------------------------
    // Целевые значения для текущих настроек
    // ------------------------------------------------------------
    void ComputeTargets(float& bias, float& minB, float& maxB,
        float& spdDown, float& ao)
    {
        bias = g_FullBright.camBias;

        const float boost = (bias > 0.0f) ? bias * 0.2f : 0.0f;
        minB = 1.0f + boost;
        maxB = 1.0f + boost;

        spdDown = 0.0f;
        ao = 0.0f;
    }

    // ------------------------------------------------------------
    // Полная запись: сначала (если ещё не считали) CaptureOriginals()
    // ------------------------------------------------------------
    void WriteAll()
    {
        if (!g_orig.valid || g_orig.pPP != g_pPP)
            CaptureOriginals();

        WriteOverridesOnce();

        float bias, minB, maxB, spdDown, ao;
        ComputeTargets(bias, minB, maxB, spdDown, ao);

        mem.Write(g_pPP + offset_PP_AutoExposureBias, bias);
        mem.Write(g_pPP + offset_PP_AutoExposureMinB, minB);
        mem.Write(g_pPP + offset_PP_AutoExposureMaxB, maxB);
        mem.Write(g_pPP + offset_PP_AutoExposureSpeedDown, spdDown);
        mem.Write(g_pPP + offset_PP_AmbientOcclusion, ao);

        if (g_pBlend)
            mem.Write(g_pBlend, 1.0f);

        g_written.valid = true;
        g_written.pPP = g_pPP;
        g_written.pBlend = g_pBlend;
        g_written.bias = bias;
        g_written.minB = minB;
        g_written.maxB = maxB;
        g_written.spdDown = spdDown;
        g_written.ao = ao;
    }

    inline bool SettingsChanged()
    {
        if (!g_written.valid) return true;

        float bias, minB, maxB, spdDown, ao;
        ComputeTargets(bias, minB, maxB, spdDown, ao);

        return bias != g_written.bias
            || minB != g_written.minB
            || maxB != g_written.maxB
            || spdDown != g_written.spdDown
            || ao != g_written.ao
            || g_pPP != g_written.pPP
            || g_pBlend != g_written.pBlend;
    }

    // ------------------------------------------------------------
    // Полный сброс состояния (после restore)
    // ------------------------------------------------------------
    void ResetState()
    {
        g_CamValid.store(false, std::memory_order_relaxed);
        g_written.valid = false;
        g_orig.valid = false;
        g_pPP = g_pBlend = g_pPawn = 0;
        g_viaPOV = false;
    }

} // namespace

// ============================================================
// Public API
// ============================================================

void FullBrightUpdate()
{
    const bool on = g_FullBright.enabled;

    // ---- выключили: восстановить оригиналы, один раз ----
    if (!on)
    {
        if (g_WasEnabled.exchange(false))
        {
            if (g_CamValid.load())
                RestoreOriginals();
            ResetState();
        }
        return;
    }

    // ---- включено ----

    // pawn сменился — надо пере-сканировать и заново сохранить оригиналы
    // новой камеры (у нового pawn'а свои оригинальные PP-значения).
    {
        auto snap = Entity::Snapshot();
        if (snap && snap->hero.APawn && snap->hero.APawn != g_pPawn)
        {
            // ВАЖНО: восстанавливать оригиналы на старом pawn'е смысла нет
            // (его уже нет), просто сбрасываем всё и цепляемся к новому.
            ResetState();
        }
    }

    if (!g_CamValid.load())
    {
        if (!ScanCamera())
            return;

        // Новая камера — сначала читаем оригиналы, потом пишем свои.
        WriteAll();
        g_WasEnabled.store(true, std::memory_order_relaxed);
        return;
    }

    // камера та же — пишем только если поменялись настройки
    if (SettingsChanged())
        WriteAll();

    g_WasEnabled.store(true, std::memory_order_relaxed);
}

const char* FullBrightStatus()
{
    static std::string s;
    static char buf[96];

    if (!g_FullBright.enabled) { s = "off"; return s.c_str(); }
    if (!g_CamValid.load()) { s = "cam:not found"; return s.c_str(); }

    snprintf(buf, sizeof(buf), "cam ok 0x%llX%s%s",
        (unsigned long long)(g_pPP & 0xFFFFFFFFFFFFull),
        g_viaPOV ? " (POV)" : "",
        g_orig.valid ? " [saved]" : " [no-save]");
    return buf;
}