#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include <cstdint>

// ============================================================
// CREATE CHARACTER — настройка внешности персонажа в меню создания.
// Логика разделена на два шага:
//   1) CreateCharRead()   — по кнопке читает текущие значения из игры
//                           в буфер g_CreateChar (для показа/правки).
//   2) CreateCharUpdate() — покадрово пишет всё, что задано
//                           в буфере g_CreateChar, в память контроллера
//                           создания (превью обновляется на ходу).
// Кнопку "Создать" в игре нажимает сам пользователь.
//
// Цепочка:
//   Base + offset_GWorld              -> UWorld
//   UWorld + offset_AuthorityGameMode -> MOGameModeMenu
//   MOGameModeMenu + pLoginHUD        -> MOLoginHUD
//   MOLoginHUD + pCreation            -> UMOCharacterCreationNew
//   Creation + charAppearance         -> FMOCharacterPlayerAppearance (inline @ 0x03C8)
// ============================================================

struct CreateCharSettings final {
    bool  enabled = false;       // покадровая запись активна (вкл/выкл)

    // --- Телосложение (charAppearance +0x00) ---
    float length       = 0.0f;   // рост
    float bodySize     = 0.0f;   // вес / телосложение
    float bodyStrength = 0.0f;   // сила тела

    // --- BodyData (+0x10): -1 = не трогать ---
    int   bodySpecies  = -1;     // раса (0 Human..4 Risar)
    int   bodySex      = -1;     // пол (0 None,1 Male,2 Female)
    int   bodyAge      = -1;     // возраст (лет)
    int   bodyColor    = -1;     // основной цвет
    int   ancestry1    = -1;     // родословная 1 (число, EMainRace)
    int   ancestry2    = -1;
    int   ancestry3    = -1;
    int   ancestry4    = -1;

    // --- DecorationData (+0x40): -1 = не трогать ---
    int   hairType     = -1;
    int   hairColor    = -1;
    int   beardType    = -1;
    int   beardColor   = -1;
    int   skinColor    = -1;
    int   eyeColor     = -1;
    int   skinDecoA    = -1;
    int   skinDecoB    = -1;
    int   skinDecoC    = -1;
    int   faceTattooType = -1;
    int   faceTattoo     = -1;
    int   bodyTattooType = -1;
    int   bodyTattoo     = -1;

    // --- Species-панель (карусели): -1 = не трогать ---
    int   race   = -1;
    int   gender = -1;
    int   clade  = -1;

    // --- Attributes-панель (карусели): -1 = не трогать ---
    int   attrStr = -1;
    int   attrDex = -1;
    int   attrCon = -1;
    int   attrInt = -1;
    int   attrPsy = -1;
    int   attrAge = -1;
    int   attrSize = -1;
    int   attrPoints = -1;       // iTotalAttributePoints

    // --- Bloodline (родословная через ComboBox): "" = не трогать ---
    std::wstring bloodline0;
    std::wstring bloodline1;
    std::wstring bloodline2;
    std::wstring bloodline3;
    std::wstring bloodline4;
    std::wstring bloodline5;
};

// Буфер значений (заполняется CreateCharRead, пишется CreateCharUpdate).
extern CreateCharSettings g_CreateChar;

// Список возможных кровей (читается из vPossibleBloodlines для подсказки).
extern std::vector<std::wstring> g_BloodlineNames;

// Диагностика цепочки (для показа в UI).
struct CreateCharDiag final {
    uintptr_t controller = 0;    // UMOCharacterCreationNew (0 = экран создания не открыт)
    uintptr_t panel       = 0;    // UMOCharCreationBloodlinePanel (0 = не тот шаг)
};
extern CreateCharDiag g_CreateCharDiag;

// true  — удалось дочитать до структуры внешности (можно читать/писать).
bool CreateCharIsAvailable();

// Читает текущие значения из игры в g_CreateChar (+ имена кровей). false = экран не открыт.
bool CreateCharRead();

// ПОКАДРОВАЯ запись. Если g_CreateChar.enabled — пишет всё заданное (!= -1 / не пусто)
// в память каждый кадр. Вызывать из главного цикла (main.cpp). Без спама в консоль.
void CreateCharUpdate();

// Проходит цепочку и заполняет g_CreateCharDiag (для кнопки Diagnose).
void CreateCharDiagnose();
