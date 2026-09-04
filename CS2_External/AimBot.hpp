#pragma once
#include <cmath>
#include <algorithm>
#include "Game.h"
#include "Entity.h"
#include "MenuConfig.hpp"
#include "Input.hpp"

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
        // CS applies aim punch to the effective view at roughly 2x the stored
        // punch angle. Compensating only 1x is why the previous implementation
        // still climbed during longer sprays.
        constexpr float recoilViewScale = 2.0f;
        constexpr float mousePitchScale = 0.022f;
        constexpr float mouseYawScale = 0.022f;
        constexpr float maxReasonablePunchStep = 12.0f;

        static Vec2 previousPunch{};
        static bool havePreviousPunch = false;
        static std::uint16_t previousWeapon = 0;
        static int previousShots = 0;

        const auto resetState = [&]()
        {
            previousPunch = Vec2{};
            havePreviousPunch = false;
            previousWeapon = 0;
            previousShots = 0;
            ExternalInput::ResetMouseRemainder(ExternalInput::MouseChannel::Recoil);
        };

        if (!MenuConfig::RCS || !local.IsAlive())
        {
            resetState();
            return;
        }

        const Vec2 currentPunch = local.Pawn.AimPunchAngle;
        if (!std::isfinite(currentPunch.x) || !std::isfinite(currentPunch.y))
        {
            resetState();
            return;
        }

        const std::uint16_t weapon = local.Pawn.WeaponDefinitionIndex;
        const int shots = std::max(local.Pawn.ShotsFired, 0);

        // Always keep a zero/low-shot baseline. This makes RCSStartBullet exact:
        // start bullet 2 now compensates the second shot instead of silently
        // waiting until the third punch update.
        if (!havePreviousPunch || previousWeapon != weapon || shots < previousShots)
        {
            previousPunch = currentPunch;
            previousWeapon = weapon;
            previousShots = shots;
            havePreviousPunch = true;

            if (shots == 0)
                ExternalInput::ResetMouseRemainder(ExternalInput::MouseChannel::Recoil);
            return;
        }

        if (shots < MenuConfig::RCSStartBullet)
        {
            previousPunch = currentPunch;
            previousWeapon = weapon;
            previousShots = shots;
            if (shots == 0)
                ExternalInput::ResetMouseRemainder(ExternalInput::MouseChannel::Recoil);
            return;
        }

        const float pitchChange = currentPunch.x - previousPunch.x;
        const float yawChange = NormalizeYaw(currentPunch.y - previousPunch.y);

        previousPunch = currentPunch;
        previousWeapon = weapon;
        previousShots = shots;

        // A stale pointer/schema transition can otherwise inject a huge synthetic
        // mouse jump. Real per-frame recoil steps are far below this guard.
        if (!std::isfinite(pitchChange) || !std::isfinite(yawChange) ||
            std::fabs(pitchChange) > maxReasonablePunchStep ||
            std::fabs(yawChange) > maxReasonablePunchStep)
        {
            ExternalInput::ResetMouseRemainder(ExternalInput::MouseChannel::Recoil);
            return;
        }

        if (std::fabs(pitchChange) < 0.00001f && std::fabs(yawChange) < 0.00001f)
            return;

        const float sensitivity = gGame.ReadSensitivity();
        if (!std::isfinite(sensitivity) || sensitivity <= 0.01f)
            return;

        // One intuitive RCS control: lower smooth = stronger correction, higher
        // smooth = lighter correction. At 1.0 the full live recoil delta is
        // compensated; at 2.0 half is corrected, and so on.
        const float rcsStrength = 1.0f / std::clamp(MenuConfig::RCSSmooth, 1.0f, 12.0f);
        const float mouseX = -(yawChange * recoilViewScale * rcsStrength) /
            (sensitivity * mouseYawScale);
        const float mouseY = -(pitchChange * recoilViewScale * rcsStrength) /
            (sensitivity * mousePitchScale);

        ExternalInput::MoveMouse(mouseX, mouseY, ExternalInput::MouseChannel::Recoil);
    }

}
