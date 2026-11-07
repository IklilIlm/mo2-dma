#include <WinSock2.h>
#include <Windows.h>

#include <functions.h>
#include <entity.h>
#include <global.h>
#include <MyMath.h>
#include <KmboxNet.h>
#include <motor_synergy.h>
#include <imgui.h>

#include <cstdio>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <vector>
#include <DMALibrary/Memory/Memory.h>

AimbotSettings     g_Aimbot;
TriggerbotSettings g_Triggerbot;

namespace {

    struct ActiveShot {
        bool   active = false;
        std::vector<motor_synergy::trajectory_point> pts;
        size_t idx = 0;
        int    sent_ix = 0, sent_iy = 0;
        std::chrono::steady_clock::time_point t0;
        uintptr_t target = 0;
    };

    struct TargetInfo {
        uintptr_t actor = 0;
        Vector3   world{};
        Vector2   screen{};
        float     distM = 0.f;
        float     charH = 100.f;
    };

    ActiveShot  g_shot;
    uintptr_t   g_lockedTarget = 0;
    bool        g_mouseBlocked = false;
    bool        g_wasKeyDown = false;

    struct TriggerState {
        bool onTarget = false;
        bool firing = false;
        bool keyHeld = false;
        std::chrono::steady_clock::time_point targetEnter;
        std::chrono::steady_clock::time_point fireStart;
        std::chrono::steady_clock::time_point lastFireEnd;
    };
    TriggerState g_trig;

    bool BuildTargetInfo(const ActorManager& a, const LocalManager& hero,
        float sw, float sh, bool enforceFov, TargetInfo& out)
    {
        const camera& cam = GetRenderCamera(hero);
        if (cam.FOV < 1.f || cam.FOV > 180.f) return false;
        if (cam.Location.x == 0.0 && cam.Location.y == 0.0 && cam.Location.z == 0.0) return false;

        if (!a.AActor || a.AActor == (uintptr_t)-1) return false;
        if (a.AActor == hero.APawn) return false;
        if (a.UEClassName.empty()) return false;
        if (a.position.x == 0.0 && a.position.y == 0.0 && a.position.z == 0.0) return false;
        if (g_Aimbot.skipFriends && IsFriend(a.AActor)) return false;
        if (a.maxHealth > 0.f && a.health <= 0.f) return false;

        double dx = a.position.x - cam.Location.x;
        double dy = a.position.y - cam.Location.y;
        double dz = a.position.z - cam.Location.z;
        float distM = (float)(std::sqrt(dx * dx + dy * dy + dz * dz) / 100.0);
        if (distM > g_Aimbot.maxDistM) return false;

        float charH = a.CharacterHeight;
        if (std::isnan(charH) || std::isinf(charH) || charH <= 0.f || charH > 1000.f)
            charH = 100.f;

        Vector3 aim = a.position;
        aim.z += (g_Aimbot.aimHeight - 0.5f) * charH;

        Vector2 sp{};
        if (!worldToScreen(aim, cam.Location,
            (float)cam.Rotation.x, (float)cam.Rotation.y, (float)cam.Rotation.z,
            cam.FOV, sw, sh, sp))
            return false;

        if (enforceFov) {
            float cx = sw * 0.5f, cy = sh * 0.5f;
            float ddx = (float)sp.x - cx;
            float ddy = (float)sp.y - cy;
            if (ddx * ddx + ddy * ddy > g_Aimbot.fovPixels * g_Aimbot.fovPixels)
                return false;
        }

        out.actor = a.AActor;
        out.world = aim;
        out.screen = sp;
        out.distM = distM;
        out.charH = charH;
        return true;
    }

    bool FindBestTarget(const LocalManager& hero,
        const std::vector<ActorManager>& actors,
        float sw, float sh, TargetInfo& out)
    {
        out = TargetInfo{};
        const float cx = sw * 0.5f, cy = sh * 0.5f;
        float bestD2 = 1e9f;
        TargetInfo ti;
        for (const auto& a : actors) {
            if (!BuildTargetInfo(a, hero, sw, sh, true, ti)) continue;
            float ddx = (float)ti.screen.x - cx;
            float ddy = (float)ti.screen.y - cy;
            float d2 = ddx * ddx + ddy * ddy;
            if (d2 < bestD2) { bestD2 = d2; out = ti; }
        }
        return out.actor != 0;
    }

    bool FindLockedTarget(const std::vector<ActorManager>& actors,
        const LocalManager& hero,
        float sw, float sh, uintptr_t target, TargetInfo& out)
    {
        out = TargetInfo{};
        if (!target) return false;
        TargetInfo ti;
        for (const auto& a : actors) {
            if (a.AActor != target) continue;
            if (!BuildTargetInfo(a, hero, sw, sh, false, ti)) return false;
            out = ti;
            return true;
        }
        return false;
    }

    bool StartShotFromTarget(const TargetInfo& ti, float sw, float sh)
    {
        double ddx = (ti.screen.x - sw * 0.5) * (double)g_Aimbot.mouseScaleX;
        double ddy = (ti.screen.y - sh * 0.5) * (double)g_Aimbot.mouseScaleY;

        if (std::fabs(ddx) < 1.0 && std::fabs(ddy) < 1.0) return false;

        motor_synergy::config cfg = g_Aimbot.traj;
        if (cfg.target_width <= 1.0) cfg.target_width = 20.0;
        cfg.overshoot_prob = g_Aimbot.overshootProb;

        auto pts = motor_synergy::generate(0.0, 0.0, ddx, ddy, cfg, 0);

        if (g_Aimbot.curveScale != 1.0f) {
            double k = g_Aimbot.curveScale;
            double ex = ddx, ey = ddy;
            for (auto& p : pts) {
                p.x = ex + (p.x - ex) * k;
                p.y = ey + (p.y - ey) * k;
            }
        }
        for (auto& p : pts) p.t *= g_Aimbot.trajSpeedScale;
        if (pts.empty()) return false;

        g_shot.pts = std::move(pts);
        g_shot.idx = 0;
        g_shot.sent_ix = 0;
        g_shot.sent_iy = 0;
        g_shot.t0 = std::chrono::steady_clock::now();
        g_shot.active = true;
        g_shot.target = ti.actor;

        printf("[aimbot] shot target=%p delta=(%.0f,%.0f) pts=%zu%s\n",
            (void*)ti.actor, ddx, ddy, g_shot.pts.size(),
            g_Aimbot.lockTarget ? " [locked]" : "");
        return true;
    }

    void SendFireKey(int vk, bool down)
    {
        switch (vk) {
        case VK_LBUTTON: kmNet_mouse_left(down ? 1 : 0);   break;
        case VK_RBUTTON: kmNet_mouse_right(down ? 1 : 0);  break;
        case VK_MBUTTON: kmNet_mouse_middle(down ? 1 : 0); break;
        default:
            if (down) kmNet_keydown(vk);
            else      kmNet_keyup(vk);
            break;
        }
    }

    void ReleaseFire()
    {
        if (g_trig.keyHeld) {
            SendFireKey(g_Triggerbot.fireKey, false);
            g_trig.keyHeld = false;
        }
    }

    bool IsAnyTargetUnderCrosshair(const LocalManager& hero,
        const std::vector<ActorManager>& actors,
        float sw, float sh,
        float halfW, float halfH,
        float maxDistM,
        bool skipFriends,
        uintptr_t& outTarget)
    {
        outTarget = 0;
        const camera& cam = GetRenderCamera(hero);
        if (cam.FOV < 1.f || cam.FOV > 180.f) return false;
        if (cam.Location.x == 0.0 && cam.Location.y == 0.0 && cam.Location.z == 0.0) return false;

        const float cx = sw * 0.5f, cy = sh * 0.5f;
        float bestD2 = 1e9f;
        uintptr_t best = 0;
        const float heights[2] = { 0.5f, (float)g_Aimbot.aimHeight };

        for (const auto& a : actors) {
            if (!a.AActor || a.AActor == (uintptr_t)-1) continue;
            if (a.AActor == hero.APawn) continue;
            if (a.UEClassName.empty()) continue;
            if (a.position.x == 0.0 && a.position.y == 0.0 && a.position.z == 0.0) continue;
            if (skipFriends && IsFriend(a.AActor)) continue;
            if (a.maxHealth > 0.f && a.health <= 0.f) continue;

            double dx = a.position.x - cam.Location.x;
            double dy = a.position.y - cam.Location.y;
            double dz = a.position.z - cam.Location.z;
            float distM = (float)(std::sqrt(dx * dx + dy * dy + dz * dz) / 100.0);
            if (distM > maxDistM) continue;

            float charH = a.CharacterHeight;
            if (std::isnan(charH) || std::isinf(charH) || charH <= 0.f || charH > 1000.f)
                charH = 100.f;

            for (float h : heights) {
                Vector3 aim = a.position;
                aim.z += (h - 0.5f) * charH;

                Vector2 sp{};
                if (!worldToScreen(aim, cam.Location,
                    (float)cam.Rotation.x, (float)cam.Rotation.y, (float)cam.Rotation.z,
                    cam.FOV, sw, sh, sp))
                    continue;

                float ddx = (float)sp.x - cx;
                float ddy = (float)sp.y - cy;
                if (std::fabs(ddx) > halfW) continue;
                if (std::fabs(ddy) > halfH) continue;

                float d2 = ddx * ddx + ddy * ddy;
                if (d2 < bestD2) { bestD2 = d2; best = a.AActor; }
            }
        }
        outTarget = best;
        return best != 0;
    }

} // namespace

void AimbotReset()
{
    g_shot.active = false;
    g_shot.idx = 0;
    g_shot.pts.clear();
    g_shot.target = 0;
}

void AimbotUpdate()
{
    if (!g_Aimbot.enabled) {
        if (g_mouseBlocked) { kmNet_unmask_all(); g_mouseBlocked = false; }
        g_wasKeyDown = false;
        g_lockedTarget = 0;
        AimbotReset();
        return;
    }
    if (g_Aimbot.aimKey == 0) return;

    bool keyDown = mem.GetKeyboard()->IsKeyDown(g_Aimbot.aimKey);

    if (keyDown && !g_wasKeyDown) {
        if (g_Aimbot.blockMouse) {
            kmNet_mask_mouse_x(1);
            kmNet_mask_mouse_y(1);
            kmNet_mask_mouse_wheel(1);
            g_mouseBlocked = true;
        }
        g_lockedTarget = 0;
    }
    if (!keyDown && g_wasKeyDown) {
        if (g_mouseBlocked) { kmNet_unmask_all(); g_mouseBlocked = false; }
        g_lockedTarget = 0;
        AimbotReset();
    }

    g_wasKeyDown = keyDown;
    if (!keyDown) return;

    if (AppSetting::get().screenWidth <= 0 || AppSetting::get().screenHeight <= 0) {
        AimbotReset(); return;
    }
    const float sw = (float)AppSetting::get().screenWidth;
    const float sh = (float)AppSetting::get().screenHeight;

    auto snap = Entity::Snapshot();
    if (!snap || !snap->hero.APawn) { AimbotReset(); return; }

    const auto& hero = snap->hero;
    const auto& actors = snap->actors;

    if (!g_shot.active) {
        TargetInfo ti;
        bool haveTarget = false;

        if (g_Aimbot.lockTarget && g_lockedTarget != 0) {
            haveTarget = FindLockedTarget(actors, hero, sw, sh, g_lockedTarget, ti);
            if (!haveTarget) g_lockedTarget = 0;
        }
        if (!haveTarget) {
            haveTarget = FindBestTarget(hero, actors, sw, sh, ti);
            if (haveTarget && g_Aimbot.lockTarget) g_lockedTarget = ti.actor;
        }
        if (!haveTarget) return;
        StartShotFromTarget(ti, sw, sh);
        if (!g_shot.active) return;
    }

    if (g_Aimbot.mode == 1) {
        double ex = g_shot.pts.back().x - g_shot.sent_ix;
        double ey = g_shot.pts.back().y - g_shot.sent_iy;
        int ix = (int)std::lround(ex);
        int iy = (int)std::lround(ey);
        if (ix != 0 || iy != 0) {
            kmNet_mouse_move_auto(ix, iy, g_Aimbot.autoMoveMs);
            g_shot.sent_ix += ix;
            g_shot.sent_iy += iy;
        }
        AimbotReset();
        return;
    }

    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(now - g_shot.t0).count();

    int sent = 0;
    while (g_shot.idx < g_shot.pts.size() &&
        g_shot.pts[g_shot.idx].t <= elapsed &&
        sent < g_Aimbot.maxSendsPerFrame)
    {
        const auto& p = g_shot.pts[g_shot.idx];
        int target_ix = (int)std::lround(p.x);
        int target_iy = (int)std::lround(p.y);
        int dx = target_ix - g_shot.sent_ix;
        int dy = target_iy - g_shot.sent_iy;
        if (dx != 0 || dy != 0) {
            kmNet_mouse_move((short)dx, (short)dy);
            g_shot.sent_ix = target_ix;
            g_shot.sent_iy = target_iy;
        }
        ++g_shot.idx;
        ++sent;
    }
    if (g_shot.idx >= g_shot.pts.size()) AimbotReset();
}

void AimbotDraw()
{
    const bool wantFov = g_Aimbot.enabled && g_Aimbot.drawFovCircle;
    const bool wantPreview = g_Aimbot.enabled && g_Aimbot.showAimPoint;
    const bool wantTrigBox = g_Triggerbot.enabled
        && g_Triggerbot.drawTriggerFov
        && g_Triggerbot.activationKey != 0
        && mem.GetKeyboard()->IsKeyDown(g_Triggerbot.activationKey);

    if (!wantFov && !wantPreview && !wantTrigBox) return;
    if (AppSetting::get().screenWidth <= 0 || AppSetting::get().screenHeight <= 0) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    const float sw = (float)AppSetting::get().screenWidth;
    const float sh = (float)AppSetting::get().screenHeight;
    const ImVec2 center(sw * 0.5f, sh * 0.5f);

    if (wantFov)
        dl->AddCircle(center, g_Aimbot.fovPixels, IM_COL32(0, 255, 180, 110), 96, 1.5f);

    if (wantTrigBox) {
        const float hw = g_Triggerbot.triggerWidth * 0.5f;
        const float hh = g_Triggerbot.triggerHeight * 0.5f;
        const ImVec2 a(center.x - hw, center.y - hh);
        const ImVec2 b(center.x + hw, center.y + hh);
        dl->AddRectFilled(a, b, IM_COL32(255, 160, 0, 30));
        dl->AddRect(a, b, IM_COL32(255, 160, 0, 200), 0.0f, 0, 1.5f);
        dl->AddLine(ImVec2(center.x - 4, center.y), ImVec2(center.x + 4, center.y), IM_COL32(255, 160, 0, 180), 1.0f);
        dl->AddLine(ImVec2(center.x, center.y - 4), ImVec2(center.x, center.y + 4), IM_COL32(255, 160, 0, 180), 1.0f);
    }

    if (!wantPreview) return;

    auto snap = Entity::Snapshot();
    if (!snap || !snap->hero.APawn) return;

    const auto& hero = snap->hero;
    const auto& actors = snap->actors;

    TargetInfo ti;
    bool haveTarget = false;
    if (g_lockedTarget != 0) haveTarget = FindLockedTarget(actors, hero, sw, sh, g_lockedTarget, ti);
    if (!haveTarget)         haveTarget = FindBestTarget(hero, actors, sw, sh, ti);
    if (!haveTarget) return;

    const ImVec2 aim((float)ti.screen.x, (float)ti.screen.y);
    const bool isLocked = (g_lockedTarget != 0 && ti.actor == g_lockedTarget);

    const ImU32 lineCol = isLocked ? IM_COL32(255, 200, 40, 220) : IM_COL32(255, 80, 80, 200);
    const ImU32 dotCol = isLocked ? IM_COL32(255, 200, 40, 240) : IM_COL32(255, 40, 40, 220);
    const ImU32 ringCol = isLocked ? IM_COL32(255, 220, 80, 160) : IM_COL32(255, 80, 80, 140);

    dl->AddLine(center, aim, lineCol, 1.5f);
    dl->AddCircleFilled(aim, 4.5f, dotCol, 16);
    dl->AddCircle(aim, 4.5f, IM_COL32(0, 0, 0, 230), 16, 1.5f);
    dl->AddCircle(aim, 9.0f, ringCol, 24, 1.2f);

    char label[96];
    snprintf(label, sizeof(label), "%.0f%% | %.1fm%s",
        g_Aimbot.aimHeight * 100.f, ti.distM, isLocked ? "  [LOCK]" : "");

    ImVec2 ts = ImGui::CalcTextSize(label);
    ImVec2 tp(aim.x - ts.x * 0.5f, aim.y + 14.f);
    dl->AddText(tp, isLocked ? IM_COL32(255, 220, 100, 255) : IM_COL32(255, 180, 180, 255), label);

    if (g_mouseBlocked) {
        const char* warn = "MOUSE BLOCKED";
        ImVec2 ws = ImGui::CalcTextSize(warn);
        ImVec2 wp(sw * 0.5f - ws.x * 0.5f, sh * 0.5f + 40.f);
        dl->AddText(wp, IM_COL32(255, 80, 80, 255), warn);
    }
}

void TriggerbotUpdate()
{
    auto now = std::chrono::steady_clock::now();

    if (!g_Triggerbot.enabled) { ReleaseFire(); g_trig = TriggerState{}; return; }

    bool armed = true;
    if (g_Triggerbot.activationKey != 0)
        armed = mem.GetKeyboard()->IsKeyDown(g_Triggerbot.activationKey);

    if (!armed) { ReleaseFire(); g_trig = TriggerState{}; return; }

    if (AppSetting::get().screenWidth <= 0 || AppSetting::get().screenHeight <= 0) return;
    const float sw = (float)AppSetting::get().screenWidth;
    const float sh = (float)AppSetting::get().screenHeight;

    auto snap = Entity::Snapshot();
    if (!snap || !snap->hero.APawn) return;
    const auto& hero = snap->hero;
    const auto& actors = snap->actors;

    uintptr_t target = 0;
    bool onTarget = IsAnyTargetUnderCrosshair(hero, actors, sw, sh,
        g_Triggerbot.triggerWidth * 0.5f, g_Triggerbot.triggerHeight * 0.5f,
        g_Triggerbot.maxDistM, g_Triggerbot.skipFriends, target);

    if (g_trig.firing) {
        double held = std::chrono::duration<double, std::milli>(now - g_trig.fireStart).count();
        if (held >= g_Triggerbot.holdTimeMs) {
            ReleaseFire();
            g_trig.firing = false;
            g_trig.lastFireEnd = now;
        }
        return;
    }

    if (!onTarget) { g_trig.onTarget = false; return; }

    if (!g_trig.onTarget) { g_trig.targetEnter = now; g_trig.onTarget = true; }

    if (g_Triggerbot.cooldownMs > 0) {
        double since = std::chrono::duration<double, std::milli>(now - g_trig.lastFireEnd).count();
        if (since < g_Triggerbot.cooldownMs) return;
    }

    double onFor = std::chrono::duration<double, std::milli>(now - g_trig.targetEnter).count();
    if (onFor < g_Triggerbot.triggerDelayMs) return;

    SendFireKey(g_Triggerbot.fireKey, true);
    g_trig.keyHeld = true;
    g_trig.firing = true;
    g_trig.fireStart = now;
    g_trig.targetEnter = now;
}