#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include "Game.h"
#include "Entity.h"

namespace SpectatorDebug
{
    // =========================================================================
    // Spectator Detection Debug Logging
    // =========================================================================
    // Tracks validation steps and rejection reasons for spectator detection.
    // This helps identify false positives and missing spectators.
    // =========================================================================

    struct ValidationStep
    {
        const char* StepName;
        bool Passed;
        const char* Reason;  // Why it passed/failed
    };

    struct SpectatorValidationLog
    {
        std::string PlayerName;
        std::uintptr_t ControllerAddress;
        int ValidationStepCount = 0;
        ValidationStep ValidationSteps[8];  // Max 8 validation steps
        bool FinalResult = false;
    };

    inline SpectatorValidationLog g_LastSpectatorLog{};

    inline void LogValidationStep(const char* stepName, bool passed, const char* reason)
    {
        if (g_LastSpectatorLog.ValidationStepCount >= 8)
            return;
        g_LastSpectatorLog.ValidationSteps[g_LastSpectatorLog.ValidationStepCount] = 
            { stepName, passed, reason };
        g_LastSpectatorLog.ValidationStepCount++;
    }

    inline void ClearLog()
    {
        g_LastSpectatorLog.ValidationStepCount = 0;
        g_LastSpectatorLog.FinalResult = false;
        g_LastSpectatorLog.PlayerName.clear();
        g_LastSpectatorLog.ControllerAddress = 0;
    }

    inline std::string DebugOutput()
    {
        char buffer[1024];
        std::snprintf(buffer, sizeof(buffer),
            "Spectator: %s (ctrl=0x%p) | Result=%s | Steps=%d",
            g_LastSpectatorLog.PlayerName.c_str(),
            reinterpret_cast<void*>(g_LastSpectatorLog.ControllerAddress),
            g_LastSpectatorLog.FinalResult ? "VALID" : "REJECTED",
            g_LastSpectatorLog.ValidationStepCount
        );
        return std::string(buffer);
    }
}

namespace SpectatorList
{
    // =========================================================================
    // Spectator Detection and Validation
    // =========================================================================
    // Validates that a player is truly spectating (not alive/spawned).
    // Performs these checks:
    // 1. Controller exists and is readable
    // 2. Controller's normal pawn is NOT alive (health <= 0 or > 100)
    // 3. Controller is in spectator/observer state
    // 4. Observer pawn handle is valid (not null/0xFFFFFFFF)
    // 5. Observer services exist and contain valid data
    // 6. Observer target matches the target player pawn address
    // 7. Observer mode indicates active spectating (not free-roam)
    // =========================================================================

    inline bool IsPlayerSpectating(
        std::uintptr_t controllerAddress,
        std::uintptr_t targetPawnAddress,
        std::string* debugName = nullptr)
    {
        SpectatorDebug::ClearLog();
        SpectatorDebug::g_LastSpectatorLog.ControllerAddress = controllerAddress;

        // ---- STEP 1: Controller Address Valid ----
        if (!controllerAddress)
        {
            SpectatorDebug::LogValidationStep("Controller Valid", false, "Null controller address");
            return false;
        }
        SpectatorDebug::LogValidationStep("Controller Valid", true, "Address is non-zero");

        // ---- STEP 2: Read Controller Pawn Handle ----
        if (!Offset::Entity.PlayerPawn)
        {
            SpectatorDebug::LogValidationStep("Read Pawn Handle", false, "Pawn offset not available");
            return false;
        }

        std::uint32_t normalPawnHandle = 0;
        if (!ProcessMgr.ReadMemory(controllerAddress + Offset::Entity.PlayerPawn, normalPawnHandle))
        {
            SpectatorDebug::LogValidationStep("Read Pawn Handle", false, "Failed to read pawn handle");
            return false;
        }
        SpectatorDebug::LogValidationStep("Read Pawn Handle", true, "Read successful");

        // ---- STEP 3: Verify Normal Pawn is NOT Alive ----
        // A spectator's normal game pawn should have health <= 0 (dead).
        // If their normal pawn is alive, they're an active player, not a spectator.
        if (normalPawnHandle && normalPawnHandle != 0xFFFFFFFFu)
        {
            const std::uintptr_t normalPawn = gGame.ResolveEntity(normalPawnHandle);
            if (normalPawn)
            {
                int health = 0;
                if (ProcessMgr.ReadMemory(normalPawn + Offset::Entity.Health, health))
                {
                    if (health > 0 && health <= 100)
                    {
                        char reason[64];
                        std::snprintf(reason, sizeof(reason), 
                            "Normal pawn is alive (health=%d)", health);
                        SpectatorDebug::LogValidationStep("Normal Pawn Dead", false, reason);
                        return false;
                    }
                }
            }
        }
        SpectatorDebug::LogValidationStep("Normal Pawn Dead", true, "Normal pawn not alive or unresolved");

        // ---- STEP 4: Read Player Name ----
        if (Offset::Entity.iszPlayerName && debugName)
        {
            char name[128]{};;
            if (ProcessMgr.ReadBuffer(controllerAddress + Offset::Entity.iszPlayerName, name, sizeof(name) - 1))
            {
                name[sizeof(name) - 1] = '\0';
                *debugName = name;
                if (debugName->empty())
                    *debugName = "(unnamed)";
                SpectatorDebug::g_LastSpectatorLog.PlayerName = *debugName;
            }
        }
        SpectatorDebug::LogValidationStep("Read Name", true, "Name read successful");

        // ---- STEP 5: Read Observer Pawn Handle ----
        if (!Offset::Entity.ObserverPawn)
        {
            SpectatorDebug::LogValidationStep("Observer Pawn Handle", false, "Observer pawn offset not available");
            return false;
        }

        std::uint32_t observerPawnHandle = 0;
        if (!ProcessMgr.ReadMemory(controllerAddress + Offset::Entity.ObserverPawn, observerPawnHandle))
        {
            SpectatorDebug::LogValidationStep("Observer Pawn Handle", false, "Failed to read observer pawn handle");
            return false;
        }

        // Check for null/invalid handle
        if (!observerPawnHandle || observerPawnHandle == 0xFFFFFFFFu)
        {
            SpectatorDebug::LogValidationStep("Observer Pawn Handle", false, "Handle is null or invalid");
            return false;
        }
        SpectatorDebug::LogValidationStep("Observer Pawn Handle", true, "Handle is valid");

        // ---- STEP 6: Resolve Observer Pawn ----
        const std::uintptr_t observerPawn = gGame.ResolveEntity(observerPawnHandle);
        if (!observerPawn)
        {
            SpectatorDebug::LogValidationStep("Resolve Observer Pawn", false, "Failed to resolve pawn address");
            return false;
        }
        SpectatorDebug::LogValidationStep("Resolve Observer Pawn", true, "Pawn resolved");

        // ---- STEP 7: Read Observer Services ----
        if (!Offset::Pawn.pObserverServices)
        {
            SpectatorDebug::LogValidationStep("Observer Services", false, "Observer services offset not available");
            return false;
        }

        std::uintptr_t observerServices = 0;
        if (!ProcessMgr.ReadMemory(observerPawn + Offset::Pawn.pObserverServices, observerServices) || !observerServices)
        {
            SpectatorDebug::LogValidationStep("Observer Services", false, "Failed to read or null services pointer");
            return false;
        }
        SpectatorDebug::LogValidationStep("Observer Services", true, "Services pointer valid");

        // ---- STEP 8: Check Observer Mode ----
        // Mode 0 = not observing, Mode 1+ = actively observing
        if (Offset::Observer.iObserverMode)
        {
            std::uint8_t mode = 0;
            if (ProcessMgr.ReadMemory(observerServices + Offset::Observer.iObserverMode, mode) && mode == 0)
            {
                SpectatorDebug::LogValidationStep("Observer Mode", false, "Observer mode is 0 (not spectating)");
                return false;
            }
        }
        SpectatorDebug::LogValidationStep("Observer Mode", true, "Observer mode indicates active spectating");

        // ---- STEP 9: Read Observer Target ----
        if (!Offset::Observer.hObserverTarget)
        {
            SpectatorDebug::LogValidationStep("Observer Target", false, "Observer target offset not available");
            return false;
        }

        std::uint32_t targetHandle = 0;
        if (!ProcessMgr.ReadMemory(observerServices + Offset::Observer.hObserverTarget, targetHandle))
        {
            SpectatorDebug::LogValidationStep("Observer Target", false, "Failed to read target handle");
            return false;
        }

        // Check for null/invalid handle
        if (!targetHandle || targetHandle == 0xFFFFFFFFu)
        {
            SpectatorDebug::LogValidationStep("Observer Target", false, "Target handle is null or invalid");
            return false;
        }
        SpectatorDebug::LogValidationStep("Observer Target", true, "Target handle is valid");

        // ---- STEP 10: Resolve Observer Target and Verify It Matches ----
        const std::uintptr_t resolvedTarget = gGame.ResolveEntity(targetHandle);
        if (resolvedTarget != targetPawnAddress)
        {
            char reason[128];
            std::snprintf(reason, sizeof(reason),
                "Target mismatch: observer watching 0x%p, not 0x%p",
                reinterpret_cast<void*>(resolvedTarget),
                reinterpret_cast<void*>(targetPawnAddress)
            );
            SpectatorDebug::LogValidationStep("Observer Target Match", false, reason);
            return false;
        }
        SpectatorDebug::LogValidationStep("Observer Target Match", true, "Observer is watching the target player");

        // ---- ALL CHECKS PASSED ----
        SpectatorDebug::g_LastSpectatorLog.FinalResult = true;
        return true;
    }

    inline void UpdateSpectatorList(
        std::vector<std::string>& outSpectators,
        std::uintptr_t targetPawnAddress,
        int maxPlayers = 64)
    {
        // =========================================================================
        // Update Spectator List - Runs regardless of local player state
        // =========================================================================
        // Separate from other gameplay systems so spectator tracking continues
        // even when the local player is dead, in menus, etc.
        // Performs comprehensive validation for each potential spectator.
        // =========================================================================

        outSpectators.clear();
        if (!targetPawnAddress)
            return;

        for (int i = 1; i <= std::clamp(maxPlayers, 1, 64); ++i)
        {
            const std::uintptr_t controllerAddress = gGame.ResolveEntity(static_cast<std::uint32_t>(i));
            if (!controllerAddress)
                continue;

            std::string spectatorName;
            if (IsPlayerSpectating(controllerAddress, targetPawnAddress, &spectatorName))
            {
                outSpectators.push_back(std::move(spectatorName));
            }
        }

        // Sort spectator list alphabetically for consistent display
        std::sort(outSpectators.begin(), outSpectators.end());
    }
}
