#include <WinSock2.h>
#include <Windows.h>
#include <iostream>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <dwmapi.h>
#include <d3d11.h>
#include "global.h"
#include <entity.h>
#include <functions.h>
#include "createcharacter.h"

ImFont* g_font = nullptr;

namespace {
    bool g_MenuOpen = false;
    bool g_ExitApplication = false;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    if (g_MenuOpen && ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param)) {
        return 0L;
    }

    if (message == WM_DESTROY) {
        g_ExitApplication = true;
        return 0L;
    }

    return DefWindowProc(window, message, w_param, l_param);
}

INT APIENTRY WinMain(HINSTANCE instance, HINSTANCE, PSTR, INT cmd_show)
{
    AllocConsole();
    FILE* fDummy = nullptr;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);

    AppSetting::get().init();

    WNDCLASSEXW wc = {
        sizeof(wc), CS_CLASSDC, WindowProc,
        0L, 0L, GetModuleHandle(nullptr),
        nullptr, nullptr, nullptr, nullptr,
        L"xMonitoring",
        nullptr
    };
    RegisterClassExW(&wc);

    HWND window = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        wc.lpszClassName,
        L"",
        WS_POPUP,
        0, 0,
        AppSetting::get().screenWidth,
        AppSetting::get().screenHeight,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    SetLayeredWindowAttributes(window, RGB(0, 0, 0), 255, LWA_ALPHA);

    RECT rc{};
    GetClientRect(window, &rc);
    MARGINS margins = { -1, -1, rc.right, rc.bottom };
    DwmExtendFrameIntoClientArea(window, &margins);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.OutputWindow = window;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* device_context = nullptr;
    IDXGISwapChain* swap_chain = nullptr;
    ID3D11RenderTargetView* render_target_view = nullptr;
    D3D_FEATURE_LEVEL level;

    D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0U,
        levels, 2U, D3D11_SDK_VERSION,
        &sd, &swap_chain, &device, &level, &device_context
    );

    ID3D11Texture2D* back_buffer = nullptr;
    swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));
    device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
    back_buffer->Release();

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    ImGui::CreateContext();
    ImGui::StyleColorsLight();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    g_font = io.Fonts->AddFontFromFileTTF("C:\\Users\\admin\\Desktop\\icon\\VerdanaIcon.ttf", 13);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
    style.TabBarBorderSize = 0.0f;
    style.TabMinWidthBase = 100.0f;
    style.TabRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.3f;
    style.WindowRounding = 7.0f;

    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(device, device_context);

    while (!g_ExitApplication)
    {
        if (GetAsyncKeyState(VK_END) & 0x8000) g_ExitApplication = true;

        static bool insertWas = false;
        bool insertDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;

        if (insertDown && !insertWas) {
            g_MenuOpen = !g_MenuOpen;

            LONG_PTR ex = GetWindowLongPtrW(window, GWL_EXSTYLE);
            if (g_MenuOpen) {
                ex &= ~WS_EX_TRANSPARENT;
            }
            else {
                ex |= WS_EX_TRANSPARENT;
            }
            SetWindowLongPtrW(window, GWL_EXSTYLE, ex);

            SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        insertWas = insertDown;

        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        CameraRefresh();   // ← свежая камера на этот кадр

        wallhack();
        TeleportPointMove();   // движение pull-точки стрелками (до Update/Draw)
        TeleportUpdate();
        TeleportDraw();
        NavPointsDraw();
        AimbotUpdate();
        AimbotDraw();
        TriggerbotUpdate();
        TimerUpdate();
        FullBrightUpdate();

        CreateCharUpdate();   // покадровая запись Create Character (если enabled)

        if (g_FriendSettings.enabled) FriendsUpdate();
        if (g_MountStamina.enabled)   MountStaminaUpdate();

        if (g_MenuOpen) menu();

        ImGui::Render();

        device_context->OMSetRenderTargets(1U, &render_target_view, nullptr);

        const float bgAlpha = AppSetting::get().fuserMode ? 1.0f : 0.0f;
        float clear_color[4] = { 0.0f, 0.0f, 0.0f, bgAlpha };
        device_context->ClearRenderTargetView(render_target_view, clear_color);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        swap_chain->Present(0U, 0U);

        Sleep(1);
    }

    StopThreads();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (swap_chain)         swap_chain->Release();
    if (device_context)     device_context->Release();
    if (device)             device->Release();
    if (render_target_view) render_target_view->Release();

    DestroyWindow(window);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}