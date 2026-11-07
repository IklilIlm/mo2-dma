#include <functions.h>
#include <entity.h>
#include <DMALibrary/Memory/Memory.h>
#include <MyMath.h>
#include <imgui.h>
#include <global.h>
#include <cstdio>
#include <cmath>
#include <chrono>

// ============================================================
// NAV POINTS — точки навигации по миру (вкладка Maps)
// Точка ставится по текущим координатам игрока, рисуется
// color-dot + метки (имя / метры). Всё отключаемо по отдельности.
// Сохранение/чтение точек — ТОЛЬКО через Misc -> Save / Load
// (config.bin, см. global.cpp).
// ============================================================

NavPointsSettings g_NavPoints{};


static Vector3 GetHeroPos()
{
    auto snap = Entity::Snapshot();
    if (!snap) return Vector3{};
    if (!snap->hero.APawn || snap->hero.APawn == (uintptr_t)-1) return Vector3{};

    for (const auto& a : snap->actors)
        if (a.AActor == snap->hero.APawn) return a.position;

    return Vector3{};
}

void NavPointAdd()
{
    if (g_NavPoints.count >= NAV_MAX_POINTS) {
        printf("[navpts] limit reached (%d)\n", NAV_MAX_POINTS);
        return;
    }

    Vector3 pos = GetHeroPos();
    if (pos.x == 0.0 && pos.y == 0.0 && pos.z == 0.0) {
        printf("[navpts] add skipped - no local position (waiting for entity update)\n");
        return;
    }

    NavPoint p{};
    p.pos = pos;
    p.enabled = true;
    p.showLabel = true;
    p.showColor = true;
    p.showDist = true;
    p.size = 6.0f;
    snprintf(p.name, sizeof(p.name), "Point %d", g_NavPoints.count + 1);

    g_NavPoints.points[g_NavPoints.count++] = p;

    printf("[navpts] added (%.1f, %.1f, %.1f)  total=%d\n",
        pos.x, pos.y, pos.z, g_NavPoints.count);
}

void NavPointRemove(int index)
{
    if (index < 0 || index >= g_NavPoints.count) return;

    for (int j = index; j < g_NavPoints.count - 1; ++j)
        g_NavPoints.points[j] = g_NavPoints.points[j + 1];

    g_NavPoints.count--;
    g_NavPoints.points[g_NavPoints.count] = NavPoint{};
}

void NavPointsClear()
{
    g_NavPoints.count = 0;
    for (int k = 0; k < NAV_MAX_POINTS; ++k)
        g_NavPoints.points[k] = NavPoint{};
    printf("[navpts] cleared\n");
}

void NavPointsDraw()
{
    if (!g_NavPoints.enabledDraw || g_NavPoints.count == 0) return;
    if (AppSetting::get().screenWidth <= 0 || AppSetting::get().screenHeight <= 0) return;

    auto snap = Entity::Snapshot();
    if (!snap) return;

    const camera& cam = GetRenderCamera(snap->hero);

    if (cam.Location.x == 0.0 && cam.Location.y == 0.0 && cam.Location.z == 0.0) return;
    if (cam.FOV < 1.0f || cam.FOV > 180.0f) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    const float screenW = (float)AppSetting::get().screenWidth;
    const float screenH = (float)AppSetting::get().screenHeight;

    Vector3 myPos{};
    if (snap->hero.APawn && snap->hero.APawn != (uintptr_t)-1) {
        for (const auto& a : snap->actors)
            if (a.AActor == snap->hero.APawn) { myPos = a.position; break; }
    }

    for (int i = 0; i < g_NavPoints.count; ++i) {
        const NavPoint& p = g_NavPoints.points[i];
        if (!p.enabled) continue;

        Vector2 screen{};
        if (!worldToScreen(p.pos, cam.Location,
            (float)cam.Rotation.x, (float)cam.Rotation.y, (float)cam.Rotation.z,
            cam.FOV, screenW, screenH, screen))
            continue;   // за камерой

        ImVec2 pt((float)screen.x, (float)screen.y);

        // ---- цветная точка (или белая, если цвет выключен) ----
        ImU32 dotCol = p.showColor ? EspColorU32(p.color) : IM_COL32(255, 255, 255, 255);

        drawList->AddCircleFilled(pt, p.size, dotCol, 16);

        // ---- дистанция ----
        double dx = p.pos.x - myPos.x;
        double dy = p.pos.y - myPos.y;
        double dz = p.pos.z - myPos.z;
        float distM = (float)(sqrt(dx * dx + dy * dy + dz * dz) / 100.0);

        char label[96];
        if (p.showDist)
            snprintf(label, sizeof(label), "%s  [%.0fm]", p.name, distM);
        else
            snprintf(label, sizeof(label), "%s", p.name);

        if (p.showLabel || p.showDist) {
            ImVec2 ts = ImGui::CalcTextSize(label);
            float tx = p.showLabel ? pt.x - ts.x * 0.5f : pt.x + p.size + 3.0f;
            float ty = pt.y - p.size - 6.0f - ts.y;

            drawList->AddText(ImVec2(tx, ty), dotCol, label);
        }
    }
}
