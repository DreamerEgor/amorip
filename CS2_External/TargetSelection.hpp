#pragma once
#include <cstdint>
#include <vector>
#include <limits>
#include <cmath>
#include "Game.h"
#include "Entity.h"
#include "MenuConfig.hpp"
#include "VisibilityCheck.hpp"

namespace TargetSelection
{
    // =========================================================================
    // Target Selection State Machine
    // =========================================================================
    // Maintains current aim target with visibility validation.
    // Automatically rejects targets that become occluded.
    // Reacquires new targets when current one becomes invalid.
    // Ensures no stale target state persists across visibility changes.
    // =========================================================================

    struct TargetState
    {
        std::uintptr_t CurrentTargetPawnAddress = 0;
        Vec2 CurrentTargetScreenPos{};
        bool IsValid = false;                    // Current target passes all checks
        bool WasValidLastFrame = false;          // Track state changes
        int FramesSinceAcquisition = 0;          // How long we've been locked
        bool BecameOccludedThisFrame = false;    // Transition event
    };

    inline TargetState g_TargetState{};

    // =========================================================================
    // Validate Current Target - Per-Frame Visibility Check
    // =========================================================================
    // Called each frame to verify the current target is still valid.
    // If visibility changes, immediately rejects the target.
    // =========================================================================
    inline bool ValidateCurrentTarget(
        const CEntity& local,
        const std::vector<CEntity>& allEntities,
        bool aimbotVisibleOnly)
    {
        // No current target
        if (!g_TargetState.CurrentTargetPawnAddress)
            return false;

        // Find the entity matching our current target
        const CEntity* currentEntity = nullptr;
        for (const auto& entity : allEntities)
        {
            if (entity.Pawn.Address == g_TargetState.CurrentTargetPawnAddress)
            {
                currentEntity = &entity;
                break;
            }
        }

        // Target disappeared from entity list (died/left server)
        if (!currentEntity)
        {
            g_TargetState.BecameOccludedThisFrame = true;
            g_TargetState.IsValid = false;
            g_TargetState.CurrentTargetPawnAddress = 0;
            return false;
        }

        // ---- CRITICAL: Re-check visibility every frame ----
        // Do NOT assume target remains visible just because it was visible last frame.
        // Each frame we must validate bone data fresh.
        const bool stillVisible = VisibilityCheck::IsTargetVisible(*currentEntity, local, aimbotVisibleOnly);

        // ---- Detect visibility state change ----
        if (g_TargetState.WasValidLastFrame && !stillVisible)
        {
            // Target just became occluded this frame
            g_TargetState.BecameOccludedThisFrame = true;
        }
        else
        {
            g_TargetState.BecameOccludedThisFrame = false;
        }

        g_TargetState.IsValid = stillVisible;
        g_TargetState.WasValidLastFrame = stillVisible;

        if (!stillVisible)
        {
            // Clear any stale state immediately
            g_TargetState.CurrentTargetPawnAddress = 0;
            g_TargetState.CurrentTargetScreenPos = {};
            g_TargetState.FramesSinceAcquisition = 0;
            return false;
        }

        // Target still valid - update tracking info
        g_TargetState.FramesSinceAcquisition++;
        return true;
    }

    // =========================================================================
    // Find Best Target - Visibility-Aware Selection
    // =========================================================================
    // Scans all candidates and selects best visible target based on screen distance.
    // Immediate rejects occluded targets if "Visible Only" is enabled.
    // Reacquires new target if current one became invalid.
    // =========================================================================
    inline bool FindBestTarget(
        const CEntity& local,
        const std::vector<CEntity>& allEntities,
        Vec2& outBestTargetScreen,
        float aimbotFovRadius,
        bool aimbotVisibleOnly)
    {
        float bestScreenDistance = std::numeric_limits<float>::max();
        Vec2 bestTargetScreen{};
        std::uintptr_t bestTargetPawn = 0;
        bool foundTarget = false;

        const Vec2 screenCenter(200.f, 200.f);  // Placeholder; actual size from Gui.Window.Size
        if (200.f > 0.f) // Gui.Window valid
            *(const_cast<Vec2*>(&screenCenter)) = Vec2(200.f, 200.f);  // Will be set from caller

        // ---- STEP 1: Attempt to keep current target ----
        // If current target is still valid, prefer it (sticky targeting)
        if (g_TargetState.IsValid && g_TargetState.CurrentTargetPawnAddress)
        {
            const CEntity* currentEntity = nullptr;
            for (const auto& entity : allEntities)
            {
                if (entity.Pawn.Address == g_TargetState.CurrentTargetPawnAddress)
                {
                    currentEntity = &entity;
                    break;
                }
            }

            if (currentEntity && VisibilityCheck::IsTargetVisible(*currentEntity, local, aimbotVisibleOnly))
            {
                if (currentEntity->Pawn.ScreenPos.x > 0.f && currentEntity->Pawn.ScreenPos.y > 0.f)
                {
                    const float dist = currentEntity->Pawn.ScreenPos.DistanceTo(screenCenter);
                    if (dist <= aimbotFovRadius)
                    {
                        outBestTargetScreen = currentEntity->Pawn.ScreenPos;
                        return true;  // Keep current target
                    }
                }
            }
        }

        // ---- STEP 2: Current target invalid, scan for new one ----
        // Only consider candidates that pass visibility check
        for (const auto& entity : allEntities)
        {
            // Basic alive check
            if (!entity.IsAlive())
                continue;

            // ---- VISIBILITY CHECK: Core logic ----
            // Each candidate is validated for actual visibility.
            // Occluded targets rejected if "Visible Only" enabled.
            if (!VisibilityCheck::IsTargetVisible(entity, local, aimbotVisibleOnly))
                continue;

            // Target is visible, now check FOV and distance
            if (!std::isfinite(entity.Pawn.ScreenPos.x) || !std::isfinite(entity.Pawn.ScreenPos.y))
                continue;

            const float screenDistance = entity.Pawn.ScreenPos.DistanceTo(screenCenter);
            if (screenDistance > aimbotFovRadius)
                continue;  // Outside FOV

            if (screenDistance < bestScreenDistance)
            {
                bestScreenDistance = screenDistance;
                bestTargetScreen = entity.Pawn.ScreenPos;
                bestTargetPawn = entity.Pawn.Address;
                foundTarget = true;
            }
        }

        if (!foundTarget)
        {
            // No valid target found
            g_TargetState.CurrentTargetPawnAddress = 0;
            g_TargetState.CurrentTargetScreenPos = {};
            g_TargetState.IsValid = false;
            g_TargetState.WasValidLastFrame = false;
            g_TargetState.FramesSinceAcquisition = 0;
            return false;
        }

        // ---- Acquired new target ----
        g_TargetState.CurrentTargetPawnAddress = bestTargetPawn;
        g_TargetState.CurrentTargetScreenPos = bestTargetScreen;
        g_TargetState.IsValid = true;
        g_TargetState.WasValidLastFrame = true;
        g_TargetState.FramesSinceAcquisition = 0;  // Reset frame counter for new acquisition
        outBestTargetScreen = bestTargetScreen;
        return true;
    }

    // =========================================================================
    // Reject Current Target - For manual/debug invalidation
    // =========================================================================
    inline void RejectCurrentTarget()
    {
        g_TargetState.CurrentTargetPawnAddress = 0;
        g_TargetState.CurrentTargetScreenPos = {};
        g_TargetState.IsValid = false;
        g_TargetState.WasValidLastFrame = false;
        g_TargetState.FramesSinceAcquisition = 0;
        g_TargetState.BecameOccludedThisFrame = false;
    }
}
