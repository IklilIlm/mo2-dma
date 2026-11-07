#include <functions.h>
#include <entity.h>
#include <MyMath.h>
#include <DMALibrary/Memory/Memory.h>
#include <cstdio>
#include <cstring>
#include <cmath>

FriendSettings g_FriendSettings;

// ============================================================
// Lock-free: ActorFriend читается/пишется только UI-потоком.
// entity_update его не касается вообще.
// ============================================================
bool IsFriend(uintptr_t pActor)
{
    if (!pActor) return false;
    auto& list = Entity::get().ActorFriend;
    for (auto p : list) if (p == pActor) return true;
    return false;
}

void ToggleFriend(uintptr_t pActor)
{
    if (!pActor) return;
    auto& list = Entity::get().ActorFriend;

    for (auto it = list.begin(); it != list.end(); ++it) {
        if (*it == pActor) {
            list.erase(it);
            printf("[friend] - 0x%p\n", (void*)pActor);
            return;
        }
    }
    list.push_back(pActor);
    printf("[friend] + 0x%p\n", (void*)pActor);
}

static void CalcAngles(const Vector3& from, const Vector3& to,
    double& pitch, double& yaw)
{
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    double dz = to.z - from.z;
    double hyp = sqrt(dx * dx + dy * dy);
    yaw = atan2(dy, dx) * 180.0 / PI;
    pitch = atan2(dz, hyp) * 180.0 / PI;
}

static double AngleDiff(double a, double b)
{
    double d = fmod(a - b + 540.0, 360.0) - 180.0;
    return fabs(d);
}

static uintptr_t FindActorUnderCrosshair()
{
    auto snap = Entity::Snapshot();
    if (!snap) return 0;

    const auto& hero = snap->hero;
    const auto& actors = snap->actors;

    const camera& cam = GetRenderCamera(hero);
    if (cam.Location.x == 0.0 && cam.Location.y == 0.0 && cam.Location.z == 0.0)
        return 0;

    const Vector3 camPos = cam.Location;
    const double  camPitch = cam.Rotation.x;
    const double  camYaw = cam.Rotation.y;

    const float maxAngle = g_FriendSettings.maxAngle;
    const float maxDist = g_FriendSettings.maxDistance;
    const bool  playersOnly = g_FriendSettings.playersOnly;

    uintptr_t bestActor = 0;
    double    bestScore = 1e18;

    for (const auto& a : actors)
    {
        if (!a.AActor || a.AActor == (uintptr_t)-1) continue;
        if (a.AActor == hero.APawn) continue;
        if (a.UEClassName.empty()) continue;
        if (a.position.x == 0.0 && a.position.y == 0.0 && a.position.z == 0.0) continue;

        if (playersOnly &&
            a.UEClassName.find("BP_PlayerCharacter_C") == std::string::npos)
            continue;

        double dx = a.position.x - camPos.x;
        double dy = a.position.y - camPos.y;
        double dz = a.position.z - camPos.z;
        double distM = sqrt(dx * dx + dy * dy + dz * dz) / 100.0;
        if (distM > maxDist) continue;

        double tPitch, tYaw;
        CalcAngles(camPos, a.position, tPitch, tYaw);

        double dYaw = AngleDiff(tYaw, camYaw);
        double dPitch = AngleDiff(tPitch, camPitch);
        double angDist = sqrt(dYaw * dYaw + dPitch * dPitch);
        if (angDist > maxAngle) continue;

        if (angDist < bestScore) {
            bestScore = angDist;
            bestActor = a.AActor;
        }
    }

    return bestActor;
}

void FriendsUpdate()
{
    static bool keyWasDown = false;

    if (!g_FriendSettings.enabled) { keyWasDown = false; return; }

    int key = g_FriendSettings.friendKey;
    if (key == 0) return;

    bool keyDown = mem.GetKeyboard()->IsKeyDown(key);

    if (keyDown && !keyWasDown) {
        uintptr_t target = FindActorUnderCrosshair();

        if (target) {
            {
                auto snap = Entity::Snapshot();
                if (snap) {
                    const auto& cam = snap->hero.CameraCachePrivate;
                    for (const auto& a : snap->actors) {
                        if (a.AActor != target) continue;
                        double dx = a.position.x - cam.Location.x;
                        double dy = a.position.y - cam.Location.y;
                        double dz = a.position.z - cam.Location.z;
                        float distM = (float)(sqrt(dx * dx + dy * dy + dz * dz) / 100.0);
                        printf("[friend] target: %s  [%.1fm]\n", a.UEClassName.c_str(), distM);
                        break;
                    }
                }
            }
            ToggleFriend(target);
        }
        else printf("[friend] not looking at anyone\n");
    }

    keyWasDown = keyDown;
}

// ============================================================
// БИНДИНГ
// ============================================================
static bool  g_waitingForKey = false;
static int* g_bindTarget = nullptr;
static bool  g_bindWaitRelease = true;

bool IsWaitingForBind() { return g_waitingForKey; }

bool IsWaitingForBind(int* target) { return g_waitingForKey && (g_bindTarget == target); }

void StartBind(int* target) { g_bindTarget = target; g_waitingForKey = true; g_bindWaitRelease = true; }

void CancelBind() { g_waitingForKey = false; g_bindTarget = nullptr; }

bool PollBindKey()
{
    if (!g_waitingForKey || !g_bindTarget) return false;

    if (g_bindWaitRelease) {
        for (int vk = 1; vk < 256; ++vk)
            if (mem.GetKeyboard()->IsKeyDown(vk)) return false;
        g_bindWaitRelease = false;
        return false;
    }

    for (int vk = 1; vk < 256; ++vk) {
        if (mem.GetKeyboard()->IsKeyDown(vk)) {
            *g_bindTarget = vk;
            g_waitingForKey = false;
            g_bindTarget = nullptr;
            printf("[bind] key -> %d\n", vk);
            return true;
        }
    }
    return false;
}

const char* GetKeyName(int vk, char* buf, size_t bufSize)
{
    switch (vk) {
    case 0:             return "None";
    case VK_LBUTTON:    return "Mouse Left";
    case VK_RBUTTON:    return "Mouse Right";
    case VK_MBUTTON:    return "Mouse Middle";
    case VK_XBUTTON1:   return "Mouse4";
    case VK_XBUTTON2:   return "Mouse5";
    case VK_F1:  return "F1";
    case VK_F2:  return "F2";
    case VK_F3:  return "F3";
    case VK_F4:  return "F4";
    case VK_F5:  return "F5";
    case VK_F6:  return "F6";
    case VK_F7:  return "F7";
    case VK_F8:  return "F8";
    case VK_F9:  return "F9";
    case VK_F10: return "F10";
    case VK_F11: return "F11";
    case VK_F12: return "F12";
    case VK_SPACE:   return "Space";
    case VK_SHIFT:   return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU:    return "Alt";
    case VK_TAB:     return "Tab";
    default:
        sprintf_s(buf, bufSize, "Key %d", vk);
        return buf;
    }
}