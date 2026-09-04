#include <Windows.h>
#include <windowsx.h>
#include <TlHelp32.h>
#include <filesystem>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <shellapi.h>
#include "Offsets.h"
#include "Cheats.h"
#include "MenuConfig.hpp"
#include "Utils/ConfigSaver.hpp"
#include "Utils/ProcessManager.hpp"
#include "Game.h"
#include "OS-ImGui/OS-ImGui.h"
#include "resource.h"

#pragma comment(lib, "Shell32.lib")

namespace fs = std::filesystem;

namespace
{
    // -------------------------------------------------------------------------
    // Integrated launcher UI.
    // Dark subscription-style layout with #00CD91 as the primary accent.
    // -------------------------------------------------------------------------
    constexpr int kLauncherWidth = 620;
    constexpr int kLauncherHeight = 340;
    constexpr COLORREF kAccent = RGB(0, 205, 145); // #00CD91
    constexpr COLORREF kBg = RGB(5, 14, 19);
    constexpr COLORREF kSidebar = RGB(6, 18, 24);
    constexpr COLORREF kPanel = RGB(7, 22, 28);
    constexpr COLORREF kPanelHover = RGB(8, 28, 34);
    constexpr COLORREF kBorder = RGB(8, 58, 51);
    constexpr COLORREF kText = RGB(237, 244, 242);
    constexpr COLORREF kMuted = RGB(126, 145, 146);
    constexpr COLORREF kMutedBright = RGB(166, 182, 181);

    RECT g_websiteLink{ 18, 88, 140, 120 };
    RECT g_supportLink{ 18, 126, 140, 158 };
    RECT g_launchButton{ 420, 220, 585, 254 };
    RECT g_minButton{ 544, 12, 576, 42 };
    RECT g_closeButton{ 580, 12, 612, 42 };
    RECT g_subscriptionCard{ 175, 96, 590, 190 };

    bool g_cs2Running = false;
    bool g_launchRequested = false;
    HBITMAP g_logoBitmap = nullptr;

    HFONT g_brandFont = nullptr;
    HFONT g_headerFont = nullptr;
    HFONT g_titleFont = nullptr;
    HFONT g_bodyFont = nullptr;
    HFONT g_smallFont = nullptr;
    HFONT g_buttonFont = nullptr;

    enum class HoverTarget
    {
        None,
        Website,
        Support,
        Launch,
        Minimize,
        Close
    };

    HoverTarget g_hoverTarget = HoverTarget::None;

    bool ProcessExists(const wchar_t* name)
    {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
            return false;

        bool found = false;
        if (Process32FirstW(snap, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szExeFile, name) == 0)
                {
                    found = true;
                    break;
                }
            } while (Process32NextW(snap, &entry));
        }

        CloseHandle(snap);
        return found;
    }

    void RoundedRectFill(HDC dc, const RECT& r, COLORREF color, int radius)
    {
        HBRUSH brush = CreateSolidBrush(color);
        HPEN pen = CreatePen(PS_NULL, 0, color);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
    }

    void RoundedRectOutline(HDC dc, const RECT& r, COLORREF color, int radius, int width = 1)
    {
        HPEN pen = CreatePen(PS_SOLID, width, color);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }

    void DrawTextSimple(HDC dc, const wchar_t* text, RECT r, COLORREF color, HFONT font, UINT format)
    {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        HGDIOBJ old = SelectObject(dc, font);
        DrawTextW(dc, text, -1, &r, format);
        SelectObject(dc, old);
    }

    void DrawLogo(HDC dc, const RECT& target)
    {
        if (!g_logoBitmap)
            return;

        BITMAP bmp{};
        if (!GetObjectW(g_logoBitmap, sizeof(bmp), &bmp))
            return;

        HDC src = CreateCompatibleDC(dc);
        if (!src)
            return;

        HGDIOBJ old = SelectObject(src, g_logoBitmap);
        const int oldMode = SetStretchBltMode(dc, HALFTONE);
        StretchBlt(dc,
            target.left, target.top,
            target.right - target.left, target.bottom - target.top,
            src,
            0, 0, bmp.bmWidth, bmp.bmHeight,
            SRCCOPY);
        SetStretchBltMode(dc, oldMode);
        SelectObject(src, old);
        DeleteDC(src);
    }

    void OpenUrl(const wchar_t* url)
    {
        ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
    }

    HoverTarget HitTarget(POINT p)
    {
        if (PtInRect(&g_closeButton, p))
            return HoverTarget::Close;
        if (PtInRect(&g_minButton, p))
            return HoverTarget::Minimize;
        if (PtInRect(&g_websiteLink, p))
            return HoverTarget::Website;
        if (PtInRect(&g_supportLink, p))
            return HoverTarget::Support;
        if (g_cs2Running && PtInRect(&g_launchButton, p))
            return HoverTarget::Launch;
        return HoverTarget::None;
    }

    LRESULT CALLBACK LauncherWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
        case WM_CREATE:
        {
            g_brandFont = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_headerFont = CreateFontW(24, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_titleFont = CreateFontW(18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_bodyFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_smallFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_buttonFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

            g_logoBitmap = static_cast<HBITMAP>(LoadImageW(
                GetModuleHandleW(nullptr),
                MAKEINTRESOURCEW(IDB_CS2_LOGO),
                IMAGE_BITMAP,
                0, 0,
                LR_CREATEDIBSECTION));

            HRGN rounded = CreateRoundRectRgn(0, 0, kLauncherWidth + 1, kLauncherHeight + 1, 22, 22);
            if (rounded)
                SetWindowRgn(hwnd, rounded, TRUE);

            SetTimer(hwnd, 1, 550, nullptr);
            g_cs2Running = ProcessExists(L"cs2.exe");
            return 0;
        }

        case WM_TIMER:
        {
            const bool old = g_cs2Running;
            g_cs2Running = ProcessExists(L"cs2.exe");
            if (old != g_cs2Running)
                InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            POINT p{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            const HoverTarget next = HitTarget(p);
            if (next != g_hoverTarget)
            {
                g_hoverTarget = next;
                InvalidateRect(hwnd, nullptr, FALSE);
            }

            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);

            SetCursor(LoadCursor(nullptr,
                g_hoverTarget == HoverTarget::None ? IDC_ARROW : IDC_HAND));
            return 0;
        }

        case WM_MOUSELEAVE:
            g_hoverTarget = HoverTarget::None;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
        {
            POINT p{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            if (p.y < 58 && !PtInRect(&g_minButton, p) && !PtInRect(&g_closeButton, p))
            {
                ReleaseCapture();
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }
            return 0;
        }

        case WM_LBUTTONUP:
        {
            POINT p{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            switch (HitTarget(p))
            {
            case HoverTarget::Website:
                OpenUrl(L"https://amor.rip");
                break;
            case HoverTarget::Support:
                OpenUrl(L"https://t.me/amorogu");
                break;
            case HoverTarget::Launch:
                g_launchRequested = true;
                DestroyWindow(hwnd);
                break;
            case HoverTarget::Minimize:
                ShowWindow(hwnd, SW_MINIMIZE);
                break;
            case HoverTarget::Close:
                DestroyWindow(hwnd);
                break;
            default:
                break;
            }
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC windowDc = BeginPaint(hwnd, &ps);
            RECT client{};
            GetClientRect(hwnd, &client);

            HDC dc = CreateCompatibleDC(windowDc);
            HBITMAP backBuffer = CreateCompatibleBitmap(windowDc,
                client.right - client.left, client.bottom - client.top);
            HGDIOBJ oldBackBuffer = SelectObject(dc, backBuffer);

            HBRUSH bg = CreateSolidBrush(kBg);
            FillRect(dc, &client, bg);
            DeleteObject(bg);

            RECT sidebar{ 0, 0, 155, kLauncherHeight };
            HBRUSH sidebarBrush = CreateSolidBrush(kSidebar);
            FillRect(dc, &sidebar, sidebarBrush);
            DeleteObject(sidebarBrush);

            // Compact brand mark, inspired by the reference layout.
            RECT brand{ 24, 18, 138, 58 };
            DrawTextSimple(dc, L"amor", brand, kText, g_brandFont,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Navigation
            if (g_hoverTarget == HoverTarget::Website)
                RoundedRectFill(dc, g_websiteLink, RGB(8, 30, 32), 10);
            if (g_hoverTarget == HoverTarget::Support)
                RoundedRectFill(dc, g_supportLink, RGB(8, 30, 32), 10);

            RECT websiteText{ 28, 88, 138, 120 };
            DrawTextSimple(dc, L"Website", websiteText,
                g_hoverTarget == HoverTarget::Website ? kAccent : kMutedBright,
                g_bodyFont, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT supportText{ 28, 126, 138, 158 };
            DrawTextSimple(dc, L"Support", supportText,
                g_hoverTarget == HoverTarget::Support ? kAccent : kMutedBright,
                g_bodyFont, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Minimal account label.
            RECT admin{ 28, 292, 138, 322 };
            DrawTextSimple(dc, L"Admin", admin, kText, g_titleFont,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Header
            RECT header{ 190, 20, 485, 50 };
            DrawTextSimple(dc, L"Subscriptions", header, kText, g_headerFont,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT subHeader{ 190, 49, 485, 75 };
            DrawTextSimple(dc, L"Active subscriptions", subHeader, kMuted,
                g_bodyFont, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Window controls
            if (g_hoverTarget == HoverTarget::Minimize)
                RoundedRectFill(dc, g_minButton, RGB(12, 34, 38), 9);
            if (g_hoverTarget == HoverTarget::Close)
                RoundedRectFill(dc, g_closeButton, RGB(44, 25, 29), 9);

            HPEN controlPen = CreatePen(PS_SOLID, 2, kText);
            HGDIOBJ oldControlPen = SelectObject(dc, controlPen);
            MoveToEx(dc, 552, 27, nullptr);
            LineTo(dc, 568, 27);
            MoveToEx(dc, 589, 20, nullptr);
            LineTo(dc, 603, 34);
            MoveToEx(dc, 603, 20, nullptr);
            LineTo(dc, 589, 34);
            SelectObject(dc, oldControlPen);
            DeleteObject(controlPen);

            // Subscription card
            RoundedRectFill(dc, g_subscriptionCard,
                g_hoverTarget == HoverTarget::Launch ? kPanelHover : kPanel, 18);
            RoundedRectOutline(dc, g_subscriptionCard, kBorder, 18, 1);

            RECT accentBar{ 175, 107, 179, 179 };
            RoundedRectFill(dc, accentBar, kAccent, 5);

            RECT gameTitle{ 198, 105, 430, 131 };
            DrawTextSimple(dc, L"Counter-Strike: 2", gameTitle, kText, g_titleFont,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT activePill{ 198, 136, 254, 158 };
            RoundedRectFill(dc, activePill, RGB(7, 47, 40), 10);
            RECT activeText = activePill;
            DrawTextSimple(dc, L"ACTIVE", activeText, kAccent, g_smallFont,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            RECT gameStatus{ 198, 162, 440, 184 };
            DrawTextSimple(dc,
                g_cs2Running ? L"Counter-Strike 2 detected" : L"Start Counter-Strike 2 first",
                gameStatus,
                g_cs2Running ? kMutedBright : RGB(190, 142, 128),
                g_smallFont, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT logoBox{ 521, 119, 559, 157 };
            DrawLogo(dc, logoBox);

            // Launch button
            const COLORREF launchColor = g_cs2Running
                ? (g_hoverTarget == HoverTarget::Launch ? RGB(5, 226, 162) : kAccent)
                : RGB(34, 54, 56);
            RoundedRectFill(dc, g_launchButton, launchColor, 12);

            RECT launchText = g_launchButton;
            DrawTextSimple(dc,
                g_cs2Running ? L"Launch" : L"Waiting for CS2",
                launchText,
                g_cs2Running ? RGB(2, 23, 19) : RGB(111, 132, 131),
                g_buttonFont, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            BitBlt(windowDc, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);

            SelectObject(dc, oldBackBuffer);
            DeleteObject(backBuffer);
            DeleteDC(dc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            if (g_logoBitmap)
            {
                DeleteObject(g_logoBitmap);
                g_logoBitmap = nullptr;
            }
            for (HFONT* font : { &g_brandFont, &g_headerFont, &g_titleFont, &g_bodyFont, &g_smallFont, &g_buttonFont })
            {
                if (*font)
                {
                    DeleteObject(*font);
                    *font = nullptr;
                }
            }
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    bool RunLauncher(HINSTANCE instance, int show)
    {
        g_launchRequested = false;
        g_cs2Running = false;
        g_hoverTarget = HoverTarget::None;

        const wchar_t* className = L"CS2ExternalLauncherWindow";
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc = LauncherWndProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = className;

        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;

        const int screenW = GetSystemMetrics(SM_CXSCREEN);
        const int screenH = GetSystemMetrics(SM_CYSCREEN);
        const int x = (screenW - kLauncherWidth) / 2;
        const int y = (screenH - kLauncherHeight) / 2;

        HWND hwnd = CreateWindowExW(
            WS_EX_APPWINDOW,
            className,
            L"amor.rip",
            WS_POPUP,
            x,
            y,
            kLauncherWidth,
            kLauncherHeight,
            nullptr,
            nullptr,
            instance,
            nullptr);

        if (!hwnd)
            return false;

        ShowWindow(hwnd, show == SW_HIDE ? SW_SHOW : show);
        UpdateWindow(hwnd);

        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        UnregisterClassW(className, instance);
        return g_launchRequested;
    }

    fs::path ExecutableDirectory()
    {
        char path[MAX_PATH]{};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        return fs::path(path).parent_path();
    }

    // Keep config/log files out of the Release folder so the app folder stays clean.
    fs::path DataDirectory()
    {
        char* localAppData = nullptr;
        size_t len = 0;
        if (_dupenv_s(&localAppData, &len, "LOCALAPPDATA") == 0 && localAppData)
        {
            fs::path result = fs::path(localAppData) / "amor.rip";
            free(localAppData);
            return result;
        }
        return ExecutableDirectory() / "amor.rip";
    }

    int Fail(const char* message)
    {
        MessageBoxA(nullptr, message, "amor.rip", MB_OK | MB_ICONERROR);
        return 1;
    }

    struct WindowSearch
    {
        DWORD pid = 0;
        HWND best = nullptr;
        std::uint64_t bestArea = 0;
    };

    BOOL CALLBACK FindProcessWindowProc(HWND hwnd, LPARAM lParam)
    {
        auto* search = reinterpret_cast<WindowSearch*>(lParam);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != search->pid || !IsWindowVisible(hwnd) || IsIconic(hwnd))
            return TRUE;
        if (GetWindow(hwnd, GW_OWNER) != nullptr)
            return TRUE;

        const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if ((exStyle & WS_EX_TOOLWINDOW) != 0)
            return TRUE;

        RECT client{};
        if (!GetClientRect(hwnd, &client))
            return TRUE;
        const auto width = static_cast<std::uint64_t>(client.right > client.left ? client.right - client.left : 0);
        const auto height = static_cast<std::uint64_t>(client.bottom > client.top ? client.bottom - client.top : 0);
        const auto area = width * height;
        if (width < 320 || height < 200 || area <= search->bestArea)
            return TRUE;

        search->best = hwnd;
        search->bestArea = area;
        return TRUE;
    }

    HWND FindMainProcessWindow(DWORD pid)
    {
        WindowSearch search{};
        search.pid = pid;
        EnumWindows(FindProcessWindowProc, reinterpret_cast<LPARAM>(&search));
        return search.best;
    }

    void WriteStartupLog(const fs::path& dir, const std::string& line)
    {
        std::error_code ec;
        fs::create_directories(dir, ec);
        std::ofstream f(dir / "startup.log", std::ios::app);
        if (f)
            f << line << '\n';
    }

    std::string DescribeWindow(HWND hwnd)
    {
        char title[256]{};
        char cls[256]{};
        GetWindowTextA(hwnd, title, static_cast<int>(sizeof(title)));
        GetClassNameA(hwnd, cls, static_cast<int>(sizeof(cls)));
        RECT r{};
        GetClientRect(hwnd, &r);
        std::ostringstream ss;
        ss << "target hwnd=0x" << std::hex << reinterpret_cast<std::uintptr_t>(hwnd) << std::dec
           << " class=\"" << cls << "\" title=\"" << title << "\" client="
           << (r.right - r.left) << 'x' << (r.bottom - r.top);
        return ss.str();
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    // The launcher now runs inside this same executable. Closing the launcher
    // exits; pressing Launch continues directly into the existing program.
    if (!RunLauncher(instance, show))
        return 0;

    MenuConfig::path = DataDirectory().string();
    std::error_code ec;
    fs::create_directories(MenuConfig::path, ec);
    WriteStartupLog(MenuConfig::path, "---- cs2 external startup ----");

    if (ProcessMgr.Attach("cs2.exe") != StatusCode::SUCCEED)
        return Fail("CS2 was not found. Start the game first, then open cs2-external again.");

    std::string offsetStatus;
    if (!Offset::UpdateOffsets(MenuConfig::path, &offsetStatus))
        return Fail(("Could not load current CS2 offsets.\n\n" + offsetStatus).c_str());

    if (!gGame.InitAddress())
        return Fail("Could not initialize client.dll addresses.");

    const HWND gameWindow = FindMainProcessWindow(ProcessMgr.ProcessID);
    if (!gameWindow)
        return Fail("Could not find the visible CS2 game window. Make sure CS2 is fully open (not just starting/minimized).");
    WriteStartupLog(MenuConfig::path, DescribeWindow(gameWindow));

    const fs::path defaultConfig = fs::path(MenuConfig::path) / "default.cfg";
    if (fs::exists(defaultConfig))
        MyConfigSaver::LoadConfig("default.cfg");
    else
        MyConfigSaver::SaveConfig("default.cfg");

    try
    {
        WriteStartupLog(MenuConfig::path, "creating overlay");
        Gui.AttachWindowHandle(gameWindow, Cheats::Run);
        WriteStartupLog(MenuConfig::path, "overlay loop ended");
    }
    catch (const OSImGui::OSException& e)
    {
        ProcessMgr.Detach();
        return Fail(e.what());
    }

    ProcessMgr.Detach();
    return 0;
}
