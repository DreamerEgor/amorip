#include "OS-ImGui_Base.h"
#include <Windows.h>
#include <filesystem>

namespace OSImGui
{
    bool OSImGui_Base::InitImGui(ID3D11Device* device, ID3D11DeviceContext* device_context)
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.LogFilename = nullptr;
        io.IniFilename = nullptr; // Keep imgui.ini from being created next to the EXE.

        char windowsDir[MAX_PATH]{};
        if (GetWindowsDirectoryA(windowsDir, MAX_PATH))
        {
            std::filesystem::path arial = std::filesystem::path(windowsDir) / "Fonts" / "arial.ttf";
            ImFont* font = io.Fonts->AddFontFromFileTTF(arial.string().c_str(), 15.5f);
            if (font)
                io.FontDefault = font;
        }
        if (io.Fonts->Fonts.empty())
            io.Fonts->AddFontDefault();

        ImGui::StyleColorsDark();
        if (!ImGui_ImplWin32_Init(Window.hWnd))
            throw OSException("ImGui_ImplWin32_Init() call failed.");
        if (!ImGui_ImplDX11_Init(device, device_context))
            throw OSException("ImGui_ImplDX11_Init() call failed.");
        return true;
    }

    void OSImGui_Base::CleanImGui()
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_Device.CleanupDeviceD3D();
        DestroyWindow(Window.hWnd);
        UnregisterClassW(Window.wClassName.c_str(), Window.hInstance);
    }

    std::wstring OSImGui_Base::StringToWstring(std::string& str)
    {
        if (str.empty())
            return {};

        const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            str.data(), static_cast<int>(str.size()), nullptr, 0);
        if (required <= 0)
            return {};

        std::wstring result(static_cast<size_t>(required), L'\0');
        const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            str.data(), static_cast<int>(str.size()), result.data(), required);
        if (written <= 0)
            return {};

        return result;
    }
}
