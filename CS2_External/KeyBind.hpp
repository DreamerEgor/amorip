#pragma once
#include <Windows.h>
#include <array>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace Keybind
{
    enum class Mode : int
    {
        Hold = 0,
        Toggle = 1,
        Always = 2
    };

    struct Bind
    {
        int key = 0;
        Mode mode = Mode::Hold;
        bool toggled = false;
        bool previousDown = false;

        bool IsActive()
        {
            if (mode == Mode::Always)
                return true;
            if (key <= 0 || key > 255)
                return false;

            const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
            if (mode == Mode::Toggle && down && !previousDown)
                toggled = !toggled;
            previousDown = down;

            return mode == Mode::Hold ? down : toggled;
        }

        void ResetRuntime()
        {
            toggled = false;
            previousDown = false;
        }
    };

    inline std::string KeyName(int vk)
    {
        if (vk == 0)
            return "none";

        switch (vk)
        {
        case VK_LBUTTON: return "mouse 1";
        case VK_RBUTTON: return "mouse 2";
        case VK_MBUTTON: return "mouse 3";
        case VK_XBUTTON1: return "mouse 4";
        case VK_XBUTTON2: return "mouse 5";
        case VK_SHIFT: return "shift";
        case VK_CONTROL: return "ctrl";
        case VK_MENU: return "alt";
        case VK_INSERT: return "insert";
        case VK_HOME: return "home";
        case VK_END: return "end";
        case VK_DELETE: return "delete";
        case VK_SPACE: return "space";
        case VK_TAB: return "tab";
        case VK_ESCAPE: return "escape";
        default: break;
        }

        UINT scanCode = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
        LONG lParam = static_cast<LONG>(scanCode << 16);
        switch (vk)
        {
        case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
        case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
        case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
            lParam |= (1 << 24);
            break;
        default:
            break;
        }

        char buffer[64]{};
        if (GetKeyNameTextA(lParam, buffer, static_cast<int>(sizeof(buffer))) > 0)
        {
            std::string name(buffer);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return name;
        }

        char fallback[16]{};
        sprintf_s(fallback, "vk %02X", vk);
        return fallback;
    }

    inline const char* ModeName(Mode mode)
    {
        switch (mode)
        {
        case Mode::Hold: return "hold";
        case Mode::Toggle: return "toggle";
        case Mode::Always: return "always";
        default: return "hold";
        }
    }
}
