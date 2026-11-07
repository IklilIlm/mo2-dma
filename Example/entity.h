#pragma once
#include <Windows.h>
#include <MyMath.h>
#include <vector>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <string>

struct FString final {
	uintptr_t Data;
	int32_t  ArrayNum;
	int32_t  ArrayMax;
};

struct FName final {
	int32_t ComparisonIndex;
	int32_t Number;
};

struct camera final {
	float   Timestamp;
	UINT8   Pad_4[0xC];
	Vector3 Location;
	Vector3 Rotation;
	float   FOV;
};

struct LocalManager final {
	uintptr_t ULocalPlayer = NULL;
	uintptr_t APlayerController = NULL;
	uintptr_t APlayerCameraManager = NULL;
	uintptr_t APawn = NULL;
	camera CameraCachePrivate;
};

struct ActorManager final {
	uintptr_t AActor = NULL;
	uintptr_t RootComponent = NULL;

	FName UEname;
	std::string UEClassName;

	FString Nickname;
	wchar_t name[64];

	float CharacterHeight;
	uint8_t TypeAttack = 255;

	Vector3 position;

	float health = 0.f;
	float maxHealth = 0.f;
};

struct TArray final {
	uintptr_t data;
	int count;
	int max;
};

struct WorldSnapshot final {
	std::vector<ActorManager> actors;
	LocalManager              hero;
};

class Entity final {
public:
	static Entity& get();
	static std::shared_ptr<const WorldSnapshot> Snapshot();

	std::vector<uintptr_t> ActorFriend;

	void entity_update();

private:
	Entity() = default;
	~Entity() = default;

	Entity(const Entity&) = delete;
	Entity& operator=(const Entity&) = delete;
	Entity(Entity&&) = delete;
	Entity& operator=(Entity&&) = delete;

	std::shared_ptr<WorldSnapshot> snap_;
};

// ============================================================
// Render camera — свежее чтение камеры раз в UI-кадр
// ============================================================
extern camera g_RenderCamera;
extern std::atomic<bool> g_RenderCameraValid;
void CameraRefresh();

// Хелпер: взять свежую камеру если она валидна, иначе из снапшота.
// inline — чтобы не плодить линковку в каждом TU.
inline const camera& GetRenderCamera(const LocalManager& hero)
{
	return g_RenderCameraValid.load(std::memory_order_relaxed)
		? g_RenderCamera
		: hero.CameraCachePrivate;
}

// SDK

#define EXE_NAME "MortalOnline2-Win64-Shipping.exe"

// оффсеты для ручного поиска начало
#define offset_GName					   0x1137B0C0
#define offset_Nickname					   0x0CE0
#define offset_Health					   0x0D20 
#define offset_MaxHealth				   0x0D24 
#define offset_RelativeLocation			   0x0210 
// оффсеты для ручного поиска конец

#define offset_GWorld                      0x114CBAC8

#define offset_PersistentLevel             0x0030
#define offset_OwningGameInstance		   0x0228

#define offset_Actors                      0x00A0
#define offset_RootComponent               0x01B8
#define offset_FName					   0x0018


#define offset_LocalPlayers				   0x0038
#define offset_PlayerController			   0x0030

#define offset_PlayerCameraManager         0x0360
#define offset_Pawn						   0x02E8
#define offset_CameraCachePrivate		   0x15D0

#define offset_LastAttackAnimationDirection 0x0BE9
#define offset_fCharacterHeight			    0x0DFC // SDK: 0x0DFC (было 0x0DF4, разница -8)

#define offset_LookAtCharacter				0x0700

#define offset_CustomTimeDilation		    0x0068

#define MOUNT_MAX_SPEED_SLOTS 16
#define offset_MountRider           0x0340
#define offset_MountPtr             0x0370

#define offset_MountRiderPose       0x1078 // SDK: 0x1078 (было 0x1068, +0x10)
#define offset_fMountedSpeed        0x110C // SDK: 0x110C (было 0x10FC, +0x10)
#define offset_iThrottleTgt         0x1110 // SDK: 0x1110 (было 0x1100, +0x10)
#define offset_fCurStamina          0x1114 // SDK: 0x1114 (было 0x1104, +0x10)
#define offset_bAllowedAsPassenger  0x1118 // SDK: 0x1118 (было 0x1108, +0x10)
#define offset_iAiMountableFromLevel 0x111C // SDK: 0x111C (было 0x110C, +0x10)
#define offset_iAiMountableSlots    0x1120 // SDK: 0x1120 (было 0x1110, +0x10)
#define offset_fTickStamina         0x1124 // SDK: 0x1124 (было 0x1114, +0x10)
#define offset_iThrottleCap         0x1128 // SDK: 0x1128 (было 0x1118, +0x10)
#define offset_DefaultCapsuleRadius 0x1130 // SDK: 0x1130 (было 0x111C, +0x10)
#define offset_DefaultCapsuleHalfHeight 0x1134 // SDK: 0x1134 (было 0x1120, +0x10)

#define offset_MountStats           0x1090 // SDK: 0x1090 (было 0x1080, +0x10)
#define offset_fMountStaminaMax     0x1098 // SDK: FMOAIMountStats.fMountStaminaMax @ 0x0008 → 0x1090+0x08
#define offset_vMountMaxSpeed_data  0x10A0 // SDK: FMOAIMountStats.vMountMaxSpeed @ 0x0010 (относительно MountStats = 0x10A0 absol.)
#define offset_vMountMaxSpeed_count 0x10A8 // SDK: TArray.Count @ 0x0018 (относительно MountStats = 0x10A8 absol.)
#define offset_fSwimSpeedMultiplier 0x10B0 // SDK: FMOAIMountStats.fSwimSpeedMultiplier @ 0x0020 (относительно MountStats = 0x10B0 absol.)
#define offset_SlowestSpeedTurnRate 0x10B4 // SDK: FMOAIMountStats.SlowestSpeedTurnRate @ 0x0024
#define offset_FastestSpeedTurnRate 0x10B8 // SDK: FMOAIMountStats.FastestSpeedTurnRate @ 0x0028
#define offset_fMountBreakSpeed     0x10BC // SDK: FMOAIMountStats.fMountBreakSpeed @ 0x002C
#define offset_fMountThrottleSpeed  0x10C0 // SDK: FMOAIMountStats.fMountThrottleSpeed @ 0x0030
#define offset_fRiderWeightPenaltyThreshold 0x10C4 // SDK: FMOAIMountStats.fRiderWeightPenaltyThreshold @ 0x0034
#define offset_fMaxArmorWeight_ModSpeed      0x10C8 // SDK: FMOAIMountStats.fMaxArmorWeight_ModSpeed @ 0x0038
#define offset_fMaxArmorWeight_ModTurnRate   0x10CC // SDK: FMOAIMountStats.fMaxArmorWeight_ModTurnRate @ 0x003C
#define offset_fSlidingMinSlopeAngle         0x10D0 // SDK:  в FMOAIMountStats 
#define offset_fSlidingMaxSlopeAngle         0x10D4 // SDK:  в FMOAIMountStats 


// ============================================================
// CHARACTER CREATION (меню создания персонажа) — CreateCharacter
// Цепочка: UWorld -> AuthorityGameMode (MOGameModeMenu) -> pLoginHUD
//          -> pMOCharacterCreationNew -> charAppearance (inline struct @ 0x03C8)
// ============================================================
#define offset_AuthorityGameMode           0x01A8   // AGameModeBase* on UWorld (= MOGameModeMenu в меню)
#define offset_GameModeMenu_pLoginHUD      0x0360   // UMOLoginHUD*
#define offset_LoginHUD_pCreation          0x03C8   // UMOCharacterCreationNew*
#define offset_CharCreation_charAppearance 0x03C8   // FMOCharacterPlayerAppearance (inline, size 0x02C8)

#define offset_Appearance_Length           0x0000   // float - рост
#define offset_Appearance_BodySize         0x0004   // float - вес / телосложение
#define offset_Appearance_BodyStrength     0x0008   // float - сила / тело

// FMOCharacterBodyData (внутри charAppearance @ +0x10)
#define offset_Body_ESpecies               0x0010   // uint8 (0 Human,1 Alvarin,2 Oghmir,3 Thursar,4 Risar)
#define offset_Body_ESex                   0x0011   // uint8 (0 None,1 Male,2 Female)
#define offset_Body_iAge                   0x0014   // int32 - ВОЗРАСТ
#define offset_Body_iColor                 0x0018   // int32 - основной цвет
#define offset_Body_vAncestry1             0x001C   // int32 - родословная 1
#define offset_Body_vAncestry2             0x0020   // int32 - родословная 2
#define offset_Body_vAncestry3             0x0024   // int32 - родословная 3
#define offset_Body_vAncestry4             0x0028   // int32 - родословная 4

// FMOCharacterDecorationData (внутри charAppearance @ +0x40)
#define offset_Dec_iHairTypeIndex          0x0040
#define offset_Dec_iHairColorIndex         0x0044
#define offset_Dec_iBeardTypeIndex         0x0048
#define offset_Dec_iBeardColorIndex        0x004C
#define offset_Dec_iSkinColorIndex         0x0050
#define offset_Dec_iEyeColorIndex          0x0054
#define offset_Dec_iSkinDecorationIndexA   0x0058
#define offset_Dec_iSkinDecorationIndexB   0x005C
#define offset_Dec_iSkinDecorationIndexC   0x0060
#define offset_Dec_iFaceTattooTypeIndex    0x0064
#define offset_Dec_iFaceTattooIndex        0x0068
#define offset_Dec_iBodyTattooTypeIndex    0x006C
#define offset_Dec_iBodyTattooIndex        0x0070

// Панели создания
#define offset_CharCreation_pPanelSpecies   0x06F8  // UMOCharCreationSpeciesPanel*
#define offset_CharCreation_pPanelAttributes 0x0708 // UMOCharCreationAttributesPanel*
#define offset_CharCreation_pPanelBloodline 0x0700  // UMOCharCreationBloodlinePanel*
// Species-карусели (UMOCarouselSelectText)
#define offset_SpeciesPanel_Gender          0x03F8
#define offset_SpeciesPanel_Clade           0x0400
#define offset_SpeciesPanel_Race            0x0408
// Attributes-карусели (UMOCarouselSelectValue)
#define offset_AttributesPanel_Str          0x03E0
#define offset_AttributesPanel_Dex          0x03E8
#define offset_AttributesPanel_Con          0x03F0
#define offset_AttributesPanel_Int          0x03F8
#define offset_AttributesPanel_Psy          0x0400
#define offset_AttributesPanel_Age          0x0408
#define offset_AttributesPanel_Size         0x0410
#define offset_AttributesPanel_iTotalAttrPoints 0x03B8 // int32 - очков на атрибуты
#define offset_CarouselText_iCurrentIndex     0x03C8  // int32 - индекс в Species-карусели
#define offset_CarouselValue_iCurrentValue    0x03D4  // int32 - значение в Attributes-карусели

// Bloodline (родословная)
#define offset_Bloodline_vPossibleBloodlines 0x03E0  // TArray<FString> - список возможных кровей
#define offset_Bloodline_Ancestry0          0x0430    // MOCharCreationComboBox*
#define offset_Bloodline_Ancestry1          0x0440
#define offset_Bloodline_Ancestry2          0x0450
#define offset_Bloodline_Ancestry3          0x0460
#define offset_Bloodline_Ancestry4          0x0470
#define offset_Bloodline_Ancestry5          0x0480
#define offset_MOComboBox_pMOComboBoxString 0x03A8    // нативный UMG.ComboBoxString*
#define offset_ComboBox_DefaultOptions      0x0178    // TArray<FString>: Data+0x00 Count+0x08
#define offset_ComboBox_SelectedOption      0x0188    // FString (16 байт)

// ============================================================
// CAMERA FULLBRIGHT — пост-обработка локальной камеры
// Используется только в fullbright.cpp
// ============================================================
#define offset_CameraComponent        0x0C10   // AMOCharacter::CameraComponent (UMOCameraComponent*)
#define offset_CamBlendWeight         0x0300   // UCameraComponent::PostProcessBlendWeight
#define offset_CamPostProcess         0x0330   // UCameraComponent::PostProcessSettings (FPostProcessSettings 0x7B0)

// Fallback: закешированный POV в APlayerCameraManager (указатель всегда валиден)
#define offset_CamCachePrivate        0x15D0   // APlayerCameraManager::CameraCachePrivate (FCameraCacheEntry)
#define offset_CamMgr_POV             0x0010   // FCameraCacheEntry::POV (FMinimalViewInfo)
#define offset_POV_PostProcessBlend   0x0070   // FMinimalViewInfo::PostProcessBlendWeight
#define offset_POV_PostProcess        0x0080   // FMinimalViewInfo::PostProcessSettings

// --- внутри FPostProcessSettings (прибавляются к offset_CamPostProcess) ---
#define offset_PP_AutoExposureBias    0x049C   // float
#define offset_PP_AutoExposureMinB    0x04C8   // float
#define offset_PP_AutoExposureMaxB    0x04CC   // float
#define offset_PP_AutoExposureSpeedDown 0x04D4 // float
#define offset_PP_AmbientOcclusion    0x0640   // float
#define offset_PP_IndirectLighting    0x03F8   // float

// --- bOverride_* биты внутри FPostProcessSettings (без них поля игнорируются) ---
#define offset_PP_Ovr_ExposureMinMax  0x0009   // бит5 MinBrightness, бит6 MaxBrightness
#define offset_PP_Ovr_ExposureSpdBias 0x000A   // бит1 SpeedDown, бит2 Bias
#define offset_PP_Ovr_AO              0x000F   // бит7 AmbientOcclusionIntensity
#define offset_PP_Ovr_Indirect        0x001A   // бит0 IndirectLightingIntensity
