#include <functions.h>
#include <entity.h>
#include <DMALibrary/Memory/Memory.h>
#include "createcharacter.h"
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>

CreateCharSettings g_CreateChar{};
std::vector<std::wstring> g_BloodlineNames{};
CreateCharDiag g_CreateCharDiag{};

// Буфер для строк, которые пишем в SelectedOption комбобокса (должен жить до нажатия "Создать").
static std::wstring g_BloodlineWriteBuf[6];

static inline void WriteOptI32(uintptr_t addr, int32_t off, int32_t v) {
    if (v >= 0) mem.Write<int32_t>(addr + off, v);
}
static inline void WriteOptU8(uintptr_t addr, int32_t off, int32_t v) {
    if (v >= 0) mem.Write<uint8_t>(addr + off, (uint8_t)v);
}

// ---- Цепочка до контроллера создания ----
static uintptr_t CreateCharGetController()
{
    // Кэшируем базовый адрес, иначе GetBaseDaddy() спамит в консоль каждый кадр.
    static uintptr_t s_cachedBase = 0;
    if (!s_cachedBase || s_cachedBase == (uintptr_t)-1)
        s_cachedBase = mem.GetBaseDaddy(EXE_NAME);
    uintptr_t Base = s_cachedBase;
    if (!Base || Base == (uintptr_t)-1) return 0;

    uintptr_t UWorld = mem.Read<uintptr_t>(Base + offset_GWorld);
    if (!UWorld || UWorld == (uintptr_t)-1) return 0;

    uintptr_t gameMode = mem.Read<uintptr_t>(UWorld + offset_AuthorityGameMode);
    if (!gameMode || gameMode == (uintptr_t)-1) return 0;

    uintptr_t loginHUD = mem.Read<uintptr_t>(gameMode + offset_GameModeMenu_pLoginHUD);
    if (!loginHUD || loginHUD == (uintptr_t)-1) return 0;

    uintptr_t creation = mem.Read<uintptr_t>(loginHUD + offset_LoginHUD_pCreation);
    if (!creation || creation == (uintptr_t)-1) return 0;

    return creation;
}

static uintptr_t CreateCharGetAppearanceAddr()
{
    uintptr_t creation = CreateCharGetController();
    if (!creation) return 0;
    return creation + offset_CharCreation_charAppearance;
}

static uintptr_t CreateCharGetSpeciesPanel()
{
    uintptr_t creation = CreateCharGetController();
    if (!creation) return 0;
    return mem.Read<uintptr_t>(creation + offset_CharCreation_pPanelSpecies);
}

static uintptr_t CreateCharGetAttributesPanel()
{
    uintptr_t creation = CreateCharGetController();
    if (!creation) return 0;
    return mem.Read<uintptr_t>(creation + offset_CharCreation_pPanelAttributes);
}

static uintptr_t CreateCharGetBloodlinePanel()
{
    uintptr_t creation = CreateCharGetController();
    if (!creation) return 0;
    return mem.Read<uintptr_t>(creation + offset_CharCreation_pPanelBloodline);
}

bool CreateCharIsAvailable()
{
    return CreateCharGetAppearanceAddr() != 0;
}

// ---- Карусели ----
static void SpeciesCarouselWrite(uintptr_t panel, uintptr_t off, int32_t idx)
{
    if (!panel || idx < 0) return;
    uintptr_t c = mem.Read<uintptr_t>(panel + off);
    if (!c || c == (uintptr_t)-1) return;
    mem.Write<int32_t>(c + offset_CarouselText_iCurrentIndex, idx);
}
static void AttrCarouselWrite(uintptr_t panel, uintptr_t off, int32_t val)
{
    if (!panel || val < 0) return;
    uintptr_t c = mem.Read<uintptr_t>(panel + off);
    if (!c || c == (uintptr_t)-1) return;
    mem.Write<int32_t>(c + offset_CarouselValue_iCurrentValue, val);
}
static int32_t SpeciesCarouselRead(uintptr_t panel, uintptr_t off)
{
    if (!panel) return -1;
    uintptr_t c = mem.Read<uintptr_t>(panel + off);
    if (!c || c == (uintptr_t)-1) return -1;
    return mem.Read<int32_t>(c + offset_CarouselText_iCurrentIndex);
}
static int32_t AttrCarouselRead(uintptr_t panel, uintptr_t off)
{
    if (!panel) return -1;
    uintptr_t c = mem.Read<uintptr_t>(panel + off);
    if (!c || c == (uintptr_t)-1) return -1;
    return mem.Read<int32_t>(c + offset_CarouselValue_iCurrentValue);
}

// ---- Bloodline: чтение/запись SelectedOption комбобокса ----
static uintptr_t BloodlineNative(uintptr_t panel, uintptr_t ancestryOffset)
{
    if (!panel) return 0;
    uintptr_t wrapper = mem.Read<uintptr_t>(panel + ancestryOffset);
    if (!wrapper) return 0;
    return mem.Read<uintptr_t>(wrapper + offset_MOComboBox_pMOComboBoxString);
}

static std::wstring BloodlineReadSelected(uintptr_t native)
{
    if (!native) return L"";
    uintptr_t selFstring = native + offset_ComboBox_SelectedOption;
    uintptr_t dataPtr = mem.Read<uintptr_t>(selFstring + 0);
    int32_t  num      = mem.Read<int32_t>(selFstring + 8);
    if (!dataPtr || num <= 0 || num > 256) return L"";
    std::wstring s;
    s.resize(num);
    mem.Read(dataPtr, s.data(), (size_t)num * sizeof(wchar_t));
    s.resize(num);
    return s;
}

static void BloodlineWriteSelected(uintptr_t native, const std::wstring& text, std::wstring& buf)
{
    if (!native || text.empty()) return;
    buf = text;   // держим буфер валидным (статический массив g_BloodlineWriteBuf)
    uintptr_t fstringAddr = native + offset_ComboBox_SelectedOption;
    size_t len = text.size();
    mem.Write<uintptr_t>(fstringAddr + 0, (uintptr_t)buf.data());
    mem.Write<int32_t>(fstringAddr + 8, (int32_t)len);
    mem.Write<int32_t>(fstringAddr + 12, (int32_t)len);
}

static uintptr_t BloodlineOffsets(int i)
{
    static const uintptr_t offs[6] = {
        offset_Bloodline_Ancestry0, offset_Bloodline_Ancestry1, offset_Bloodline_Ancestry2,
        offset_Bloodline_Ancestry3, offset_Bloodline_Ancestry4, offset_Bloodline_Ancestry5
    };
    return offs[i];
}


// ============================================================
// READ: читает текущие значения из игры в g_CreateChar
// ============================================================
bool CreateCharRead()
{
    uintptr_t appearance = CreateCharGetAppearanceAddr();
    if (!appearance) return false;

    g_CreateChar.length       = mem.Read<float>(appearance + offset_Appearance_Length);
    g_CreateChar.bodySize     = mem.Read<float>(appearance + offset_Appearance_BodySize);
    g_CreateChar.bodyStrength = mem.Read<float>(appearance + offset_Appearance_BodyStrength);

    g_CreateChar.bodySpecies = mem.Read<uint8_t>(appearance + offset_Body_ESpecies);
    g_CreateChar.bodySex     = mem.Read<uint8_t>(appearance + offset_Body_ESex);
    g_CreateChar.bodyAge     = mem.Read<int32_t>(appearance + offset_Body_iAge);
    g_CreateChar.bodyColor   = mem.Read<int32_t>(appearance + offset_Body_iColor);
    g_CreateChar.ancestry1   = mem.Read<int32_t>(appearance + offset_Body_vAncestry1);
    g_CreateChar.ancestry2   = mem.Read<int32_t>(appearance + offset_Body_vAncestry2);
    g_CreateChar.ancestry3   = mem.Read<int32_t>(appearance + offset_Body_vAncestry3);
    g_CreateChar.ancestry4   = mem.Read<int32_t>(appearance + offset_Body_vAncestry4);

    g_CreateChar.hairType     = mem.Read<int32_t>(appearance + offset_Dec_iHairTypeIndex);
    g_CreateChar.hairColor    = mem.Read<int32_t>(appearance + offset_Dec_iHairColorIndex);
    g_CreateChar.beardType    = mem.Read<int32_t>(appearance + offset_Dec_iBeardTypeIndex);
    g_CreateChar.beardColor   = mem.Read<int32_t>(appearance + offset_Dec_iBeardColorIndex);
    g_CreateChar.skinColor    = mem.Read<int32_t>(appearance + offset_Dec_iSkinColorIndex);
    g_CreateChar.eyeColor     = mem.Read<int32_t>(appearance + offset_Dec_iEyeColorIndex);
    g_CreateChar.skinDecoA    = mem.Read<int32_t>(appearance + offset_Dec_iSkinDecorationIndexA);
    g_CreateChar.skinDecoB    = mem.Read<int32_t>(appearance + offset_Dec_iSkinDecorationIndexB);
    g_CreateChar.skinDecoC    = mem.Read<int32_t>(appearance + offset_Dec_iSkinDecorationIndexC);
    g_CreateChar.faceTattooType = mem.Read<int32_t>(appearance + offset_Dec_iFaceTattooTypeIndex);
    g_CreateChar.faceTattoo     = mem.Read<int32_t>(appearance + offset_Dec_iFaceTattooIndex);
    g_CreateChar.bodyTattooType = mem.Read<int32_t>(appearance + offset_Dec_iBodyTattooTypeIndex);
    g_CreateChar.bodyTattoo     = mem.Read<int32_t>(appearance + offset_Dec_iBodyTattooIndex);

    uintptr_t species = CreateCharGetSpeciesPanel();
    if (species) {
        g_CreateChar.race   = SpeciesCarouselRead(species, offset_SpeciesPanel_Race);
        g_CreateChar.gender = SpeciesCarouselRead(species, offset_SpeciesPanel_Gender);
        g_CreateChar.clade  = SpeciesCarouselRead(species, offset_SpeciesPanel_Clade);
    }

    uintptr_t attrs = CreateCharGetAttributesPanel();
    if (attrs) {
        g_CreateChar.attrStr  = AttrCarouselRead(attrs, offset_AttributesPanel_Str);
        g_CreateChar.attrDex  = AttrCarouselRead(attrs, offset_AttributesPanel_Dex);
        g_CreateChar.attrCon  = AttrCarouselRead(attrs, offset_AttributesPanel_Con);
        g_CreateChar.attrInt  = AttrCarouselRead(attrs, offset_AttributesPanel_Int);
        g_CreateChar.attrPsy  = AttrCarouselRead(attrs, offset_AttributesPanel_Psy);
        g_CreateChar.attrAge  = AttrCarouselRead(attrs, offset_AttributesPanel_Age);
        g_CreateChar.attrSize = AttrCarouselRead(attrs, offset_AttributesPanel_Size);
        g_CreateChar.attrPoints = mem.Read<int32_t>(attrs + offset_AttributesPanel_iTotalAttrPoints);
    }

    uintptr_t bloodline = CreateCharGetBloodlinePanel();
    g_BloodlineNames.clear();
    std::wstring* sel[6] = {
        &g_CreateChar.bloodline0, &g_CreateChar.bloodline1, &g_CreateChar.bloodline2,
        &g_CreateChar.bloodline3, &g_CreateChar.bloodline4, &g_CreateChar.bloodline5
    };
    if (bloodline) {
        uintptr_t arrData = mem.Read<uintptr_t>(bloodline + offset_Bloodline_vPossibleBloodlines + 0);
        int32_t  arrCount = mem.Read<int32_t>(bloodline + offset_Bloodline_vPossibleBloodlines + 8);
        if (arrData && arrCount > 0 && arrCount < 4096) {
            for (int i = 0; i < arrCount; i++) {
                uintptr_t fstring = mem.Read<uintptr_t>(arrData + i * 16);
                if (!fstring) continue;
                uintptr_t dataPtr = mem.Read<uintptr_t>(fstring + 0);
                int32_t  numLen   = mem.Read<int32_t>(fstring + 8);
                if (!dataPtr || numLen <= 0 || numLen > 256) continue;
                std::wstring s;
                s.resize(numLen);
                mem.Read(dataPtr, s.data(), (size_t)numLen * sizeof(wchar_t));
                g_BloodlineNames.push_back(std::move(s));
            }
        }
        for (int i = 0; i < 6; i++)
            *sel[i] = BloodlineReadSelected(BloodlineNative(bloodline, BloodlineOffsets(i)));
    }

    return true;
}


// ============================================================
// Ядро записи: пишет всё заданное в g_CreateChar в память.
// Возвращает кол-во записанных параметров (0 = экран не открыт).
// ============================================================
static int CreateCharWriteAll()
{
    uintptr_t appearance = CreateCharGetAppearanceAddr();
    if (!appearance) return 0;

    int written = 0;

    // Телосложение — пишем всегда (слайдеры превью)
    mem.Write<float>(appearance + offset_Appearance_Length,       g_CreateChar.length);       written++;
    mem.Write<float>(appearance + offset_Appearance_BodySize,     g_CreateChar.bodySize);     written++;
    mem.Write<float>(appearance + offset_Appearance_BodyStrength, g_CreateChar.bodyStrength); written++;

    // BodyData
    if (g_CreateChar.bodySpecies >= 0) { WriteOptU8(appearance, offset_Body_ESpecies, g_CreateChar.bodySpecies); written++; }
    if (g_CreateChar.bodySex     >= 0) { WriteOptU8(appearance, offset_Body_ESex,     g_CreateChar.bodySex);     written++; }
    if (g_CreateChar.bodyAge     >= 0) { WriteOptI32(appearance, offset_Body_iAge,     g_CreateChar.bodyAge);     written++; }
    if (g_CreateChar.bodyColor   >= 0) { WriteOptI32(appearance, offset_Body_iColor,   g_CreateChar.bodyColor);   written++; }
    if (g_CreateChar.ancestry1   >= 0) { WriteOptI32(appearance, offset_Body_vAncestry1, g_CreateChar.ancestry1); written++; }
    if (g_CreateChar.ancestry2   >= 0) { WriteOptI32(appearance, offset_Body_vAncestry2, g_CreateChar.ancestry2); written++; }
    if (g_CreateChar.ancestry3   >= 0) { WriteOptI32(appearance, offset_Body_vAncestry3, g_CreateChar.ancestry3); written++; }
    if (g_CreateChar.ancestry4   >= 0) { WriteOptI32(appearance, offset_Body_vAncestry4, g_CreateChar.ancestry4); written++; }

    // DecorationData
    if (g_CreateChar.hairType     >= 0) { WriteOptI32(appearance, offset_Dec_iHairTypeIndex,     g_CreateChar.hairType);     written++; }
    if (g_CreateChar.hairColor    >= 0) { WriteOptI32(appearance, offset_Dec_iHairColorIndex,    g_CreateChar.hairColor);    written++; }
    if (g_CreateChar.beardType    >= 0) { WriteOptI32(appearance, offset_Dec_iBeardTypeIndex,    g_CreateChar.beardType);    written++; }
    if (g_CreateChar.beardColor   >= 0) { WriteOptI32(appearance, offset_Dec_iBeardColorIndex,   g_CreateChar.beardColor);   written++; }
    if (g_CreateChar.skinColor    >= 0) { WriteOptI32(appearance, offset_Dec_iSkinColorIndex,    g_CreateChar.skinColor);    written++; }
    if (g_CreateChar.eyeColor     >= 0) { WriteOptI32(appearance, offset_Dec_iEyeColorIndex,     g_CreateChar.eyeColor);     written++; }
    if (g_CreateChar.skinDecoA    >= 0) { WriteOptI32(appearance, offset_Dec_iSkinDecorationIndexA, g_CreateChar.skinDecoA); written++; }
    if (g_CreateChar.skinDecoB    >= 0) { WriteOptI32(appearance, offset_Dec_iSkinDecorationIndexB, g_CreateChar.skinDecoB); written++; }
    if (g_CreateChar.skinDecoC    >= 0) { WriteOptI32(appearance, offset_Dec_iSkinDecorationIndexC, g_CreateChar.skinDecoC); written++; }
    if (g_CreateChar.faceTattooType >= 0) { WriteOptI32(appearance, offset_Dec_iFaceTattooTypeIndex, g_CreateChar.faceTattooType); written++; }
    if (g_CreateChar.faceTattoo     >= 0) { WriteOptI32(appearance, offset_Dec_iFaceTattooIndex,     g_CreateChar.faceTattoo);     written++; }
    if (g_CreateChar.bodyTattooType >= 0) { WriteOptI32(appearance, offset_Dec_iBodyTattooTypeIndex, g_CreateChar.bodyTattooType); written++; }
    if (g_CreateChar.bodyTattoo     >= 0) { WriteOptI32(appearance, offset_Dec_iBodyTattooIndex,     g_CreateChar.bodyTattoo);     written++; }

    // Species-карусели
    uintptr_t species = CreateCharGetSpeciesPanel();
    if (species) {
        if (g_CreateChar.race   >= 0) { SpeciesCarouselWrite(species, offset_SpeciesPanel_Race,   g_CreateChar.race);   written++; }
        if (g_CreateChar.gender >= 0) { SpeciesCarouselWrite(species, offset_SpeciesPanel_Gender, g_CreateChar.gender); written++; }
        if (g_CreateChar.clade  >= 0) { SpeciesCarouselWrite(species, offset_SpeciesPanel_Clade,  g_CreateChar.clade);  written++; }
    }

    // Attributes-карусели
    uintptr_t attrs = CreateCharGetAttributesPanel();
    if (attrs) {
        if (g_CreateChar.attrStr  >= 0) { AttrCarouselWrite(attrs, offset_AttributesPanel_Str,  g_CreateChar.attrStr);  written++; }
        if (g_CreateChar.attrDex  >= 0) { AttrCarouselWrite(attrs, offset_AttributesPanel_Dex,  g_CreateChar.attrDex);  written++; }
        if (g_CreateChar.attrCon  >= 0) { AttrCarouselWrite(attrs, offset_AttributesPanel_Con,  g_CreateChar.attrCon);  written++; }
        if (g_CreateChar.attrInt  >= 0) { AttrCarouselWrite(attrs, offset_AttributesPanel_Int,  g_CreateChar.attrInt);  written++; }
        if (g_CreateChar.attrPsy  >= 0) { AttrCarouselWrite(attrs, offset_AttributesPanel_Psy,  g_CreateChar.attrPsy);  written++; }
        if (g_CreateChar.attrAge  >= 0) { AttrCarouselWrite(attrs, offset_AttributesPanel_Age,  g_CreateChar.attrAge);  written++; }
        if (g_CreateChar.attrSize >= 0) { AttrCarouselWrite(attrs, offset_AttributesPanel_Size, g_CreateChar.attrSize); written++; }
        if (g_CreateChar.attrPoints >= 0) { mem.Write<int32_t>(attrs + offset_AttributesPanel_iTotalAttrPoints, g_CreateChar.attrPoints); written++; }
    }

    // Родословная пишется ТОЛЬКО числами через BodyData vAncestry1..4 (выше).
    // Строковые ComboBox крови убраны — в игре их список пуст, писать нечего.

    return written;
}

// ============================================================
// ПОКАДРОВАЯ запись — вызывать из главного цикла.
// Если enabled — пишет каждый кадр (перебивает таймер игры, рост применяется).
// ============================================================
void CreateCharUpdate()
{
    if (!g_CreateChar.enabled) return;
    CreateCharWriteAll();
}

// ============================================================
// DIAGNOSE
// ============================================================
void CreateCharDiagnose()
{
    g_CreateCharDiag = CreateCharDiag{};
    g_CreateCharDiag.controller = CreateCharGetController();
    if (!g_CreateCharDiag.controller) return;
    g_CreateCharDiag.panel = CreateCharGetBloodlinePanel();
}

