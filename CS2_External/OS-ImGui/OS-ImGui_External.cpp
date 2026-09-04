#include "OS-ImGui_External.h"
#include "..\MenuConfig.hpp"
#include <climits>
#include <chrono>
#include <algorithm>
#include <thread>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
/****************************************************
* Copyright (C)	: Liv
* @file			: OS-ImGui_External.cpp
* @author		: Liv
* @email		: 1319923129@qq.com
* @version		: 1.0
* @date			: 2023/6/18	11:21
****************************************************/

// D3D11 Device
namespace OSImGui
{
#ifdef _CONSOLE
    bool D3DDevice::CreateDeviceD3D(HWND hWnd)
    {
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT createDeviceFlags = 0;
        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
        HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
        if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
            res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
        if (res != S_OK)
            return false;

        CreateRenderTarget();
        return true;
    }

    void D3DDevice::CleanupDeviceD3D()
    {
        CleanupRenderTarget();
        if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
        if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
        if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
    }

    void D3DDevice::CreateRenderTarget()
    {
        ID3D11Texture2D* pBackBuffer;
        g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        if (pBackBuffer == nullptr)
            return;
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }

    void D3DDevice::CleanupRenderTarget()
    {
        if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
    }
#endif
}

// OSImGui External
namespace OSImGui
{

    LRESULT WINAPI WndProc_External(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OSImGui_External::NewWindow(std::string WindowName, Vec2 WindowSize, std::function<void()> CallBack)
    {
        if (!CallBack)
            throw OSException("CallBack is empty");
        if (WindowName.empty())
            Window.Name = "Window";

        Window.Name = WindowName;
        Window.wName = StringToWstring(Window.Name);
        Window.ClassName = "WindowClass";
        Window.wClassName = StringToWstring(Window.ClassName);
        Window.Size = WindowSize;

        Type = NEW;
        CallBackFn = CallBack;

        if (!CreateMyWindow())
            throw OSException("CreateMyWindow() call failed");

        try {
            InitImGui(g_Device.g_pd3dDevice, g_Device.g_pd3dDeviceContext);
        }
        catch (OSException& e)
        {
            throw e;
        }

        MainLoop();
    }

    void  OSImGui_External::AttachAnotherWindow(std::string DestWindowName, std::string DestWindowClassName, std::function<void()> CallBack)
    {
        if (!CallBack)
            throw OSException("CallBack is empty");
        if (DestWindowName.empty() && DestWindowClassName.empty())
            throw OSException("DestWindowName and DestWindowClassName are empty");

        Window.Name = "Window";
        Window.wName = StringToWstring(Window.Name);
        Window.ClassName = "WindowClass";
        Window.wClassName = StringToWstring(Window.ClassName);
        Window.BgColor = ImColor(0, 0, 0, 0);

        DestWindow.hWnd = FindWindowA(
            (DestWindowClassName.empty() ? NULL : DestWindowClassName.c_str()),
            (DestWindowName.empty() ? NULL : DestWindowName.c_str()));
        if (DestWindow.hWnd == NULL)
            throw OSException("DestWindow isn't exist");
        DestWindow.Name = DestWindowName;
        DestWindow.ClassName = DestWindowClassName;

        Type = ATTACH;
        CallBackFn = CallBack;

        if (!CreateMyWindow())
            throw OSException("CreateMyWindow() call failed");

        try {
            InitImGui(g_Device.g_pd3dDevice,g_Device.g_pd3dDeviceContext);
        }
        catch (OSException& e)
        {
            throw e;
        }

        MainLoop();
    }


    void OSImGui_External::AttachWindowHandle(HWND DestWindowHandle, std::function<void()> CallBack)
    {
        if (!CallBack)
            throw OSException("CallBack is empty");
        if (DestWindowHandle == nullptr || !IsWindow(DestWindowHandle))
            throw OSException("Destination window handle is invalid");

        Window.Name = "Window";
        Window.wName = StringToWstring(Window.Name);
        Window.ClassName = "WindowClass";
        Window.wClassName = StringToWstring(Window.ClassName);
        Window.BgColor = ImColor(0, 0, 0, 0);

        DestWindow.hWnd = DestWindowHandle;
        DestWindow.Name.clear();
        DestWindow.ClassName.clear();

        Type = ATTACH;
        CallBackFn = CallBack;

        // Prime the target dimensions before creating the overlay so it can never
        // spend its first frame attached to a 0x0/hidden SDL helper window.
        RECT rect{};
        POINT point{};
        if (!GetClientRect(DestWindow.hWnd, &rect) || !ClientToScreen(DestWindow.hWnd, &point) ||
            rect.right <= rect.left || rect.bottom <= rect.top)
            throw OSException("Destination window has an invalid client area");

        Window.Pos = DestWindow.Pos = Vec2(static_cast<float>(point.x), static_cast<float>(point.y));
        Window.Size = DestWindow.Size = Vec2(static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top));

        if (!CreateMyWindow())
            throw OSException("CreateMyWindow() call failed");

        InitImGui(g_Device.g_pd3dDevice, g_Device.g_pd3dDeviceContext);
        MainLoop();
    }

    bool OSImGui_External::PeekEndMessage()
    {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                return true;
        }
        return false;
    }

    void OSImGui_External::MainLoop()
    {
        // Windows' default scheduler tick can be ~15.6 ms. That made short
        // sleeps for 120/240/400 FPS round up to roughly one 15.6 ms tick
        // (about 64 FPS). Request 1 ms timer resolution while the overlay is
        // running so the UI-only frame limiter can actually honor the slider.
        const bool timerResolutionRaised = (timeBeginPeriod(1) == TIMERR_NOERROR);

        while (!EndFlag)
        {
            const auto frameStart = std::chrono::steady_clock::now();
            if (PeekEndMessage())
                break;
            if (Type == ATTACH)
            {
                if (!UpdateWindowData())
                    break;
            }
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            this->CallBackFn();

            ImGui::Render();
            const float clear_color_with_alpha[4] = { Window.BgColor.Value.x, Window.BgColor.Value.y , Window.BgColor.Value.z, Window.BgColor.Value.w };
            g_Device.g_pd3dDeviceContext->OMSetRenderTargets(1, &g_Device.g_mainRenderTargetView, NULL);
            g_Device.g_pd3dDeviceContext->ClearRenderTargetView(g_Device.g_mainRenderTargetView, clear_color_with_alpha);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            // Present without forcing monitor-vsync so the overlay-only FPS slider
            // can actually control rates above/below the display refresh.
            g_Device.g_pSwapChain->Present(0, 0);

            // Overlay-only frame cap. This does not change CS2's own FPS.
            // Clamp here too so malformed/old config files cannot create extreme values.
            const int fpsLimit = std::clamp(MenuConfig::OverlayFpsLimit, 30, 400);
            const auto target = std::chrono::duration<double>(1.0 / static_cast<double>(fpsLimit));
            const auto deadline = frameStart + std::chrono::duration_cast<std::chrono::steady_clock::duration>(target);

            // Sleep for most of the remaining frame, then yield for the final
            // fraction of a millisecond. This avoids the old ~65 FPS ceiling
            // while keeping CPU usage much lower than a full busy-spin.
            auto now = std::chrono::steady_clock::now();
            constexpr auto fineWindow = std::chrono::microseconds(750);
            if (now + fineWindow < deadline)
                std::this_thread::sleep_until(deadline - fineWindow);
            while ((now = std::chrono::steady_clock::now()) < deadline)
                std::this_thread::yield();
        }

        if (timerResolutionRaised)
            timeEndPeriod(1);
        CleanImGui();
    }

    bool OSImGui_External::CreateMyWindow()
    {
        WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc_External, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, Window.wClassName.c_str(), NULL };
        RegisterClassExW(&wc);
        if (Type == ATTACH)
        {
			DWORD exStyle = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW;
            if (!MenuConfig::ShowMenu) exStyle |= WS_EX_TRANSPARENT;
            Window.hWnd = CreateWindowExW(exStyle, Window.wClassName.c_str(), Window.wName.c_str(), WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, NULL, NULL, GetModuleHandle(NULL), NULL);
			SetLayeredWindowAttributes(Window.hWnd, 0, 255, LWA_ALPHA);
        }
        else
        {
            Window.hWnd = CreateWindowW(Window.wClassName.c_str(), Window.wName.c_str(), WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU, (int)Window.Pos.x, (int)Window.Pos.y, (int)Window.Size.x, (int)Window.Size.y, NULL, NULL, wc.hInstance, NULL);
        }
        Window.hInstance = wc.hInstance;

        if (!g_Device.CreateDeviceD3D(Window.hWnd))
        {
            g_Device.CleanupDeviceD3D();
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        ShowWindow(Window.hWnd, SW_SHOWDEFAULT);
        UpdateWindow(Window.hWnd);

        return Window.hWnd != NULL;
    }

    bool OSImGui_External::UpdateWindowData()
    {
        POINT Point{};
        RECT Rect{};

        // Legacy title/class attach still re-resolves each frame. Direct-handle
        // attachment intentionally keeps the HWND chosen from the cs2.exe PID.
        if (!DestWindow.Name.empty() || !DestWindow.ClassName.empty())
        {
            DestWindow.hWnd = FindWindowA(
                (DestWindow.ClassName.empty() ? NULL : DestWindow.ClassName.c_str()),
                (DestWindow.Name.empty() ? NULL : DestWindow.Name.c_str()));
        }
        if (DestWindow.hWnd == NULL || !IsWindow(DestWindow.hWnd))
            return false;

        GetClientRect(DestWindow.hWnd, &Rect);
        ClientToScreen(DestWindow.hWnd, &Point);

        Window.Pos = DestWindow.Pos = Vec2(static_cast<float>(Point.x), static_cast<float>(Point.y));
        Window.Size = DestWindow.Size = Vec2(static_cast<float>(Rect.right), static_cast<float>(Rect.bottom));

        const HWND foreground = GetForegroundWindow();
        DWORD foregroundPid = 0;
        DWORD targetPid = 0;
        if (foreground)
            GetWindowThreadProcessId(foreground, &foregroundPid);
        GetWindowThreadProcessId(DestWindow.hWnd, &targetPid);

        const DWORD selfPid = GetCurrentProcessId();
        const bool directlyActive =
            foreground == DestWindow.hWnd ||
            foreground == Window.hWnd ||
            GetActiveWindow() == Window.hWnd ||
            (foregroundPid != 0 && (foregroundPid == targetPid || foregroundPid == selfPid));

        // Windows can report a transient/null foreground window while an ImGui
        // control is being clicked or dragged. Hiding the overlay on that one
        // sample creates a self-lock: once hidden it cannot regain focus until
        // CS2 is clicked. Keep a short grace period before hiding instead.
        const ULONGLONG now = GetTickCount64();
        static ULONGLONG lastActiveTick = now;
        if (directlyActive)
            lastActiveTick = now;
        const bool targetActive = directlyActive || (now - lastActiveTick) < 500;

        static bool wasTargetActive = true;
        if (targetActive != wasTargetActive)
        {
            ShowWindow(Window.hWnd, targetActive ? SW_SHOWNOACTIVATE : SW_HIDE);
            wasTargetActive = targetActive;
        }

        static int lastX = INT_MIN, lastY = INT_MIN, lastW = INT_MIN, lastH = INT_MIN;
        const int x = static_cast<int>(Window.Pos.x), y = static_cast<int>(Window.Pos.y);
        const int w = static_cast<int>(Window.Size.x), h = static_cast<int>(Window.Size.y);
        if (x != lastX || y != lastY || w != lastW || h != lastH)
        {
            UINT flags = SWP_NOACTIVATE;
            if (targetActive)
                flags |= SWP_SHOWWINDOW;
            SetWindowPos(Window.hWnd, HWND_TOPMOST, x, y, w, h, flags);
            lastX = x; lastY = y; lastW = w; lastH = h;
        }

        if (MenuConfig::ShowMenu)
        {
            POINT mousePos{};
            GetCursorPos(&mousePos);
            ScreenToClient(Window.hWnd, &mousePos);
            ImGui::GetIO().MousePos.x = static_cast<float>(mousePos.x);
            ImGui::GetIO().MousePos.y = static_cast<float>(mousePos.y);
        }

        LONG_PTR exStyle = GetWindowLongPtrW(Window.hWnd, GWL_EXSTYLE);
        LONG_PTR wantedStyle = exStyle | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
        if (targetActive && MenuConfig::ShowMenu)
            wantedStyle &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
        else
            wantedStyle |= WS_EX_TRANSPARENT;
        if (wantedStyle != exStyle)
            SetWindowLongPtrW(Window.hWnd, GWL_EXSTYLE, wantedStyle);
        return true;
    }

    LRESULT WINAPI WndProc_External(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        switch (msg)
        {
        case WM_CREATE:
        {
            MARGINS     Margin = { -1 };
            DwmExtendFrameIntoClientArea(hWnd, &Margin);
            break;
        }
        case WM_SIZE:
            if (g_Device.g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
            {
                g_Device.CleanupRenderTarget();
                g_Device.g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                g_Device.CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        }
        return ::DefWindowProcW(hWnd, msg, wParam, lParam);
    }

}