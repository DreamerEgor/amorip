#pragma once
#include <cmath>
#include <algorithm>
#include "Game.h"
#include "Entity.h"
#include "MenuConfig.hpp"
#include "Input.hpp"
#include "WeaponProfiles.hpp"
#include "RCSDebug.hpp"

namespace AimControl
{
    inline constexpr float kPi = 3.14159265358979323846f;

    inline float NormalizeYaw(float yaw)
    {
        while (yaw > 180.f) yaw -= 360.f;
        while (yaw < -180.f) yaw += 360.f;
        return yaw;
    }

    inline Vec2 CalcAngle(const Vec3& from, const Vec3& to)
    {
        const Vec3 delta = to - from;
        const float hyp = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        Vec2 out{};
        out.x = -std::atan2(delta.z, hyp) * 180.f / kPi;
        out.y = std::atan2(delta.y, delta.x) * 180.f / kPi;
        return out;
    }

    inline float AngularDistance(const Vec3& from, const Vec3& to, const Vec2& current)
    {
        const Vec2 desired = CalcAngle(from, to);
        const float dp = desired.x - current.x;
        const float dy = NormalizeYaw(desired.y - current.y);
        return std::sqrt(dp * dp + dy * dy);
    }

    inline bool AimAt(const CEntity& local, const Vec3& target)
    {
        if (!local.IsAlive())
            return false;

        Vec2 current{};
        if (!gGame.ReadViewAngle(current))
            return false;

        const Vec2 desired = CalcAngle(local.Pawn.CameraPos, target);
        float pitchDelta = desired.x - current.x;
        float yawDelta = NormalizeYaw(desired.y - current.y);
        const float fov = std::sqrt(pitchDelta * pitchDelta + yawDelta * yawDelta);
        if (!std::isfinite(fov) || fov > MenuConfig::AimFov)
            return false;

        const float sensitivity = gGame.ReadSensitivity();
        const float smooth = std::max(MenuConfig::AimSmooth, 1.f);
        constexpr float yawScale = 0.022f;

        const float mouseX = yawDelta / (sensitivity * yawScale * smooth);
        const float mouseY = -pitchDelta / (sensitivity * yawScale * smooth);
        ExternalInput::MoveMouse(mouseX, mouseY, ExternalInput::MouseChannel::Aim);
        return true;
    }

    inline float AimFovRadiusPixels()
    {
        if (Gui.Window.Size.x <= 0.f)
            return 0.f;
        constexpr float assumedGameFov = 90.f;
        return std::tan(MenuConfig::AimFov * 0.5f * kPi / 180.f) /
            std::tan(assumedGameFov * 0.5f * kPi / 180.f) * (Gui.Window.Size.x * 0.5f);
    }

    inline bool AimAtScreen(const Vec2& targetScreen)
    {
        const Vec2 center(Gui.Window.Size.x * 0.5f, Gui.Window.Size.y * 0.5f);
        const Vec2 delta = targetScreen - center;
        const float radius = AimFovRadiusPixels();
        if (!std::isfinite(delta.x) || !std::isfinite(delta.y) || delta.Length() > radius)
            return false;

        const float smooth = std::max(MenuConfig::AimSmooth, 1.f);
        ExternalInput::MoveMouse(delta.x / smooth, delta.y / smooth, ExternalInput::MouseChannel::Aim);
        return true;
    }

    inline void RunRCS(const CEntity& local)
    {
        // =========================================================================
        // RCS (Recoil Compensation System) with separated pitch/yaw handling
        // =========================================================================
        // CS applies aim punch to the effective view at roughly 2x the stored punch angle.
        // We compensate for this 2x scale to achieve proper recoil suppression.
        // Pitch (vertical) and Yaw (horizontal) are treated independently.
        // =========================================================================

        constexpr float recoilViewScale = 2.0f;        // CS view effect multiplier
        constexpr float mousePitchScale = 0.022f;      // Vertical mouse sensitivity scale
        constexpr float mouseYawScale = 0.022f;        // Horizontal mouse sensitivity scale
        constexpr float maxReasonablePunchStep = 12.0f; // Guard against stale data

        // Persistent state across frames
        static Vec2 previousPunch{};
        static bool havePreviousPunch = false;
        static std::uint16_t previousWeapon = 0;
        static int previousShots = 0;

        // Helper lambda to reset RCS state
        const auto resetState = [&]()
        {
            previousPunch = Vec2{};
            havePreviousPunch = false;
            previousWeapon = 0;
            previousShots = 0;
            ExternalInput::ResetMouseRemainder(ExternalInput::MouseChannel::Recoil);
            RCSDebug::g_RcsState.HasValidData = false;
        };

        // Exit if RCS disabled or player not alive
        if (!MenuConfig::RCS || !local.IsAlive())
        {
            resetState();
            return;
        }

        // Update debug state
        RCSDebug::UpdateRCSState(local);

        // Get current recoil angles
        const Vec2 currentPunch = local.Pawn.AimPunchAngle;
        if (!std::isfinite(currentPunch.x) || !std::isfinite(currentPunch.y))
        {
            resetState();
            return;
        }

        // Get current weapon and shot count
        const std::uint16_t weapon = local.Pawn.WeaponDefinitionIndex;
        const int shots = std::max(local.Pawn.ShotsFired, 0);

        // ---- WEAPON CHANGE DETECTION ----
        // When weapon changes, reset to baseline and don't apply compensation yet.
        // This ensures we start fresh with the new weapon's recoil pattern.
        if (previousWeapon != weapon)
        {
            previousPunch = currentPunch;
            previousWeapon = weapon;
            previousShots = shots;
            havePreviousPunch = true;
            ExternalInput::ResetMouseRemainder(ExternalInput::MouseChannel::Recoil);
            return;
        }

        // ---- BASELINE ESTABLISHMENT ----
        // Always keep a zero/low-shot baseline. This ensures RCS "start bullet"
        // settings work correctly by having known reference points.
        if (!havePreviousPunch || shots < previousShots)
        {
            previousPunch = currentPunch;
            previousWeapon = weapon;
            previousShots = shots;
            havePreviousPunch = true;
            if (shots == 0)
                ExternalInput::ResetMouseRemainder(ExternalInput::MouseChannel::Recoil);
            return;
        }

        // ---- RCS START BULLET CHECK ----
        // Only start compensation after the configured start bullet.
        // This lets users skip initial recoil for weapons with stable early shots.
        if (shots < MenuConfig::RCSStartBullet)
        {
            previousPunch = currentPunch;
            previousWeapon = weapon;
            previousShots = shots;
            if (shots == 0)
                ExternalInput::ResetMouseRemainder(ExternalInput::MouseChannel::Recoil);
            return;
        }

        // ---- DELTA CALCULATION (SEPARATED AXES) ----
        // Calculate pitch (vertical) and yaw (horizontal) changes independently.
        // Pitch: negative values = up recoil, positive = down recoil
        // Yaw: values increase/decrease horizontal aim
        const float pitchChange = currentPunch.x - previousPunch.x;  // Vertical delta
        const float yawChange = NormalizeYaw(currentPunch.y - previousPunch.y);   // Horizontal delta

        // ---- STALE DATA PROTECTION ----
        // A stale pointer or schema transition can inject huge synthetic jumps.
        // Real per-frame recoil steps are far below this guard threshold.
        if (!std::isfinite(pitchChange) || !std::isfinite(yawChange) ||
            std::fabs(pitchChange) > maxReasonablePunchStep ||
            std::fabs(yawChange) > maxReasonablePunchStep)
        {
            ExternalInput::ResetMouseRemainder(ExternalInput::MouseChannel::Recoil);
            previousPunch = currentPunch;
            previousShots = shots;
            return;
        }

        // ---- INSIGNIFICANT DELTA CHECK ----
        // If both changes are negligible, skip mouse movement.
        if (std::fabs(pitchChange) < 0.00001f && std::fabs(yawChange) < 0.00001f)
        {
            previousPunch = currentPunch;
            previousShots = shots;
            return;
        }

        // ---- SENSITIVITY VALIDATION ----
        const float sensitivity = gGame.ReadSensitivity();
        if (!std::isfinite(sensitivity) || sensitivity <= 0.01f)
        {
            previousPunch = currentPunch;
            previousShots = shots;
            return;
        }

        // ---- SMOOTH FACTOR CALCULATION ----
        // Intuitive control: lower smooth = stronger correction, higher = lighter.
        // At 1.0 the full live recoil delta is compensated.
        // At 2.0 half is corrected, etc.
        const float rcsStrength = 1.0f / std::clamp(MenuConfig::RCSSmooth, 1.0f, 12.0f);

        // ---- SEPARATE X/Y MOUSE CORRECTIONS ----
        // Yaw (horizontal, X-axis): convert yaw delta to mouse movement
        //   Negative yaw = left, positive yaw = right
        //   Formula: mouseX = -(yaw_delta * view_scale) / (sensitivity * mouse_scale)
        const float mouseX = -(yawChange * recoilViewScale * rcsStrength) /
            (sensitivity * mouseYawScale);

        // Pitch (vertical, Y-axis): convert pitch delta to mouse movement
        //   Negative pitch (up recoil) = positive mouseY (up movement)
        //   Positive pitch (down recoil) = negative mouseY (down movement)
        //   Formula: mouseY = -(pitch_delta * view_scale) / (sensitivity * mouse_scale)
        const float mouseY = -(pitchChange * recoilViewScale * rcsStrength) /
            (sensitivity * mousePitchScale);

        // ---- DEBUG STATE UPDATE ----
        RCSDebug::g_RcsState.PreviousPitchRecoil = previousPunch.x;
        RCSDebug::g_RcsState.PreviousYawRecoil = previousPunch.y;
        RCSDebug::g_RcsState.CalculatedMouseX = mouseX;
        RCSDebug::g_RcsState.CalculatedMouseY = mouseY;

        // ---- APPLY CORRECTIONS ----
        ExternalInput::MoveMouse(mouseX, mouseY, ExternalInput::MouseChannel::Recoil);

        // ---- UPDATE STATE FOR NEXT FRAME ----
        previousPunch = currentPunch;
        previousWeapon = weapon;
        previousShots = shots;
    }
}
