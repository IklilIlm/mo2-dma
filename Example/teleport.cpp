#include <functions.h>
#include <entity.h>
#include <DMALibrary/Memory/Memory.h>
#include <MyMath.h>
#include <imgui.h>
#include <global.h>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <chrono>

TeleportSettings g_Teleport{};

void TeleportCapture()
{
    if (!g_Teleport.targetActor) {
        printf("[teleport] capture skipped - no target\n");
        return;
    }

    auto snap = Entity::Snapshot();
    if (!snap) return;

    Vector3 pos{};
    bool found = false;
    for (const auto& a : snap->actors) {
        if (a.AActor == g_Teleport.targetActor) { pos = a.position; found = true; break; }
    }

    if (!found) {
        printf("[teleport] capture skipped - target not in list\n");
        return;
    }

    g_Teleport.frozenTarget = pos;
    g_Teleport.hasFrozen = true;
    g_Teleport.enabled = false;

    printf("[teleport] captured (%.1f, %.1f, %.1f)\n", pos.x, pos.y, pos.z);
}

void TeleportClear()
{
    g_Teleport.enabled = false;
    g_Teleport.targetActor = 0;
    g_Teleport.hasFrozen = false;
    g_Teleport.frozenTarget = Vector3{};
    g_Teleport.pointMode = false;
    printf("[teleport] cleared\n");
}

// ============================================================
// PULL POINT — свободная точка (треугольник) на луче прицела.
// Spawn Point ставит точку на пересечении луча из камеры через
// ЦЕНТР ЭКРАНА на дистанции pointDist (м). Стрелками точка
// регулируется: Up/Down — дальше/ближе от игрока вдоль луча,
// Left/Right — вбок (перпендикуляр взгляда).
// После — обычный Enable Pull притянет к ней.
// ============================================================
void TeleportSpawnPoint()
{
    // камера: свежая UI-кадровая (CameraRefresh), иначе из снапшота
    camera cam{};
    if (g_RenderCameraValid.load(std::memory_order_relaxed)) {
        cam = g_RenderCamera;
    }
    else {
        auto snap = Entity::Snapshot();
        if (!snap) {
            printf("[tp] spawn point skipped - no snapshot\n");
            return;
        }
        cam = snap->hero.CameraCachePrivate;
    }
    if (cam.FOV < 1.0f || cam.FOV > 180.0f) {
        printf("[tp] spawn point skipped - bad camera\n");
        return;
    }

    // луч через прицел = forward камеры. Формула как axisX в worldToScreen,
    // поэтому точка проецируется РОВНО в центр экрана.
    const double pitch = (double)cam.Rotation.x * (PI / 180.0);
    const double yaw   = (double)cam.Rotation.y * (PI / 180.0);
    const double cp = cos(pitch), sp = sin(pitch);
    const double cy = cos(yaw),   sy = sin(yaw);

    const double distCm = (double)g_Teleport.pointDist * 100.0;
    Vector3 pos = {
        cam.Location.x + cp * cy * distCm,
        cam.Location.y + cp * sy * distCm,
        cam.Location.z + sp * distCm
    };

    g_Teleport.targetActor = 0;
    g_Teleport.frozenTarget = pos;
    g_Teleport.hasFrozen = true;
    g_Teleport.enabled = false;
    g_Teleport.pointMode = true;

    printf("[tp] pull point spawned on crosshair dist=%.1fm (%.1f, %.1f, %.1f)\n",
        g_Teleport.pointDist, pos.x, pos.y, pos.z);
}

// Регулировка точки стрелками в реальном времени.
// Up/Down — дистанция от игрока вдоль текущего луча "камера -> точка"
//           (наводился выше — точка уходит вверх, ниже — вниз).
// Left/Right — сдвиг вбок по горизонтали (перпендикуляр взгляда).
// Работает только пока Enable Pull выключен (иначе не двигаем во время полёта).
void TeleportPointMove()
{
    if (!g_Teleport.pointMode || !g_Teleport.hasFrozen) return;
    if (g_Teleport.enabled) return;

    // камера: свежая UI-кадровая (CameraRefresh), иначе из снапшота
    camera cam{};
    if (g_RenderCameraValid.load(std::memory_order_relaxed)) {
        cam = g_RenderCamera;
    }
    else {
        auto snap = Entity::Snapshot();
        if (!snap) return;
        cam = snap->hero.CameraCachePrivate;
    }
    if (cam.FOV < 1.0f || cam.FOV > 180.0f) return;

    const double step = (double)g_Teleport.pointStep * 100.0;   // м -> см

    static auto lastTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - lastTime).count();
    lastTime = now;
    if (dt <= 0.0) dt = 0.016;
    if (dt > 0.1)  dt = 0.1;

    const double move = step * dt;

    bool moved = false;
    Vector3 t = g_Teleport.frozenTarget;

    // --- Up/Down: дистанция вдоль луча "камера -> точка" ---
    const double dx = t.x - cam.Location.x;
    const double dy = t.y - cam.Location.y;
    const double dz = t.z - cam.Location.z;
    const double dist = sqrt(dx * dx + dy * dy + dz * dz);

    if (dist >= 1.0) {
        double newDist = dist;
        if (GetAsyncKeyState(VK_UP)   & 0x8000) { newDist += move; moved = true; }   // дальше
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) { newDist -= move; moved = true; }   // ближе
        if (moved) {
            if (newDist < 100.0) newDist = 100.0;   // не ближе 1 м
            const double k = newDist / dist;        // масштаб вдоль текущего луча
            t.x = cam.Location.x + dx * k;
            t.y = cam.Location.y + dy * k;
            t.z = cam.Location.z + dz * k;
            g_Teleport.pointDist = (float)(newDist / 100.0);   // запоминаем для след. спавна
        }
    }

    // --- Left/Right: вбок по горизонтали (перпендикуляр взгляда) ---
    const double yaw = (double)cam.Rotation.y * (PI / 180.0);
    const double rx = -sin(yaw), ry = cos(yaw);   // правый вектор (как в worldToScreen)
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { t.x += rx * move; t.y += ry * move; moved = true; }
    if (GetAsyncKeyState(VK_LEFT)  & 0x8000) { t.x -= rx * move; t.y -= ry * move; moved = true; }

    if (moved) g_Teleport.frozenTarget = t;
}


enum class TeleportPhase { Lift, Fly, Land, Done };

void TeleportUpdate()
{
    if (!g_Teleport.enabled)   return;
    if (!g_Teleport.hasFrozen) return;

    uintptr_t myPawn = 0;
    {
        auto snap = Entity::Snapshot();
        if (snap) myPawn = snap->hero.APawn;
    }
    if (!myPawn || myPawn == (uintptr_t)-1) return;

    uintptr_t root = 0;
    mem.Read(myPawn + offset_RootComponent, &root, sizeof(uintptr_t));
    if (!root || root == (uintptr_t)-1) return;

    Vector3 myPos{};
    mem.Read(root + offset_RelativeLocation, &myPos, sizeof(Vector3));

    static auto lastTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - lastTime).count();
    lastTime = now;
    if (dt <= 0.0) dt = 0.016;
    if (dt > 0.1)  dt = 0.1;

    static TeleportPhase phase = TeleportPhase::Lift;
    static bool wasEnabled = false;
    static std::chrono::steady_clock::time_point nearRadiusStart;
    static bool nearRadiusActive = false;

    if (g_Teleport.enabled && !wasEnabled) {
        phase = TeleportPhase::Lift;
        nearRadiusActive = false;
        printf("[tp] starting: lift -> fly -> land\n");
    }

    Vector3 target = g_Teleport.frozenTarget;
    target.z += (double)g_Teleport.zOffset * 100.0;

    const double LIFT_HEIGHT_CM = 300.0;
    double liftZ = target.z + LIFT_HEIGHT_CM;

    double stopUnits = (double)g_Teleport.stopDistance * 100.0;
    const double BIG_STEP = 100.0;

    double dxFull = target.x - myPos.x;
    double dyFull = target.y - myPos.y;
    double dzFull = target.z - myPos.z;
    double distFull = sqrt(dxFull * dxFull + dyFull * dyFull + dzFull * dzFull);

    double nearRadius = stopUnits * 1.6;
    if (nearRadius < 500.0) nearRadius = 500.0;

    bool inNearRadius = (distFull < nearRadius);
    bool forceJump = false;

    if (inNearRadius) {
        if (!nearRadiusActive) { nearRadiusStart = now; nearRadiusActive = true; }
        double inRadSec = std::chrono::duration<double>(now - nearRadiusStart).count();
        if (inRadSec > 2.0) forceJump = true;
    }
    else nearRadiusActive = false;

    Vector3 newPos = myPos;
    bool doWrite = false;

    if (forceJump) {
        newPos = target; doWrite = true;
        phase = TeleportPhase::Done; g_Teleport.enabled = false;
        nearRadiusActive = false;
        printf("[tp] stuck in radius >2s - force jump\n");
    }

    if (!forceJump && phase == TeleportPhase::Lift) {
        double dz = liftZ - myPos.z;
        if (fabs(dz) < 100.0) {
            phase = TeleportPhase::Fly;
            printf("[tp] reached lift altitude (%.1fm)\n", myPos.z / 100.0);
        }
        else {
            newPos.x = myPos.x; newPos.y = myPos.y;
            if (dz > 0) { newPos.z = myPos.z + BIG_STEP; if (newPos.z > liftZ) newPos.z = liftZ; }
            else { newPos.z = myPos.z - BIG_STEP; if (newPos.z < liftZ) newPos.z = liftZ; }
            doWrite = true;
        }
    }

    if (!forceJump && phase == TeleportPhase::Fly) {
        double dx = target.x - myPos.x;
        double dy = target.y - myPos.y;
        double xyDist = sqrt(dx * dx + dy * dy);

        if (xyDist <= stopUnits) {
            phase = TeleportPhase::Land;
            printf("[tp] XY reached, landing (dist=%.2fm)\n", xyDist / 100.0);
        }
        else {
            double step = BIG_STEP * 2.0;
            if (step > xyDist) step = xyDist;
            double inv = 1.0 / xyDist;
            newPos.x = myPos.x + dx * inv * step;
            newPos.y = myPos.y + dy * inv * step;
            newPos.z = liftZ;
            doWrite = true;
        }
    }

    if (!forceJump && phase == TeleportPhase::Land) {
        newPos = target; doWrite = true;
        if (fabs(myPos.z - target.z) < 100.0) {
            phase = TeleportPhase::Done;
            g_Teleport.enabled = false;
            printf("[tp] done\n");
        }
    }

    if (doWrite) mem.Write(root + offset_RelativeLocation, &newPos, sizeof(Vector3));

    static int logTick = 0;
    if (++logTick % 10 == 0) {
        const char* pn = "?";
        switch (phase) {
        case TeleportPhase::Lift: pn = "LIFT"; break;
        case TeleportPhase::Fly:  pn = "FLY";  break;
        case TeleportPhase::Land: pn = "LAND"; break;
        case TeleportPhase::Done: pn = "DONE"; break;
        }
        printf("[tp][%s] dist=%.1fm myZ=%.0f liftZ=%.0f targetXY=(%.0f,%.0f)\n",
            pn, distFull / 100.0, myPos.z, liftZ, target.x, target.y);
    }

    wasEnabled = g_Teleport.enabled;
}

void TeleportDraw()
{
    if (!g_Teleport.hasFrozen) return;
    if (AppSetting::get().screenWidth <= 0 || AppSetting::get().screenHeight <= 0) return;

    auto snap = Entity::Snapshot();
    if (!snap) return;

    const auto& hero = snap->hero;
    const auto& actors = snap->actors;

    const camera& cam = GetRenderCamera(hero);   // ← СВЕЖАЯ КАМЕРА

    if (cam.Location.x == 0.0 && cam.Location.y == 0.0 && cam.Location.z == 0.0) return;
    if (cam.FOV < 1.0f || cam.FOV > 180.0f) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    const float screenW = (float)AppSetting::get().screenWidth;
    const float screenH = (float)AppSetting::get().screenHeight;

    Vector2 screen{};
    if (!worldToScreen(g_Teleport.frozenTarget, cam.Location,
        (float)cam.Rotation.x, (float)cam.Rotation.y, (float)cam.Rotation.z,
        cam.FOV, screenW, screenH, screen))
        return;

    ImVec2 p((float)screen.x, (float)screen.y);

    const float triSize = 16.0f;

    static const auto startTime = std::chrono::steady_clock::now();
    float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
    float baseAngle = t * 2.0f;

    ImVec2 verts[3];
    for (int i = 0; i < 3; ++i) {
        float a = baseAngle + i * (2.0f * 3.14159265f / 3.0f);
        verts[i] = ImVec2(p.x + cosf(a) * triSize, p.y + sinf(a) * triSize);
    }

    drawList->AddTriangleFilled(verts[0], verts[1], verts[2], IM_COL32(180, 180, 180, 90));
    drawList->AddTriangle(verts[0], verts[1], verts[2], IM_COL32(200, 200, 200, 180), 1.5f);
    drawList->AddCircleFilled(p, 3.0f, IM_COL32(255, 0, 0, 255), 12);
    drawList->AddCircle(p, 3.0f, IM_COL32(0, 0, 0, 220), 12, 1.2f);

    Vector3 myPos{};
    if (hero.APawn) {
        for (const auto& a : actors)
            if (a.AActor == hero.APawn) { myPos = a.position; break; }
    }

    double dx = g_Teleport.frozenTarget.x - myPos.x;
    double dy = g_Teleport.frozenTarget.y - myPos.y;
    double dz = g_Teleport.frozenTarget.z - myPos.z;
    float distM = (float)(sqrt(dx * dx + dy * dy + dz * dz) / 100.0);

    char label[64];
    snprintf(label, sizeof(label),
        g_Teleport.pointMode ? "POINT  %.1fm" : "TARGET %.1fm", distM);

    ImVec2 ts = ImGui::CalcTextSize(label);
    float tx = p.x - ts.x * 0.5f;
    float ty = p.y - triSize - 18.0f;

    drawList->AddText(ImVec2(tx, ty), IM_COL32(255, 80, 80, 255), label);
}