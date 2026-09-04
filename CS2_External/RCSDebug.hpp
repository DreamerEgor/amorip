#pragma once
#include <cstdint>
#include <string>
#include <cstdio>
#include "Game.h"
#include "Entity.h"
#include "WeaponProfiles.hpp"

namespace RCSDebug
{
    struct RCSState
    {
        // Current frame data
        std::uint16_t CurrentWeapon = 0;
        float CurrentPitchRecoil = 0.f;
        float CurrentYawRecoil = 0.f;
        int CurrentShotsFired = 0;

        // Previous frame data for delta calculation
        float PreviousPitchRecoil = 0.f;
        float PreviousYawRecoil = 0.f;
        int PreviousShotsFired = 0;
        std::uint16_t PreviousWeapon = 0;

        // Calculated corrections
        float CalculatedMouseX = 0.f;  // Yaw correction (horizontal)
        float CalculatedMouseY = 0.f;  // Pitch correction (vertical)

        // Debug state
        bool HasValidData = false;
        bool WeaponChanged = false;
    };

    inline RCSState g_RcsState{};

    inline void UpdateRCSState(const CEntity& local)
    {
        // Detect weapon change
        const std::uint16_t newWeapon = local.Pawn.WeaponDefinitionIndex;
        if (newWeapon != g_RcsState.PreviousWeapon)
        {
            g_RcsState.WeaponChanged = true;
            g_RcsState.PreviousWeapon = newWeapon;
            g_RcsState.CurrentWeapon = newWeapon;
            g_RcsState.PreviousPitchRecoil = local.Pawn.AimPunchAngle.x;
            g_RcsState.PreviousYawRecoil = local.Pawn.AimPunchAngle.y;
            g_RcsState.PreviousShotsFired = local.Pawn.ShotsFired;
            g_RcsState.HasValidData = false;
            return;
        }

        g_RcsState.WeaponChanged = false;
        g_RcsState.CurrentWeapon = newWeapon;
        g_RcsState.CurrentPitchRecoil = local.Pawn.AimPunchAngle.x;
        g_RcsState.CurrentYawRecoil = local.Pawn.AimPunchAngle.y;
        g_RcsState.CurrentShotsFired = local.Pawn.ShotsFired;
        g_RcsState.HasValidData = true;
    }

    inline std::string DebugInfo()
    {
        char buffer[512];

        const char* weaponName = WeaponProfiles::GetWeaponName(g_RcsState.CurrentWeapon);
        const float pitchDelta = g_RcsState.CurrentPitchRecoil - g_RcsState.PreviousPitchRecoil;
        const float yawDelta = g_RcsState.CurrentYawRecoil - g_RcsState.PreviousYawRecoil;

        std::snprintf(buffer, sizeof(buffer),
            "RCS Debug: Weapon=%s (idx=%u) | Pitch: cur=%.3f prev=%.3f delta=%.3f | "
            "Yaw: cur=%.3f prev=%.3f delta=%.3f | Shots: %d | "
            "Mouse: X=%.2f Y=%.2f | Changed=%s | Valid=%s",
            weaponName,
            g_RcsState.CurrentWeapon,
            g_RcsState.CurrentPitchRecoil,
            g_RcsState.PreviousPitchRecoil,
            pitchDelta,
            g_RcsState.CurrentYawRecoil,
            g_RcsState.PreviousYawRecoil,
            yawDelta,
            g_RcsState.CurrentShotsFired,
            g_RcsState.CalculatedMouseX,
            g_RcsState.CalculatedMouseY,
            g_RcsState.WeaponChanged ? "YES" : "no",
            g_RcsState.HasValidData ? "YES" : "no"
        );

        return std::string(buffer);
    }
}
