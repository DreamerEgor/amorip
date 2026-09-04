#include "TriggerBot.h"
#include "MenuConfig.hpp"
#include "Input.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <random>

namespace
{
    std::chrono::steady_clock::time_point g_firstValid{};
    std::uint32_t g_lastTarget = 0xFFFFFFFFu;
    std::mt19937 g_rng{ std::random_device{}() };

    constexpr std::array<std::uint32_t, 5> kTriggerBones{
        BONEINDEX::head,
        BONEINDEX::neck_0,
        BONEINDEX::spine_2,
        BONEINDEX::spine_1,
        BONEINDEX::pelvis
    };

    bool AnyHitboxEnabled()
    {
        return std::any_of(MenuConfig::TriggerHitboxes.begin(), MenuConfig::TriggerHitboxes.end(), [](bool value) {
            return value;
        });
    }

    const CEntity* FindScannedTarget(std::uintptr_t targetAddress, const std::vector<CEntity>& entities)
    {
        for (const auto& entity : entities)
        {
            if (entity.Pawn.Address == targetAddress)
                return &entity;
        }
        return nullptr;
    }

    bool CursorInsideEllipse(const Vec2& cursor, const Vec2& center, float radiusX, float radiusY)
    {
        if (radiusX <= 0.f || radiusY <= 0.f)
            return false;
        const float dx = (cursor.x - center.x) / radiusX;
        const float dy = (cursor.y - center.y) / radiusY;
        return dx * dx + dy * dy <= 1.f;
    }

    bool SelectedHitboxUnderCrosshair(const CEntity& entity)
    {
        const auto& bones = entity.Pawn.BoneData.BonePosList;
        if (bones.size() <= BONEINDEX::head || bones.size() <= BONEINDEX::pelvis)
            return false;

        const auto& headBone = bones[BONEINDEX::head];
        const auto& pelvisBone = bones[BONEINDEX::pelvis];
        if (!headBone.IsVisible || !pelvisBone.IsVisible)
            return false;

        const float bodyHeight = std::clamp(std::fabs(pelvisBone.ScreenPos.y - headBone.ScreenPos.y), 30.f, 700.f);
        const Vec2 crosshair(Gui.Window.Size.x * 0.5f, Gui.Window.Size.y * 0.5f);

        // Screen-space ellipses are only used to classify which part of an already
        // confirmed crosshair target is being intersected. Their dimensions scale
        // with the projected player height so the filter behaves consistently at
        // different distances/resolutions.
        // Slightly generous central hit regions make the trigger responsive at
        // hitbox edges while still requiring the selected body zone to be under
        // the actual crosshair target.
        constexpr std::array<float, 5> radiusXScale{ 0.120f, 0.125f, 0.185f, 0.185f, 0.190f };
        constexpr std::array<float, 5> radiusYScale{ 0.105f, 0.085f, 0.145f, 0.145f, 0.135f };

        for (std::size_t i = 0; i < kTriggerBones.size(); ++i)
        {
            if (!MenuConfig::TriggerHitboxes[i])
                continue;

            const std::uint32_t boneIndex = kTriggerBones[i];
            if (boneIndex >= bones.size() || !bones[boneIndex].IsVisible)
                continue;

            if (CursorInsideEllipse(crosshair, bones[boneIndex].ScreenPos,
                bodyHeight * radiusXScale[i], bodyHeight * radiusYScale[i]))
                return true;
        }

        return false;
    }
}

void TriggerBot::Reset()
{
    g_firstValid = {};
    g_lastTarget = 0xFFFFFFFFu;
}

void TriggerBot::Run(const CEntity& localEntity, const std::vector<CEntity>& entities)
{
    if (!localEntity.IsAlive() || !AnyHitboxEnabled())
    {
        Reset();
        return;
    }

    const std::uint32_t index = localEntity.Pawn.CrosshairEntityIndex;
    if (index == 0 || index == 0xFFFFFFFFu || index > 0x7FFFu)
    {
        Reset();
        return;
    }

    const std::uintptr_t target = gGame.ResolveEntity(index);
    if (!target)
    {
        Reset();
        return;
    }

    int health = 0;
    int team = 0;
    if (!ProcessMgr.ReadMemory(target + Offset::Entity.Health, health) ||
        !ProcessMgr.ReadMemory(target + Offset::Entity.TeamID, team) ||
        health <= 0 || health > 100 || (MenuConfig::TeamCheck && team == localEntity.Pawn.TeamID))
    {
        Reset();
        return;
    }

    // Always classify the crosshair against the configured hitboxes. The old
    // all-enabled shortcut allowed arm/leg intersections to fire even though
    // those zones were not present in the hitbox selector.
    const CEntity* scannedTarget = FindScannedTarget(target, entities);
    if (!scannedTarget || !SelectedHitboxUnderCrosshair(*scannedTarget))
    {
        Reset();
        return;
    }

    if (MenuConfig::TriggerSnipersScopedOnly && localEntity.Pawn.IsSniperWeapon() && !localEntity.Pawn.IsScoped)
    {
        Reset();
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (g_lastTarget != index)
    {
        g_lastTarget = index;
        g_firstValid = now;
    }

    if (g_firstValid.time_since_epoch().count() == 0)
        g_firstValid = now;

    if (now - g_firstValid < std::chrono::milliseconds(MenuConfig::TriggerDelay))
        return;

    if (MenuConfig::TriggerHitchance < 100)
    {
        std::uniform_int_distribution<int> roll(1, 100);
        if (roll(g_rng) > MenuConfig::TriggerHitchance)
        {
            // Do not impose the full reaction delay again after a chance miss.
            // A short re-arm keeps lower hitchance values usable instead of making
            // the trigger feel randomly unresponsive.
            g_firstValid = now - std::chrono::milliseconds(std::max(MenuConfig::TriggerDelay - 12, 0));
            return;
        }
    }

    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
        ExternalInput::ClickLeft();
    g_firstValid = now;
}
