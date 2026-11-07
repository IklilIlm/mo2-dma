#pragma once

#include <Windows.h>
#include <string>
#include <atomic>


class AppSetting final {
public:
	static AppSetting& get();

	int screenWidth = 1920;
	int screenHeight = 1080;

	// общие между потоками — только атомарно (пишет entity_update, читает UI)
	std::atomic<double> last_update_ms{ 0 };
	std::atomic<float>  limit_update_ms{ 16.67f };

	char kmboxIP[24] = "192.168.2.188";
	char kmboxPort[10] = "4120";
	char kmboxUUID[32] = "3619DC32";

	// Fuser Mode: непрозрачный чёрный фон оверлея (Misc -> чекбокс);
	// лежит в padding-байте после char[32], sizeof(AppSetting) не меняется
	bool fuserMode = false;

	void init();
	void load();
	void save();

private:
	AppSetting() = default;
	~AppSetting() = default;

	AppSetting(const AppSetting&) = delete;
	AppSetting& operator=(const AppSetting&) = delete;

	AppSetting(AppSetting&&) = delete;
	AppSetting& operator=(AppSetting&&) = delete;
};

double diff(LARGE_INTEGER start, LARGE_INTEGER end);

// kmBox init OK; пишется фоновым потоком, читается UI — только атомарно
extern std::atomic<bool> kmboxDetected;

// false => фоновые потоки (entity_update / kmboxInit) должны завершиться
extern std::atomic<bool> g_Running;

// дождаться завершения фоновых потоков (вызывать при выходе из приложения)
void StopThreads();