#include <WinSock2.h>
#include <Windows.h>
#include <entity.h>
#include <global.h>
#include <DMALibrary/Memory/Memory.h>
#include <iostream>
#include <cstring>
#include <KmboxNet.h>
#include <thread>
#include <memory>

namespace {
	std::unordered_map<uint32_t, std::string> g_fnameCache;

	// ============================================================
	// Кеш "игрочности" по УКАЗАТЕЛЮ актора.
	// Заполняется каждому кадру при разрешении FName (там всегда
	// актуальное, правильное имя), читается при формировании
	// scatter-запросов player-полей.
	// Это чинит баг: раньше решение "BP_PlayerCharacter_C" принималось
	// по UEClassName из СЛОТА БУФЕРА, который содержал имя с прошлой
	// итерации этого же буфера (2 кадра назад). Индексы акторов между
	// кадрами сдвигаются (спавн/деспавн), и в слоте оказывалось имя
	// ДРУГОГО актора -> HP/ник читались у "не тех" сущностей, а у
	// настоящих игроков наоборот пропускались.
	// ============================================================
	std::unordered_map<uintptr_t, bool> g_isPlayerCache;
}

// ============================================================
// Render camera — свежая камера, читается раз в UI-кадр
// ============================================================
camera g_RenderCamera{};
std::atomic<bool> g_RenderCameraValid{ false };

void CameraRefresh()
{
	auto snap = Entity::Snapshot();
	if (!snap) { g_RenderCameraValid.store(false, std::memory_order_relaxed); return; }

	uintptr_t camMgr = snap->hero.APlayerCameraManager;
	if (!camMgr || camMgr == (uintptr_t)-1) {
		g_RenderCameraValid.store(false, std::memory_order_relaxed);
		return;
	}

	camera c{};
	mem.Read(camMgr + offset_CameraCachePrivate, &c, sizeof(camera));

	if (c.FOV < 1.0f || c.FOV > 180.0f ||
		(c.Location.x == 0.0 && c.Location.y == 0.0 && c.Location.z == 0.0)) {
		g_RenderCameraValid.store(false, std::memory_order_relaxed);
		return;
	}

	g_RenderCamera = c;
	g_RenderCameraValid.store(true, std::memory_order_relaxed);
}

// ============================================================
Entity& Entity::get() {
	static Entity instance;
	return instance;
}

std::shared_ptr<const WorldSnapshot> Entity::Snapshot() {
	return std::atomic_load(&get().snap_);
}

void Entity::entity_update()
{
	LARGE_INTEGER start, temp_end, end;

	if (!mem.Init(EXE_NAME, true, true));
	if (!mem.FixCr3());
	if (!mem.GetKeyboard()->InitKeyboard());

	uintptr_t Base = mem.GetBaseDaddy(EXE_NAME);

	uintptr_t UWorld = mem.Read<uintptr_t>(Base + offset_GWorld);
	uintptr_t PersistentLevel = mem.Read<uintptr_t>(UWorld + offset_PersistentLevel);
	uintptr_t OwningGameInstance = mem.Read<uintptr_t>(UWorld + offset_OwningGameInstance);
	uintptr_t GNames = Base + offset_GName;

	std::shared_ptr<WorldSnapshot> bufs[2] = {
		std::make_shared<WorldSnapshot>(),
		std::make_shared<WorldSnapshot>()
	};
	int cur = 0;

	auto handle = mem.CreateScatterHandle();
	while (!GetAsyncKeyState(VK_END)) {
		QueryPerformanceCounter(&start);

		if (PersistentLevel && OwningGameInstance != NULL) {

			while (bufs[cur].use_count() > 1) std::this_thread::yield();

			auto& snap = *bufs[cur];

			TArray LocalPlayers, Actors;

			mem.AddScatterReadRequest(handle, OwningGameInstance + offset_LocalPlayers, &LocalPlayers, sizeof(TArray));
			mem.AddScatterReadRequest(handle, PersistentLevel + offset_Actors, &Actors, sizeof(TArray));
			mem.ExecuteReadScatter(handle);

			if (Actors.data != NULL)
			{
				if (static_cast<int>(snap.actors.size()) != Actors.count)
					snap.actors.resize(Actors.count);

				for (int i = 0; i < Actors.count; ++i)
					mem.AddScatterReadRequest(handle, Actors.data + 0x8 * i, &snap.actors[i].AActor, sizeof(uintptr_t));
				mem.ExecuteReadScatter(handle);
				for (int i = 0, length = (int)snap.actors.size(); i < length; ++i)
				{
					if (snap.actors[i].AActor)
					{
						mem.AddScatterReadRequest(handle, snap.actors[i].AActor + offset_RootComponent, &snap.actors[i].RootComponent, sizeof(uintptr_t));
						mem.AddScatterReadRequest(handle, snap.actors[i].AActor + offset_FName, &snap.actors[i].UEname, sizeof(FName));

						// player-поля решаем по кешу УКАЗАТЕЛЯ актора.
						// Имя на этот момент ещё не разрешено для текущего кадра,
						// а решение по слоту/старому строковому буферу давало
						// ложные срабатывания. Для НОВОГО актора поля
						// дочитаются на следующем обновлении (~16мс) — это не видно.
						auto itP = g_isPlayerCache.find(snap.actors[i].AActor);
						if (itP != g_isPlayerCache.end() && itP->second) {
							mem.AddScatterReadRequest(handle, snap.actors[i].AActor + offset_Nickname, &snap.actors[i].Nickname, sizeof(FString));
							mem.AddScatterReadRequest(handle, snap.actors[i].AActor + offset_LastAttackAnimationDirection, &snap.actors[i].TypeAttack, sizeof(uint8_t));
							mem.AddScatterReadRequest(handle, snap.actors[i].AActor + offset_Health, &snap.actors[i].health, sizeof(float));
							mem.AddScatterReadRequest(handle, snap.actors[i].AActor + offset_MaxHealth, &snap.actors[i].maxHealth, sizeof(float));
							mem.AddScatterReadRequest(handle, snap.actors[i].AActor + offset_fCharacterHeight, &snap.actors[i].CharacterHeight, sizeof(float));
						}
					}
				}
				mem.ExecuteReadScatter(handle);

				for (int i = 0, length = (int)snap.actors.size(); i < length; ++i)
				{
					if (snap.actors[i].RootComponent)
						mem.AddScatterReadRequest(handle, snap.actors[i].RootComponent + offset_RelativeLocation, &snap.actors[i].position, sizeof(Vector3));
					if (snap.actors[i].Nickname.Data && snap.actors[i].Nickname.ArrayNum > 0 && snap.actors[i].Nickname.ArrayNum < 62)
						mem.AddScatterReadRequest(handle, snap.actors[i].Nickname.Data, &snap.actors[i].name[0], snap.actors[i].Nickname.ArrayNum * 2);
				}
				mem.ExecuteReadScatter(handle);

				for (int i = 0, length = (int)snap.actors.size(); i < length; ++i)
				{
					if (!snap.actors[i].AActor) {
						// слот пуст, но UEname мог остаться от старого использования
						// этого же буфера (2 кадра назад) — не резолвим мусор
						snap.actors[i].UEClassName.clear();
						continue;
					}

					uint32_t ci = snap.actors[i].UEname.ComparisonIndex;
					if (ci == 0) {
						snap.actors[i].UEClassName.clear();
						continue;
					}
					
					auto it = g_fnameCache.find(ci);
					if (it == g_fnameCache.end())
					{
						uint32_t block = ci >> 16;
						uint32_t offset = ci & 0xFFFF;

						uintptr_t pBlock = 0;
						mem.Read(GNames + (block + 2) * 8, &pBlock, sizeof(uintptr_t));

						if (!pBlock) { snap.actors[i].UEClassName.clear(); continue; }

						uintptr_t entryAddr = pBlock + offset * 2;

						uint16_t header = 0;
						mem.Read(entryAddr, &header, sizeof(uint16_t));

						uint16_t len = header >> 6;
						bool     wide = (header & 0x01) != 0;

						if (len == 0 || len >= 128) { snap.actors[i].UEClassName.clear(); continue; }

						char buffer[128] = {};
						if (wide) {
							wchar_t wbuf[128] = {};
							mem.Read(entryAddr + 2, wbuf, len * 2);
							wbuf[len] = L'\0';
							wcstombs(buffer, wbuf, sizeof(buffer) - 1);
						}
						else {
							mem.Read(entryAddr + 2, buffer, len);
							buffer[len] = '\0';
						}

						g_fnameCache[ci] = buffer;
						snap.actors[i].UEClassName = buffer;
					}
					else snap.actors[i].UEClassName = it->second;

					// имя разрешено ВЕРНО (для текущего кадра и именно этого
					// актора) — обновляем кеш "игрочности" по указателю.
					// При ci==0 / сбое чтения FName мы сюда не попадаем:
					// в кеш остаётся прошлое верное значение, а не мусор.
					g_isPlayerCache[snap.actors[i].AActor] =
						(strstr(snap.actors[i].UEClassName.c_str(), "BP_PlayerCharacter_C") != nullptr);
				}

				// ограничение роста кеша (unique указатели акторов копятся за сессию)
				if (g_isPlayerCache.size() > 16384) g_isPlayerCache.clear();
			}
			if (LocalPlayers.data != NULL)
			{
				snap.hero.ULocalPlayer = mem.Read<uintptr_t>(LocalPlayers.data);

				if (snap.hero.ULocalPlayer)
					snap.hero.APlayerController = mem.Read<uintptr_t>(snap.hero.ULocalPlayer + offset_PlayerController);

				if (snap.hero.APlayerController) {
					snap.hero.APlayerCameraManager = mem.Read<uintptr_t>(snap.hero.APlayerController + offset_PlayerCameraManager);
					snap.hero.APawn = mem.Read<uintptr_t>(snap.hero.APlayerController + offset_Pawn);
				}

				if (snap.hero.APlayerCameraManager)
					snap.hero.CameraCachePrivate = mem.Read<camera>(snap.hero.APlayerCameraManager + offset_CameraCachePrivate);
			}

			std::atomic_store(&snap_, bufs[cur]);
			cur = 1 - cur;
		}

		QueryPerformanceCounter(&temp_end);
		double temp_update_ms = diff(start, temp_end);
		if (AppSetting::get().limit_update_ms >= temp_update_ms)
			Sleep(static_cast<DWORD>(AppSetting::get().limit_update_ms - temp_update_ms));
		QueryPerformanceCounter(&end);
		AppSetting::get().last_update_ms = diff(start, end);
	}
	mem.CloseScatterHandle(handle);
}