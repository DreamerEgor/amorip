#pragma once
#include <cstdint>
#include <string>
#include <cstdio>
#include "Game.h"
#include "Entity.h"
#include "Bone.h"

namespace VisibilityDebug
{
    // =========================================================================
    // Visibility Detection Debug Logging
    // =========================================================================
    // Tracks why each target candidate is accepted or rejected based on
    // visibility state. Helps identify stale visibility assumptions.
    // =========================================================================

    struct VisibilityCheckLog
    {
        std::string PlayerName;
        std::uintptr_t PawnAddress;
        bool WorldToScreenOk = false;
        bool BoneDataValid = false;
        bool BonesVisible = false;  // At least one head/neck/chest bone visible
        bool SpottedByMask = false; // Radar state (irrelevant to visibility)
        std::int64_t SpottedByMaskValue = 0;
        bool IsOccluded = false;    // Key: Actually occluded or visible
        bool VisibleOnlyMode = false;
        bool FinalResult = false;   // Accept/Reject
        const char* RejectionReason = nullptr;
    };

    inline VisibilityCheckLog g_LastVisibilityLog{};

    inline std::string DebugOutput()
    {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer),
            "Visibility: %s (pawn=0x%p) | W2S=%s | Bones=%s | BoneVis=%s | Spotted=%s (0x%llx) | "
            "Occluded=%s | VOnlyMode=%s | RESULT=%s%s%s",
            g_LastVisibilityLog.PlayerName.c_str(),
            reinterpret_cast<void*>(g_LastVisibilityLog.PawnAddress),
            g_LastVisibilityLog.WorldToScreenOk ? "YES" : "NO",
            g_LastVisibilityLog.BoneDataValid ? "YES" : "NO",
            g_LastVisibilityLog.BonesVisible ? "YES" : "NO",
            g_LastVisibilityLog.SpottedByMask ? "YES" : "NO",
            g_LastVisibilityLog.SpottedByMaskValue,
            g_LastVisibilityLog.IsOccluded ? "YES" : "NO",
            g_LastVisibilityLog.VisibleOnlyMode ? "YES" : "NO",
            g_LastVisibilityLog.FinalResult ? "ACCEPT" : "REJECT",
            g_LastVisibilityLog.RejectionReason ? " (" : "",
            g_LastVisibilityLog.RejectionReason ? g_LastVisibilityLog.RejectionReason : ""
        );
        // Safely append reason if present
        std::string result = buffer;
        if (g_LastVisibilityLog.RejectionReason)
            result += ")";
        return result;
    }
}

namespace VisibilityCheck
{
    // =========================================================================
    // Visibility Detection - Source 2 Bone-Based Method
    // =========================================================================
    // A target is truly visible if:
    // 1. Bone data is loaded and valid
    // 2. At least one critical bone (head/neck/chest) is marked IsVisible
    //    (This indicates line-of-sight from camera to that bone)
    //
    // NOT visibility indicators (DO NOT use these):
    // - WorldToScreen success (only means bone is in view frustum)
    // - SpottedByMask/radar state (only means player was seen recently)
    // - Assumed visibility from previous frames (stale state)
    //
    // Key insight: Source 2 sets BoneJointPos.IsVisible per-frame based on
    // actual LOS checks during bone transform. Using this is more reliable
    // than trying to replicate Source 2's occlusion checks externally.
    // =========================================================================

    // Critical bones for visibility detection: head, neck, chest
    constexpr std::uint32_t CRITICAL_BONES[] = {
        BONEINDEX::head,        // 7
        BONEINDEX::neck_0,      // 6
        BONEINDEX::spine_2      // 4 (upper chest)
    };
    constexpr std::size_t CRITICAL_BONE_COUNT = 3;

    inline bool IsTargetVisible(const CEntity& entity, const CEntity& local, bool aimbotVisibleOnlyMode = false)
    {
        // ---- STEP 1: Validate bone data exists ----
        const auto& bones = entity.Pawn.BoneData.BonePosList;
        if (bones.empty() || bones.size() <= BONEINDEX::spine_2)
        {
            VisibilityDebug::g_LastVisibilityLog.BoneDataValid = false;
            VisibilityDebug::g_LastVisibilityLog.RejectionReason = "Bone data unavailable";
            VisibilityDebug::g_LastVisibilityLog.FinalResult = false;
            return false;
        }
        VisibilityDebug::g_LastVisibilityLog.BoneDataValid = true;
        VisibilityDebug::g_LastVisibilityLog.PawnAddress = entity.Pawn.Address;
        VisibilityDebug::g_LastVisibilityLog.PlayerName = entity.Controller.PlayerName;
        VisibilityDebug::g_LastVisibilityLog.VisibleOnlyMode = aimbotVisibleOnlyMode;

        // ---- STEP 2: Check if any critical bone is visible ----
        // Source 2 sets BoneJointPos.IsVisible based on actual LOS checks
        // during the last bone transform. This is per-frame and accurate.
        bool anyCriticalBoneVisible = false;
        for (std::size_t i = 0; i < CRITICAL_BONE_COUNT; ++i)
        {
            const std::uint32_t boneIndex = CRITICAL_BONES[i];
            if (boneIndex < bones.size() && bones[boneIndex].IsVisible)
            {
                anyCriticalBoneVisible = true;
                break;  // Found at least one visible critical bone
            }
        }

        VisibilityDebug::g_LastVisibilityLog.BonesVisible = anyCriticalBoneVisible;

        // ---- STEP 3: Determine occlusion state ----
        // If no critical bones are visible, target is occluded
        const bool isOccluded = !anyCriticalBoneVisible;
        VisibilityDebug::g_LastVisibilityLog.IsOccluded = isOccluded;

        // ---- STEP 4: Verify WorldToScreen (projection check) ----
        // This is a sanity check, NOT a visibility indicator.
        // WorldToScreen just means the bone projects into screen space.
        Vec2 screenPos{};
        if (!gGame.View.WorldToScreen(entity.Pawn.Pos, screenPos))
        {
            VisibilityDebug::g_LastVisibilityLog.WorldToScreenOk = false;
            VisibilityDebug::g_LastVisibilityLog.RejectionReason = "Position not in view frustum";
            VisibilityDebug::g_LastVisibilityLog.FinalResult = false;
            return false;  // Position isn't even in screen space
        }
        VisibilityDebug::g_LastVisibilityLog.WorldToScreenOk = true;

        // ---- STEP 5: Check SpottedByMask (informational only) ----
        // SpottedByMask just means the player was seen by your team recently.
        // It is NOT proof of current visibility. A player can be spotted on radar
        // but currently behind a wall. Do NOT use this as visibility check.
        VisibilityDebug::g_LastVisibilityLog.SpottedByMask = (entity.Pawn.SpottedByMask != 0);
        VisibilityDebug::g_LastVisibilityLog.SpottedByMaskValue = entity.Pawn.SpottedByMask;

        // ---- STEP 6: Make visibility decision ----
        // Visibility is ONLY determined by critical bone LOS state.
        // This is checked fresh every frame, no stale state.
        if (isOccluded)
        {
            // Target is currently behind cover/walls
            VisibilityDebug::g_LastVisibilityLog.RejectionReason = "All critical bones occluded";
            VisibilityDebug::g_LastVisibilityLog.FinalResult = false;
            return false;
        }

        // ---- STEP 7: Apply aiming filter if "Visible Only" is enabled ----
        // If user wants to only aim at visible targets, reject occluded ones
        if (aimbotVisibleOnlyMode && isOccluded)
        {
            VisibilityDebug::g_LastVisibilityLog.RejectionReason = "Occluded (Visible Only mode)";
            VisibilityDebug::g_LastVisibilityLog.FinalResult = false;
            return false;
        }

        // ---- ALL CHECKS PASSED ----
        VisibilityDebug::g_LastVisibilityLog.FinalResult = true;
        VisibilityDebug::g_LastVisibilityLog.RejectionReason = nullptr;
        return true;
    }
}
