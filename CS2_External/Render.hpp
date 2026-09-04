#pragma once
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include "Entity.h"
#include "MenuConfig.hpp"
#include "VisibilityCheck.hpp"
#include "OS-ImGui/OS-ImGui.h"

namespace Render
{
    struct Box2D
    {
        Vec2 Pos{};
        Vec2 Size{};
        bool Valid = false;
    };

    inline bool ValidScreenPoint(const Vec2& p)
    {
        return std::isfinite(p.x) && std::isfinite(p.y) &&
            p.x > -200.f && p.y > -200.f &&
            p.x < Gui.Window.Size.x + 200.f && p.y < Gui.Window.Size.y + 200.f;
    }

    inline Box2D GetBox(const CEntity& entity)
    {
        Vec2 feet{};
        Vec2 head{};
        Vec3 headWorld = entity.Pawn.Pos + Vec3(0.f, 0.f, 72.f);

        if (entity.Pawn.BoneData.BonePosList.size() > BONEINDEX::head)
            headWorld = entity.Pawn.BoneData.BonePosList[BONEINDEX::head].Pos + Vec3(0.f, 0.f, 8.f);

        if (!gGame.View.WorldToScreen(entity.Pawn.Pos, feet) || !gGame.View.WorldToScreen(headWorld, head))
            return {};
        if (!ValidScreenPoint(feet) || !ValidScreenPoint(head))
            return {};

        const float height = feet.y - head.y;
        if (!std::isfinite(height) || height < 12.f || height > Gui.Window.Size.y * 1.3f)
            return {};
        const float width = height * 0.46f;
        return { Vec2(head.x - width * 0.5f, head.y), Vec2(width, height), true };
    }

    // =========================================================================
    // Draw Bone with Visibility-Aware Coloring
    // =========================================================================
    // Uses actual visibility state from VisibilityCheck, not WorldToScreen success.
    // Colors update immediately when entity moves behind/out from behind obstacle.
    // =========================================================================
    inline void DrawBone(const CEntity& entity, const CEntity& local, bool isVisible)
    {
        const auto& bones = entity.Pawn.BoneData.BonePosList;
        if (bones.size() <= BONEINDEX::ankle_R)
            return;

        // Select color based on actual visibility state, not projection
        ImColor boneColor = isVisible ? MenuConfig::BoneColor : MenuConfig::OccludedBoneColor;

        for (const auto& chain : BoneJointList::List)
        {
            auto it = chain.begin();
            if (it == chain.end())
                continue;
            std::uint32_t previous = *it++;
            for (; it != chain.end(); ++it)
            {
                const std::uint32_t current = *it;
                if (previous < bones.size() && current < bones.size() &&
                    bones[previous].IsVisible && bones[current].IsVisible &&
                    ValidScreenPoint(bones[previous].ScreenPos) && ValidScreenPoint(bones[current].ScreenPos))
                    Gui.Line(bones[previous].ScreenPos, bones[current].ScreenPos, boneColor, 1.2f);
                previous = current;
            }
        }
    }

    inline void DrawHealth(const Box2D& box, int health)
    {
        health = std::clamp(health, 0, 100);
        const float ratio = static_cast<float>(health) / 100.f;
        const float barH = box.Size.y * ratio;
        const Vec2 base(box.Pos.x - 6.f, box.Pos.y);
        Gui.RectangleFilled(base, Vec2(3.f, box.Size.y), ImColor(24, 24, 28, 220), 1.f);
        Gui.RectangleFilled(Vec2(base.x, base.y + box.Size.y - barH), Vec2(3.f, barH), ImColor(120, 220, 120, 255), 1.f);
    }

    // =========================================================================
    // Draw Player with Visibility-Aware Rendering
    // =========================================================================
    // Box and skeleton colors determined by actual visibility check.
    // Immediately responds to visibility state changes.
    // =========================================================================
    inline void DrawPlayer(const CEntity& entity, const CEntity& local)
    {
        if (!entity.IsAlive())
            return;

        const Box2D box = GetBox(entity);
        if (!box.Valid)
            return;

        // ---- CHECK VISIBILITY USING PROPER METHOD ----
        // This performs the bone-based LOS check, not WorldToScreen
        const bool isVisible = VisibilityCheck::IsTargetVisible(entity, local, false);

        // ---- SELECT COLORS BASED ON VISIBILITY STATE ----
        // Separate colors for visible vs occluded (not based on WorldToScreen)
        ImColor boxColor = isVisible ? MenuConfig::BoxColor : MenuConfig::OccludedBoxColor;
        ImColor textColor = isVisible ? MenuConfig::TextColor : MenuConfig::OccludedTextColor;

        if (MenuConfig::ShowBoxESP)
            Gui.Rectangle(box.Pos, box.Size, boxColor, 1.2f, 0.5f);
        
        if (MenuConfig::ShowBoneESP)
            DrawBone(entity, local, isVisible);
        
        if (MenuConfig::ShowHealthBar)
            DrawHealth(box, entity.Pawn.Health);

        float topOffset = 15.f;
        if (MenuConfig::ShowPlayerName)
            Gui.StrokeText(entity.Controller.PlayerName, Vec2(box.Pos.x + box.Size.x * 0.5f, box.Pos.y - topOffset), textColor, 13.f, true);

        float bottom = box.Pos.y + box.Size.y + 2.f;
        if (MenuConfig::ShowWeaponESP)
        {
            Gui.StrokeText(entity.Pawn.WeaponName, Vec2(box.Pos.x + box.Size.x * 0.5f, bottom), textColor, 12.f, true);
            bottom += 13.f;
        }
        if (MenuConfig::ShowDistance)
        {
            const float metres = local.Pawn.Pos.DistanceTo(entity.Pawn.Pos) / 100.f;
            char text[32]{};
            sprintf_s(text, "%.0fm", metres);
            Gui.StrokeText(text, Vec2(box.Pos.x + box.Size.x * 0.5f, bottom), textColor, 12.f, true);
        }
    }

    inline void DrawAimFov()
    {
        if (!MenuConfig::ShowAimFovRange || Gui.Window.Size.x <= 0.f)
            return;
        constexpr float assumedGameFov = 90.f;
        const float radius = std::tan(MenuConfig::AimFov * 0.5f * 3.14159265f / 180.f) /
            std::tan(assumedGameFov * 0.5f * 3.14159265f / 180.f) * (Gui.Window.Size.x * 0.5f);
        Gui.Circle(Vec2(Gui.Window.Size.x * 0.5f, Gui.Window.Size.y * 0.5f), std::max(radius, 2.f), MenuConfig::AimFovRangeColor, 1.f, 72);
    }
}
