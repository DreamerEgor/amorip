#pragma once
#include <Windows.h>
#include <chrono>
#include "Entity.h"
#include "Input.hpp"
#include "MenuConfig.hpp"

namespace BunnyHop
{
    namespace Detail
    {
        inline std::chrono::steady_clock::time_point LastAttempt{};
    }

    inline void Reset()
    {
        Detail::LastAttempt = {};
    }

    inline void Run(const CEntity& local)
    {
        if (!MenuConfig::BunnyHop || !local.IsAlive() || !MenuConfig::BunnyHopKey.IsActive())
        {
            Reset();
            return;
        }

        // Prefer the pawn's explicit current-era ground-state field and keep the
        // traditional FL_ONGROUND bit as a fallback. This makes the external less
        // sensitive to short flag timing windows around a landing.
        const bool grounded = local.Pawn.OnGroundLastTick || ((local.Pawn.Flags & 1u) != 0);
        if (!grounded)
            return;

        const auto now = std::chrono::steady_clock::now();
        if (Detail::LastAttempt.time_since_epoch().count() != 0 &&
            now - Detail::LastAttempt < std::chrono::milliseconds(14))
            return;

        // If Space itself is the activation bind it is already physically held, so
        // a normal synthetic key-down may not form a new press edge. Re-arm it with
        // UP->DOWN. For a separate activation bind, a normal Space tap is enough.
        if (MenuConfig::BunnyHopKey.key == VK_SPACE && (GetAsyncKeyState(VK_SPACE) & 0x8000))
            ExternalInput::RetriggerHeldKey(VK_SPACE);
        else
            ExternalInput::TapKey(VK_SPACE);
        Detail::LastAttempt = now;
    }
}
