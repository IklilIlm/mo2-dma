#include <imgui.h>
#include <entity.h>
#include <MyMath.h>
#include <global.h>
#include <functions.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <unordered_map>

EspSettings g_Esp;

struct ActorCache {
    Vector3 lastPos{};
    bool    hasPos = false;

    bool    espVisible = false;
    bool    radarVisible = false;

    bool    computed = false;
    int     rulesGen = -1;
    bool    isPlayer = false;
    bool    ignored = false;
    const EspClassRule* rule = nullptr;
    const char* icon = nullptr;

    uintptr_t nickData = 0;
    int       nickLen = 0;
    char      nick[128] = {};

    uint64_t lastSeen = 0;
};

static std::unordered_map<uintptr_t, ActorCache> g_actorCache;
static uint64_t g_frameGen = 0;
static int      g_rulesGen = 0;
static int      g_lastRuleCount = -1;
static int      g_lastIgnoreCount = -1;
static bool     g_cacheReserved = false;

ImU32 EspColorU32(const float rgba[4], float alphaMul)
{
    float r = std::clamp(rgba[0], 0.f, 1.f);
    float g = std::clamp(rgba[1], 0.f, 1.f);
    float b = std::clamp(rgba[2], 0.f, 1.f);
    float a = std::clamp(rgba[3] * alphaMul, 0.f, 1.f);
    return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255));
}

static inline ImU32 ApplyAlpha(ImU32 color, float alpha)
{
    if (alpha < 0.f) alpha = 0.f;
    if (alpha > 255.f) alpha = 255.f;
    return (color & 0x00FFFFFF) | (((ImU32)alpha) << 24);
}

// Прозрачность (fade по дистанции) удалена: ESP и radar рисуются
// только с полной альфой, диапазон задаётся Min/Max dist.

static ImU32 GetHealthColor(float hpPercent)
{
    float r, g, b = 0.f;
    if (hpPercent < 0.5f) { r = 1.0f; g = 2.0f * hpPercent; }
    else { r = 2.0f * (1.0f - hpPercent); g = 1.0f; }
    return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
}

static void DrawTextCentered(ImDrawList* dl, float x, float y,
    const char* text, ImU32 color, float scale, ImFont* font = nullptr)
{
    if (!text || !text[0]) return;
    ImFont* baseFont = font ? font : ImGui::GetFont();
    if (!baseFont) return;
    float fsize = baseFont->LegacySize * scale;
    ImVec2 ts = baseFont->CalcTextSizeA(fsize, FLT_MAX, 0.0f, text);
    dl->AddText(baseFont, fsize, ImVec2(x - ts.x * 0.5f, y), color, text);
}

static inline bool IsValidWorldPos(const Vector3& p)
{
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        return false;
    if (std::fabs(p.x) > 1e9 || std::fabs(p.y) > 1e9 || std::fabs(p.z) > 1e9)
        return false;
    if (p.x == 0.0 && p.y == 0.0 && p.z == 0.0)
        return false;
    return true;
}

static const EspClassRule* FindClassRule(const std::string& cls)
{
    for (int i = 0; i < g_Esp.classRuleCount && i < ESP_MAX_CLASS_RULES; ++i) {
        const auto& r = g_Esp.classRules[i];
        if (!r.enabled || r.namePart[0] == '\0') continue;
        if (cls.find(r.namePart) != std::string::npos) return &r;
    }
    return nullptr;
}

static bool IsIgnoredClass(const std::string& cls)
{
    if (cls.empty()) return false;
    for (int i = 0; i < g_Esp.ignoreCount && i < ESP_MAX_IGNORE; ++i) {
        const char* pat = g_Esp.ignoreClasses[i];
        if (!pat || pat[0] == '\0') continue;
        if (cls.find(pat) != std::string::npos) return true;
    }
    return false;
}

static ImU32 GetActorColorRGBA(const EspClassRule* rule, bool isFriend)
{
    const float* c =
        isFriend ? g_Esp.friendColor :
        rule ? rule->color :
        g_Esp.npcColor;

    float r = std::clamp(c[0], 0.f, 1.f);
    float g = std::clamp(c[1], 0.f, 1.f);
    float b = std::clamp(c[2], 0.f, 1.f);
    float a = std::clamp(c[3], 0.f, 1.f);
    return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255));
}

static const char* GetAttackDirectionText(uint8_t t)
{
    switch (t) {
    case 0: return "Bottom";
    case 1: return "Right";
    case 2: return "Left";
    case 3: return "Up";
    default: return nullptr;
    }
}

struct ClassIconRule { const char* exactClass; const char* glyph; };

// ============================================================
// СПРАВКА ПО ГЛИФАМ (иконочный шрифт VerdanaIcon.ttf):
//   0x01B1 (\xC6\xB1) = Barrel      — бочка
//   0x01B2 (\xC6\xB2) = Chest       — сундук
//   0x01B3 (\xC6\xB3) = Box/Crate   — ящик / контейнер
//   0x01B4 (\xC6\xB4) = LootBag     — мешок / труп / ресурсы
//   0x01B5 (\xC6\xB5) = Urn         — ваза / горшок
// Новую иконку добавляешь так: следующий кодпоинт 0x01B6 =
// "\xC6\xB6" (+1 к последнему байту; 0x1BF -> \xC6\xBF; 0x1C0 -> \xC7\x80)
// и строка { "точное_имя_класса_C", "глиф" }.
// ============================================================
static const ClassIconRule kClassIcons[] = {
    { "BP_AvatarStaticMesh_ChestA_Animated_C",         "\xC6\xB2" },   // Chest
    { "BP_AvatarStaticMesh_RadiusChestA_Animated_C",   "\xC6\xB2" },   // Chest
    { "BP_AvatarStaticMesh_BarrelB_C",                 "\xC6\xB1" },   // Barrel
    { "BP_AvatarStaticMesh_Carcass_medium_C",          "\xC6\xB4" },   // LootBag (труп/ресурсы)
    { "BP_AvatarStaticMesh_CrateA_C",                  "\xC6\xB3" },   // Box/Crate
    { "BP_AvatarStaticMesh_LootBagA_C",                "\xC6\xB4" },   // LootBag
    { "BP_AvatarStaticMesh_MelisarUrnA_C",             "\xC6\xB5" },   // Urn
};
static const int kClassIconCount = sizeof(kClassIcons) / sizeof(kClassIcons[0]);

static const char* GetClassIcon(const std::string& cls)
{
    for (int i = 0; i < kClassIconCount; ++i)
        if (cls == kClassIcons[i].exactClass) return kClassIcons[i].glyph;
    return nullptr;
}

static inline void EnsureCacheReserved(size_t actorCount)
{
    if (!g_cacheReserved) {
        g_actorCache.reserve(actorCount + 256);
        g_cacheReserved = true;
    }
}

static inline void RefreshRulesGeneration()
{
    if (g_lastRuleCount != g_Esp.classRuleCount ||
        g_lastIgnoreCount != g_Esp.ignoreCount)
    {
        ++g_rulesGen;
        g_lastRuleCount = g_Esp.classRuleCount;
        g_lastIgnoreCount = g_Esp.ignoreCount;
    }
}

static ActorCache& GetOrUpdateCache(const ActorManager& a)
{
    ActorCache& c = g_actorCache[a.AActor];

    const bool haveName = !a.UEClassName.empty();

    // Классификация фиксируется ТОЛЬКО по бецамеречному (не пустому) имени.
    // Если имя временно не разрешилось (срыв DMA-чтения FName, ci==0):
    //   - для нового актора computed не ставится -> классификация
    //     пересчитается на следующем кадре, раньше залипала навсегда
    //     (до рестарта) — отсюда был "фиолетовый цвет не применяется";
    //   - для уже классифицированного актора прежнее верное состояние
    //     (ignored / rule / isPlayer) сохраняется -> акторы из ignore
    //     списка не светлятся миганием в такие кадры.
    if (haveName && (!c.computed || c.rulesGen != g_rulesGen)) {
        c.isPlayer = (a.UEClassName.find("BP_PlayerCharacter_C") != std::string::npos);
        c.ignored = IsIgnoredClass(a.UEClassName);
        c.rule = FindClassRule(a.UEClassName);
        c.icon = GetClassIcon(a.UEClassName);
        c.rulesGen = g_rulesGen;
        c.computed = true;
    }

    if (IsValidWorldPos(a.position)) {
        c.lastPos = a.position;
        c.hasPos = true;
    }

    if (a.Nickname.Data && a.Nickname.ArrayNum > 0 &&
        a.Nickname.ArrayNum < 62 && a.name[0] != L'\0')
    {
        if (c.nickData != a.Nickname.Data || c.nickLen != a.Nickname.ArrayNum) {
            wcstombs(c.nick, a.name, sizeof(c.nick) - 1);
            c.nickData = a.Nickname.Data;
            c.nickLen = a.Nickname.ArrayNum;
        }
    }
    else {
        c.nickData = 0;
        c.nickLen = 0;
        c.nick[0] = '\0';
    }

    c.lastSeen = g_frameGen;
    return c;
}

static void CleanupCaches()
{
    for (auto it = g_actorCache.begin(); it != g_actorCache.end(); ) {
        if (it->second.lastSeen != g_frameGen) it = g_actorCache.erase(it);
        else ++it;
    }
}

static void DrawCornerBox(ImDrawList* dl, const Vector2& head, const Vector2& feet,
    ImU32 color, float thickness)
{
    float boxHeight = fabsf((float)(head.y - feet.y));
    if (boxHeight <= 0.f || boxHeight > 2000.f) return;

    float boxWidth = boxHeight / 2.5f;
    float centerX = (float)((head.x + feet.x) * 0.5);
    float centerY = (float)((head.y + feet.y) * 0.5);

    float left = centerX - boxWidth * 0.5f;
    float right = centerX + boxWidth * 0.5f;
    float top = centerY - boxHeight * 0.5f;
    float bottom = centerY + boxHeight * 0.5f;

    float cornerLen = std::clamp(boxHeight * 0.09f, 3.f, 50.f);

    dl->AddLine(ImVec2(left, top), ImVec2(left + cornerLen, top), color, thickness);
    dl->AddLine(ImVec2(left, top), ImVec2(left, top + cornerLen), color, thickness);
    dl->AddLine(ImVec2(right, top), ImVec2(right - cornerLen, top), color, thickness);
    dl->AddLine(ImVec2(right, top), ImVec2(right, top + cornerLen), color, thickness);
    dl->AddLine(ImVec2(left, bottom), ImVec2(left + cornerLen, bottom), color, thickness);
    dl->AddLine(ImVec2(left, bottom), ImVec2(left, bottom - cornerLen), color, thickness);
    dl->AddLine(ImVec2(right, bottom), ImVec2(right - cornerLen, bottom), color, thickness);
    dl->AddLine(ImVec2(right, bottom), ImVec2(right, bottom - cornerLen), color, thickness);
}

static void DrawHealthBar(ImDrawList* dl, const Vector2& head, const Vector2& feet,
    float health, float maxHealth, float alpha)
{
    if (maxHealth <= 0.f) return;
    float boxHeight = fabsf((float)(head.y - feet.y));
    if (boxHeight <= 0.f || boxHeight > 2000.f) return;

    float boxWidth = boxHeight / 2.5f;
    float centerX = (float)((head.x + feet.x) * 0.5);
    float centerY = (float)((head.y + feet.y) * 0.5);

    float left = centerX - boxWidth * 0.5f;
    float top = centerY - boxHeight * 0.5f;
    float bottom = centerY + boxHeight * 0.5f;

    float barWidth = std::clamp(boxWidth * 0.12f, 2.0f, 5.0f);
    float barX = left - barWidth - 4.0f;

    float hpPct = std::clamp(health / maxHealth, 0.0f, 1.0f);
    float filled = (bottom - top) * hpPct;
    if (filled > 0.f)
        dl->AddLine(ImVec2(barX, bottom - filled), ImVec2(barX, bottom),
            ApplyAlpha(GetHealthColor(hpPct), alpha), barWidth);
}

static void DrawDot(ImDrawList* dl, float x, float y, float size, ImU32 color)
{
    dl->AddCircleFilled(ImVec2(x, y), size, color, 12);
}

static void DrawRadarInline(ImDrawList* dl,
    const LocalManager& hero, const std::vector<ActorManager>& actors)
{
    if (!g_Esp.radarEnabled) return;

    const camera& cam = GetRenderCamera(hero);
    if (cam.Location.x == 0.0 && cam.Location.y == 0.0 && cam.Location.z == 0.0) return;

    const float size = g_Esp.radarSize;
    const float half = size * 0.5f;
    const float left = g_Esp.radarPosX;
    const float top = g_Esp.radarPosY;
    const float right = left + size;
    const float bottom = top + size;
    const float cx = left + half;
    const float cy = top + half;

    dl->AddRectFilled(ImVec2(left, top), ImVec2(right, bottom), EspColorU32(g_Esp.radarBgColor));

    if (g_Esp.radarShowRing)
        dl->AddRect(ImVec2(left, top), ImVec2(right, bottom),
            EspColorU32(g_Esp.radarBorderColor), 0.0f, 0, 1.5f);

    if (g_Esp.radarShowAxes) {
        dl->AddLine(ImVec2(cx, top), ImVec2(cx, bottom), EspColorU32(g_Esp.radarAxisColor), 1.0f);
        dl->AddLine(ImVec2(left, cy), ImVec2(right, cy), EspColorU32(g_Esp.radarAxisColor), 1.0f);
    }

    for (int i = 0; i < g_Esp.radarRingCount && i < 3; ++i) {
        float r = half * g_Esp.radarRingSizes[i];
        dl->AddRect(ImVec2(cx - r, cy - r), ImVec2(cx + r, cy + r),
            EspColorU32(g_Esp.radarRingColor), 0.0f, 0, 1.0f);
    }

    if (g_Esp.radarShowPlayer)
        dl->AddCircleFilled(ImVec2(cx, cy), g_Esp.radarPlayerDotSize,
            EspColorU32(g_Esp.radarPlayerColor), 12);

    const float yawRad = (float)cam.Rotation.y * (3.14159265f / 180.0f);
    const float sinY = sinf(yawRad);
    const float cosY = cosf(yawRad);
    const float scale = half / g_Esp.radarWorldSize;
    const Vector3 myPos = cam.Location;

    const float dotR = g_Esp.radarDotSize;

    auto drawRadarDot = [&](const ActorManager& actor, ActorCache& c)
        {
            if (!actor.AActor || actor.AActor == (uintptr_t)-1) return;
            if (actor.AActor == hero.APawn) return;
            if (c.ignored) return;
            if (!c.hasPos) return;

            const Vector3& usePos = c.lastPos;

            float dx = (float)(usePos.x - myPos.x);
            float dy = (float)(usePos.y - myPos.y);
            float dist = sqrtf(dx * dx + dy * dy);

            const bool wasVisible = c.radarVisible;

            float minD = g_Esp.radarMinDist;
            float maxD = g_Esp.radarMaxDist;
            if (wasVisible) {
                minD *= 0.90f;
                maxD *= 1.10f;
            }

            if (dist < minD || dist > maxD) { c.radarVisible = false; return; }

            const bool isFriend = IsFriend(actor.AActor);
            if (isFriend && !g_Esp.radarShowFriends) { c.radarVisible = false; return; }
            if (!isFriend && !g_Esp.radarShowEnemies) { c.radarVisible = false; return; }

            float rotX = dx * cosY + dy * sinY;
            float rotY = -dx * sinY + dy * cosY;
            float rx = cx + rotY * scale;
            float ry = cy - rotX * scale;

            float slack = wasVisible ? dotR * 2.0f : 0.0f;

            if (rx < left - slack || rx > right + slack ||
                ry < top - slack || ry > bottom + slack)
            {
                c.radarVisible = false;
                return;
            }

            float drawX = std::clamp(rx, left + dotR, right - dotR);
            float drawY = std::clamp(ry, top + dotR, bottom - dotR);

            c.radarVisible = true;

            ImU32 baseCol;
            if (g_Esp.radarUseEspColors) {
                baseCol = GetActorColorRGBA(c.rule, isFriend);
            }
            else {
                const float* col = isFriend ? g_Esp.radarFriendColor : g_Esp.radarEnemyColor;
                baseCol = IM_COL32(
                    (int)(std::clamp(col[0], 0.f, 1.f) * 255),
                    (int)(std::clamp(col[1], 0.f, 1.f) * 255),
                    (int)(std::clamp(col[2], 0.f, 1.f) * 255),
                    (int)(std::clamp(col[3], 0.f, 1.f) * 255));
            }

            const ImU32 col = baseCol;
            dl->AddCircleFilled(ImVec2(drawX, drawY), dotR, col, 12);
        };

    for (const auto& actor : actors) {
        if (!actor.AActor || actor.AActor == (uintptr_t)-1) continue;
        if (actor.AActor == hero.APawn) continue;
        ActorCache& c = GetOrUpdateCache(actor);
        if (c.ignored || c.isPlayer) continue;
        drawRadarDot(actor, c);
    }
    for (const auto& actor : actors) {
        auto it = g_actorCache.find(actor.AActor);
        if (it == g_actorCache.end()) continue;
        ActorCache& c = it->second;
        if (c.ignored || !c.isPlayer) continue;
        drawRadarDot(actor, c);
    }
}

static void DrawMyCoords(ImDrawList* dl,
    const LocalManager& hero, const std::vector<ActorManager>& actors)
{
    if (!g_Esp.showMyCoords) return;

    Vector3 myPos{};
    bool foundMe = false;
    for (const auto& actor : actors) {
        if (actor.AActor == hero.APawn) { myPos = actor.position; foundMe = true; break; }
    }
    if (!foundMe) myPos = GetRenderCamera(hero).Location;

    float bottomEdge = g_Esp.radarEnabled
        ? g_Esp.radarPosY + g_Esp.radarSize
        : g_Esp.radarPosY + 20.0f;

    float centerX = g_Esp.radarPosX + g_Esp.radarSize * 0.5f;

    char coordBuf[160];
    snprintf(coordBuf, sizeof(coordBuf), "X: %.0f  Y: %.0f  Z: %.0f", myPos.x, myPos.y, myPos.z);

    ImVec2 ts = ImGui::CalcTextSize(coordBuf);
    dl->AddText(ImVec2(centerX - ts.x * 0.5f, bottomEdge + 4.0f),
        EspColorU32(g_Esp.myCoordsColor, 1.0f), coordBuf);
}

void wallhack()
{
    auto snap = Entity::Snapshot();
    if (!snap) return;

    const auto& hero = snap->hero;
    const auto& actors = snap->actors;
    const camera& cam = GetRenderCamera(hero);

    if (AppSetting::get().screenWidth <= 0 || AppSetting::get().screenHeight <= 0) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    EnsureCacheReserved(actors.size());
    RefreshRulesGeneration();
    ++g_frameGen;

    DrawRadarInline(dl, hero, actors);
    DrawMyCoords(dl, hero, actors);

    if (!g_Esp.enabled) { CleanupCaches(); return; }
    if (actors.empty()) { CleanupCaches(); return; }
    if (cam.Location.x == 0.0 && cam.Location.y == 0.0 && cam.Location.z == 0.0) { CleanupCaches(); return; }
    if (cam.FOV < 1.0f || cam.FOV > 180.0f) { CleanupCaches(); return; }

    const float screenW = (float)AppSetting::get().screenWidth;
    const float screenH = (float)AppSetting::get().screenHeight;
    const Vector3 myPos = cam.Location;
    const uintptr_t heroPawn = hero.APawn;

    const float camRotX = (float)cam.Rotation.x;
    const float camRotY = (float)cam.Rotation.y;
    const float camRotZ = (float)cam.Rotation.z;
    const float camFOV = cam.FOV;

    const float appearDist = g_Esp.maxDrawDist;
    const float hideDist = g_Esp.maxDrawDist * 1.08f;
    const float minDrawDist = g_Esp.nearDist;   // мин. дальность отрисовки (м)

    const float dotSize = g_Esp.dotSize;
    const float iconScale = g_Esp.iconScale;
    const float textScale = g_Esp.textScale;
    const float atkScale = g_Esp.atkTextScale;
    const float boxThick = g_Esp.boxThickness;

    for (const auto& actor : actors)
    {
        if (!actor.AActor || actor.AActor == (uintptr_t)-1) continue;
        if (actor.AActor == heroPawn) continue;

        ActorCache& c = GetOrUpdateCache(actor);
        if (c.ignored) continue;
        if (!c.hasPos) continue;

        const Vector3& usePos = c.lastPos;

        const float dx = (float)(usePos.x - myPos.x);
        const float dy = (float)(usePos.y - myPos.y);
        const float dz = (float)(usePos.z - myPos.z);
        const float distM = sqrtf(dx * dx + dy * dy + dz * dz) / 100.0f;

        const float effectiveMax = c.espVisible ? hideDist : appearDist;
        if (distM > effectiveMax) { c.espVisible = false; continue; }
        if (distM < minDrawDist)  { c.espVisible = false; continue; }
        c.espVisible = true;

        float charHeight = actor.CharacterHeight;
        if (std::isnan(charHeight) || std::isinf(charHeight) || charHeight <= 0.f || charHeight > 1000.f)
            charHeight = 100.f;

        Vector3 headPos = usePos; headPos.z += charHeight * 0.5;
        Vector3 feetPos = usePos; feetPos.z -= charHeight * 0.5;

        Vector2 headScreen{}, feetScreen{};
        bool headOn = worldToScreen(headPos, cam.Location, camRotX, camRotY, camRotZ,
            camFOV, screenW, screenH, headScreen);
        bool feetOn = worldToScreen(feetPos, cam.Location, camRotX, camRotY, camRotZ,
            camFOV, screenW, screenH, feetScreen);

        if (!headOn && !feetOn) continue;

        const bool isFriend = IsFriend(actor.AActor);
        const bool isPlayer = c.isPlayer;

        const EspClassRule* rule = c.rule;
        const bool hasRule = (rule != nullptr);

        const ImU32 boxColor = GetActorColorRGBA(rule, isFriend);
        const ImU32 textColor = EspColorU32(g_Esp.textColor);

        const bool effDrawBox = hasRule ? rule->drawBox : g_Esp.drawBox;
        const bool effDrawHPBar = hasRule ? rule->drawHealthBar : g_Esp.drawHealthBar;
        const bool effDrawHPText = hasRule ? rule->drawHealthText : g_Esp.drawHealthText;
        const bool effDrawAttack = hasRule ? rule->drawAttack : g_Esp.drawAttack;
        const bool effDrawFName = hasRule ? rule->drawFName : g_Esp.drawFName;
        const bool effDrawNick = hasRule ? rule->drawNick : g_Esp.drawNick;
        const bool effDrawDistance = hasRule ? rule->drawDistance : g_Esp.drawDistance;
        const bool effDrawIcons = hasRule ? rule->drawIcons : g_Esp.drawIcons;

        const bool drawPlayerBox = (isPlayer && effDrawBox && headOn && feetOn);
        const bool drawNpcDot = (!isPlayer && effDrawBox && headOn);

        if (drawPlayerBox) {
            DrawCornerBox(dl, headScreen, feetScreen, boxColor, boxThick);
        }
        else if (drawNpcDot) {
            DrawDot(dl, (float)headScreen.x, (float)headScreen.y, dotSize, boxColor);
        }

        if (effDrawHPBar && headOn && feetOn && actor.maxHealth > 0.f)
            DrawHealthBar(dl, headScreen, feetScreen, actor.health, actor.maxHealth, 255.f);

        if (!headOn) continue;

        const float baseX = (float)headScreen.x;

        if (effDrawAttack) {
            const char* atk = GetAttackDirectionText(actor.TypeAttack);
            if (atk) {
                ImU32 atkColor = EspColorU32(g_Esp.atkColor);
                ImFont* font = ImGui::GetFont();
                if (font) {
                    float fsize = font->LegacySize * textScale * atkScale;
                    ImVec2 ts = font->CalcTextSizeA(fsize, FLT_MAX, 0.0f, atk);
                    dl->AddText(font, fsize,
                        ImVec2(baseX - ts.x * 0.5f, (float)headScreen.y - 55.0f),
                        atkColor, atk);
                }
            }
        }

        if (effDrawFName && !actor.UEClassName.empty())
            DrawTextCentered(dl, baseX, (float)headScreen.y - 85.0f + 8.0f,
                actor.UEClassName.c_str(), textColor, textScale);

        const bool hasNick = (c.nick[0] != '\0');

        float belowY;
        if (drawPlayerBox) {
            belowY = (float)feetScreen.y + 10.0f;
        }
        else if (drawNpcDot) {
            belowY = (float)headScreen.y + dotSize + 4.0f;
        }
        else {
            belowY = feetOn ? (float)feetScreen.y + 10.0f : (float)headScreen.y + dotSize + 4.0f;
        }

        // ---- 1) иконка + ник ----
        const char* icon = (effDrawIcons && c.icon) ? c.icon : nullptr;
        const bool  showIcon = (icon && g_font);
        const bool  showNick = (effDrawNick && hasNick);

        if (showIcon && showNick) {
            const float iconSize = g_font->LegacySize * iconScale;
            ImVec2 iconTs = g_font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, icon);
            ImVec2 nickTs = ImGui::CalcTextSize(c.nick);
            float totalW = iconTs.x + 6.0f + nickTs.x;
            float startX = baseX - totalW * 0.5f;

            dl->AddText(g_font, iconSize, ImVec2(startX, belowY - 2), textColor, icon);
            DrawTextCentered(dl, startX + iconTs.x + 6.0f + nickTs.x * 0.5f,
                belowY, c.nick, textColor, textScale);

            belowY += iconSize + 4.0f;
        }
        else if (showIcon) {
            const float iconSize = g_font->LegacySize * iconScale;
            ImVec2 iconTs = g_font->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, icon);
            dl->AddText(g_font, iconSize,
                ImVec2(baseX - iconTs.x * 0.5f, belowY - 2), textColor, icon);

            belowY += iconSize + 4.0f;
        }
        else if (showNick) {
            DrawTextCentered(dl, baseX, belowY, c.nick, textColor, textScale);
            belowY += 15.0f * textScale;
        }

        // ---- 2) метры ----
        if (effDrawDistance) {
            char distBuf[32];
            if (distM >= 1000.f) snprintf(distBuf, sizeof(distBuf), "%.2fkm", distM / 1000.f);
            else                 snprintf(distBuf, sizeof(distBuf), "%.1fm", distM);
            DrawTextCentered(dl, baseX, belowY, distBuf, textColor, textScale);
            belowY += 15.0f * textScale;
        }

        // ---- 3) HP текст ----
        bool showHpText = effDrawHPText && (!g_Esp.hpTextOnlyPlayers || isPlayer);
        if (showHpText && actor.maxHealth > 0.f) {
            char hpBuf[48];
            snprintf(hpBuf, sizeof(hpBuf), "HP: %.0f / %.0f", actor.health, actor.maxHealth);
            DrawTextCentered(dl, baseX, belowY, hpBuf, textColor, textScale);
        }
    }

    CleanupCaches();
}