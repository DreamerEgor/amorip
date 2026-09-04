#pragma once
#include <Windows.h>
#include <cmath>
#include <array>
#include <cstddef>

#ifndef MOUSEEVENTF_MOVE_NOCOALESCE
#define MOUSEEVENTF_MOVE_NOCOALESCE 0x2000
#endif

namespace ExternalInput
{
    enum class MouseChannel : std::size_t
    {
        General = 0,
        Aim,
        Recoil,
        Count
    };

    struct MouseRemainder
    {
        double X = 0.0;
        double Y = 0.0;
    };

    inline std::array<MouseRemainder, static_cast<std::size_t>(MouseChannel::Count)> g_MouseRemainders{};

    inline void ResetMouseRemainder(MouseChannel channel)
    {
        g_MouseRemainders[static_cast<std::size_t>(channel)] = {};
    }

    inline void MoveMouse(float dx, float dy, MouseChannel channel = MouseChannel::General)
    {
        if (!std::isfinite(dx) || !std::isfinite(dy))
            return;

        // Aim and recoil use separate fractional accumulators. Sharing one accumulator
        // let a tiny aimbot remainder leak into RCS (and vice versa), which could add
        // one-count side steps during a spray.
        auto& remainder = g_MouseRemainders[static_cast<std::size_t>(channel)];
        const double accumulatedX = static_cast<double>(dx) + remainder.X;
        const double accumulatedY = static_cast<double>(dy) + remainder.Y;
        const LONG moveX = static_cast<LONG>(std::lround(accumulatedX));
        const LONG moveY = static_cast<LONG>(std::lround(accumulatedY));
        remainder.X = accumulatedX - static_cast<double>(moveX);
        remainder.Y = accumulatedY - static_cast<double>(moveY);

        if (moveX == 0 && moveY == 0)
            return;

        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
        input.mi.dx = moveX;
        input.mi.dy = moveY;
        SendInput(1, &input, sizeof(INPUT));
    }

    inline void ClickLeft()
    {
        INPUT inputs[2]{};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(2, inputs, sizeof(INPUT));
    }


    inline void SendKeyEdge(WORD virtualKey, bool keyUp)
    {
        const WORD scan = static_cast<WORD>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC));
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        if (scan)
        {
            input.ki.wScan = scan;
            input.ki.dwFlags = KEYEVENTF_SCANCODE | (keyUp ? KEYEVENTF_KEYUP : 0);
        }
        else
        {
            input.ki.wVk = virtualKey;
            input.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
        }
        SendInput(1, &input, sizeof(INPUT));
    }

    // Retrigger a key that may already be physically held. Ending on KEYDOWN keeps
    // the in-game state consistent with the user's held key while still creating a
    // fresh edge for jump handling. This is only called while CS2 is foreground.
    inline void RetriggerHeldKey(WORD virtualKey)
    {
        SendKeyEdge(virtualKey, true);
        SendKeyEdge(virtualKey, false);
    }

    inline void TapKey(WORD virtualKey)
    {
        SendKeyEdge(virtualKey, false);
        SendKeyEdge(virtualKey, true);
    }
}
