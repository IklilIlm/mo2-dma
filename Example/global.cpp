#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <global.h>
#include <iostream>
#include <mmsystem.h>
#include <thread>
#include <cstring>
#include <entity.h>
#include <functions.h>
#include <KmboxNet.h>

#pragma comment(lib, "winmm.lib")

std::atomic<bool> kmboxDetected{ false };
std::atomic<bool> g_Running{ true };

namespace {
	std::thread g_entityThread;
	std::thread g_kmboxThread;

	// POD-зеркало AppSetting для config.bin: поля читаются/пишутся поэлементно,
	// т.к. сам AppSetting содержит атомики (побайтовый fread в него — UB).
	struct AppSettingCfg {
		int    screenWidth;
		int    screenHeight;
		double last_update_ms;
		float  limit_update_ms;
		char   kmboxIP[24];
		char   kmboxPort[10];
		char   kmboxUUID[32];
		bool   fuserMode;
	};
	static_assert(sizeof(AppSettingCfg) == sizeof(AppSetting),
		"layout AppSetting изменился — обновите AppSettingCfg (config.bin несовместим)");
}

void kmboxInit() {
	int err = kmNet_init(AppSetting::get().kmboxIP, AppSetting::get().kmboxPort, AppSetting::get().kmboxUUID);
	if (err == 0) kmboxDetected = true;
}

AppSetting& AppSetting::get() {
	static AppSetting instance;
	return instance;
}

void AppSetting::init() {
	timeBeginPeriod(1);
	load();
	if (!g_entityThread.joinable())
		g_entityThread = std::thread(&Entity::entity_update, &Entity::get());
	if (!g_kmboxThread.joinable())
		g_kmboxThread = std::thread(kmboxInit);
}

void StopThreads() {
	g_Running = false;
	// entity_update — обязательно ждём (работает с общими статиками/mem)
	if (g_entityThread.joinable()) g_entityThread.join();
	// kmboxInit может навечно сидеть в recvfrom (UDP без таймаута),
	// если устройства нет — join завис бы; поток пишет только атомар bool
	if (g_kmboxThread.joinable()) g_kmboxThread.detach();
}

void AppSetting::load() {

	FILE* F = fopen("config.bin", "rb");
	if (F == nullptr)
	{
		std::cout << "[-] Config not found!" << std::endl;
		return;
	}
	std::cout << "[+] Config load!" << std::endl;

	AppSettingCfg cfg{};
	if (fread(&cfg, sizeof(cfg), 1, F) == 1) {
		screenWidth = cfg.screenWidth;
		screenHeight = cfg.screenHeight;
		last_update_ms = cfg.last_update_ms;
		limit_update_ms = cfg.limit_update_ms;
		memcpy(kmboxIP, cfg.kmboxIP, sizeof(kmboxIP));
		memcpy(kmboxPort, cfg.kmboxPort, sizeof(kmboxPort));
		memcpy(kmboxUUID, cfg.kmboxUUID, sizeof(kmboxUUID));
		fuserMode = cfg.fuserMode;
	}

	fread(&g_FriendSettings, sizeof(FriendSettings), 1, F);
	fread(&g_MountStamina, sizeof(MountStaminaSettings), 1, F);
	fread(&g_Esp, sizeof(EspSettings), 1, F);
	fread(&g_Aimbot, sizeof(AimbotSettings), 1, F);
	fread(&g_Triggerbot, sizeof(TriggerbotSettings), 1, F);
	fread(&g_Timer, sizeof(TimerSettings), 1, F);
	fread(&g_FullBright, sizeof(FullBrightSettings), 1, F);
	// nav points: блок добавлен позже, в старом config.bin его нет —
	// fread просто не сработает и g_NavPoints останется дефолтным (пустым)
	fread(&g_NavPoints, sizeof(NavPointsSettings), 1, F);
	fclose(F);
}

void AppSetting::save() {

	FILE* F = fopen("config.bin", "wb");
	if (F == nullptr)
	{
		std::cout << "[-] Config error save!" << std::endl;
		return;
	}
	std::cout << "[+] Config save!" << std::endl;

	AppSettingCfg cfg{};
	cfg.screenWidth = screenWidth;
	cfg.screenHeight = screenHeight;
	cfg.last_update_ms = last_update_ms;
	cfg.limit_update_ms = limit_update_ms;
	memcpy(cfg.kmboxIP, kmboxIP, sizeof(cfg.kmboxIP));
	memcpy(cfg.kmboxPort, kmboxPort, sizeof(cfg.kmboxPort));
	memcpy(cfg.kmboxUUID, kmboxUUID, sizeof(cfg.kmboxUUID));
	cfg.fuserMode = fuserMode;
	fwrite(&cfg, sizeof(cfg), 1, F);

	fwrite(&g_FriendSettings, sizeof(FriendSettings), 1, F);
	fwrite(&g_MountStamina, sizeof(MountStaminaSettings), 1, F);
	fwrite(&g_Esp, sizeof(EspSettings), 1, F);
	fwrite(&g_Aimbot, sizeof(AimbotSettings), 1, F);
	fwrite(&g_Triggerbot, sizeof(TriggerbotSettings), 1, F);
	fwrite(&g_Timer, sizeof(TimerSettings), 1, F);
	fwrite(&g_FullBright, sizeof(FullBrightSettings), 1, F);
	fwrite(&g_NavPoints, sizeof(NavPointsSettings), 1, F);
	fclose(F);
}

double diff(LARGE_INTEGER start, LARGE_INTEGER end) { // returns milliseconds
	static LARGE_INTEGER freq;
	static bool init = false;
	if (!init) {
		QueryPerformanceFrequency(&freq);
		init = true;
	}

	return static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart);
}